/*
 * SPDX-License-Identifier: Apache-2.0
 */ // License header: project uses Apache-2.0
#include <zephyr/kernel.h>            // Core kernel APIs: threads, semaphores, msgq, sleep
#include <zephyr/device.h>            // Device model (DEVICE_DT_GET, device_is_ready)
#include <zephyr/drivers/uart.h>      // UART driver APIs (async callbacks, uart_tx/rx)
#include <zephyr/logging/log.h>       // Logging subsystem (LOG_*)
#include <zephyr/sys/atomic.h>        // Atomic operations (atomic_t, atomic_get/set/or/and)l
#include <string.h>                   // memcpy()

LOG_MODULE_REGISTER(uart_ab_worker, LOG_LEVEL_INF); // Define a log module at INFO level

/* UART & RX config */
#define UART_NODE            DT_CHOSEN(zephyr_shell_uart) // Use the board's chosen shell/console UART
#define RX_CHUNK_LEN         64                            // Size of each RX buffer chunk
#define RX_IDLE_TIMEOUT_US   20000  /* 20 ms */            // RX idle timeout to deliver UART_RX_RDY slices

/* Commands */
#define CMD_A 'A'            // ASCII 'A' will trigger sending A's payload
#define CMD_B 'B'            // ASCII 'B' will trigger sending B's payload

static const struct device *const uart_dev = DEVICE_DT_GET(UART_NODE); // Get the UART device at build time

/* ---------- Shared 4-byte caches + ready flags (lock-free) ---------- */
typedef union { uint8_t b[4]; uint32_t u32; } four_bytes_t; // Helper to pack/unpack 4 bytes atomically

static atomic_t a_cache;      /* 4 bytes for A (packed as u32) */ // Atomic storage for latest A payload
static atomic_t b_cache;      /* 4 bytes for B (packed as u32) */ // Atomic storage for latest B payload

#define FLAG_A_READY BIT(0)   // Bit mask for "A data ready"
#define FLAG_B_READY BIT(1)   // Bit mask for "B data ready"
static atomic_t ready_flags;  // Atomic bitfield of which payloads are ready

/* Soft half-duplex: when 1, RX bytes are ignored (self-echo mute) */
static atomic_t tx_active;

/* Producers call these to update cache + set flag */
static inline void set_a(const uint8_t v[4]) {               // Producer helper: publish A bytes + mark ready
    four_bytes_t t = { .b = { v[0], v[1], v[2], v[3] } };    // Pack 4 bytes into a u32
    atomic_set(&a_cache, (atomic_val_t)t.u32);               // Atomically store new A payload
    atomic_or(&ready_flags, FLAG_A_READY);                   // Atomically set A-ready flag
}
static inline void set_b(const uint8_t v[4]) {               // Producer helper: publish B bytes + mark ready
    four_bytes_t t = { .b = { v[0], v[1], v[2], v[3] } };    // Pack 4 bytes into a u32
    atomic_set(&b_cache, (atomic_val_t)t.u32);               // Atomically store new B payload
    atomic_or(&ready_flags, FLAG_B_READY);                   // Atomically set B-ready flag
}

/* ---------- RX byte queue (ISR -> parser) ---------- */
K_MSGQ_DEFINE(rx_q, sizeof(uint8_t), 64, 4);  /* room for bursts of bytes */

/* ---------- Worker-thread signaling ---------- */
K_MSGQ_DEFINE(cmd_q, sizeof(uint8_t), 8, 4);   /* RX ISR → worker */ // Queue of 1-byte commands from ISR to worker
static struct k_sem tx_done_sem;               /* ISR gives on TX_DONE */ // Semaphore signaled when TX finishes

/* ---------- RX double buffering ---------- */
static uint8_t rx_buf[2][RX_CHUNK_LEN];  // Two ping-pong RX buffers for zero-copy async receiving
static volatile uint8_t rx_idx;          // Index of the next buffer to hand to the driver (toggled in ISR)

/* ---------- UART async callback (ISR context) ---------- */
static void uart_cb(const struct device *dev, struct uart_event *evt, void *ud) // ISR callback for UART events
{
    ARG_UNUSED(ud); // We don't use the user_data pointer

    switch (evt->type) { // Handle the specific UART event type
    case UART_RX_BUF_REQUEST: {                                  // Driver asks for the next RX buffer
        uint8_t i = rx_idx;                                      // Snapshot which buffer index to hand over
        int rc = uart_rx_buf_rsp(dev, rx_buf[i], sizeof(rx_buf[0])); // Provide buffer i to keep RX continuous
        if (rc == 0) {                                           // If handoff succeeded
        	rx_idx = i ^ 1;                                      // Toggle 0↔1 so the other buffer is next
    	}
        break;                                                   // Done handling buffer request
    }
    case UART_RX_RDY: {                                          // RX data slice is ready (idle timeout or fill)
        const uint8_t *p = evt->data.rx.buf + evt->data.rx.offset; // Pointer to received bytes within buffer
        size_t n = evt->data.rx.len;                             // Number of new bytes in this slice
        for (size_t i = 0; i < n; i++) {                         // Iterate each received byte
			if (atomic_get(&tx_active)) {
            	LOG_INF("Soft-mute active: drop self-echo / any bytes during our TX");
            	continue;
        	}
            uint8_t c = p[i];                                    // Current byte
            (void)k_msgq_put(&rx_q, &c, K_NO_WAIT);             // Enqueue command for worker (drops if full)
        }
        break;                                                   // Done with this RX_RDY slice
    }
    case UART_TX_DONE:                                           // TX finished sending the last buffer
        k_sem_give(&tx_done_sem);                                // Wake the worker waiting for completion
        break;                                                   // Return from ISR
    case UART_TX_ABORTED:                                        // TX aborted (error/cancel)
        k_sem_give(&tx_done_sem);                                // Also wake the worker so it can recover
        break;                                                   // Return from ISR
    default:                                                     // Other events not used here
        break;                                                   // Ignore to keep ISR minimal
    }
}

/* ---------- Parser thread: turns byte stream into commands ---------- */
void parser_worker(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    enum { WAIT_S, GOT_S } state = WAIT_S;
    uint8_t ch;

    for (;;) {
        k_msgq_get(&rx_q, &ch, K_FOREVER);

        switch (state) {
        case WAIT_S:
            if (ch == 'S') { state = GOT_S; }
            break;

        case GOT_S:
            if (ch == 'A' || ch == 'B') {
                /* forward the command to the TX worker */
                (void)k_msgq_put(&cmd_q, &ch, K_NO_WAIT);
            }
            /* regardless of what the second byte was, reset */
            state = WAIT_S;
            break;
        }
    }
}

/* ---------- Worker thread: does TX (not ISR) ---------- */
void tx_worker(void *a, void *b, void *c) // Worker thread that serializes TX and clears flags after send
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c); // Unused thread args

    uint8_t cmd;                   // Command byte dequeued from cmd_q
    uint8_t payload[4];            // Local buffer to hold 4-byte snapshot to send

    for (;;) {                                                         // Run forever
        /* Wait for a command from RX ISR */
        k_msgq_get(&cmd_q, &cmd, K_FOREVER);                           // Block until a command arrives

        /* Check if producer flagged data as ready */
        uint32_t flags = (uint32_t)atomic_get(&ready_flags);           // Snapshot ready flags atomically
        if (cmd == CMD_A) {                                            // If 'A' command
            if (!(flags & FLAG_A_READY)) continue;                     // Skip if no new A data is ready
            four_bytes_t t; t.u32 = (uint32_t)atomic_get(&a_cache);    // Atomically read latest A payload
            memcpy(payload, t.b, 4);                                   // Copy into local TX buffer
        } else if (cmd == CMD_B) {                                     // If 'B' command
            if (!(flags & FLAG_B_READY)) continue;                     // Skip if no new B data is ready
            four_bytes_t t; t.u32 = (uint32_t)atomic_get(&b_cache);    // Atomically read latest B payload
            memcpy(payload, t.b, 4);                                   // Copy into local TX buffer
        } else {                                                       // Unknown command
            continue;                                                  // Ignore and wait for the next
        }

		/* --- Begin soft half-duplex window --- */
		atomic_set(&tx_active, 1);
		/* Drop any bytes that slipped in just before we set the flag */
		k_msgq_purge(&rx_q);

        /* Start TX (non-blocking call), retry if UART currently busy */
        int rc;                                                        // Return code from uart_tx()
        do {
            rc = uart_tx(uart_dev, payload, sizeof(payload), 0);       // Try to start TX immediately (no timeout)
            if (rc == -EBUSY) {                                        // If UART is currently sending something
                /* Wait for previous TX to finish, then retry */
                k_sem_take(&tx_done_sem, K_FOREVER);                   // Block until ISR signals TX done/aborted
            }
        } while (rc == -EBUSY);                                        // Loop until uart_tx() is accepted or fails

        if (rc == 0) {                                                 // TX successfully started
            /* Wait for TX completion to ensure payload lives long enough */
            k_sem_take(&tx_done_sem, K_FOREVER);                       // Wait for UART_TX_DONE/ABORTED from ISR

            /* After every send, unset the flag */
            if (cmd == CMD_A) {                                        // We just sent A
                atomic_and(&ready_flags, ~FLAG_A_READY);               // Clear A-ready flag (consumed)
            } else {                                                   // We just sent B
                atomic_and(&ready_flags, ~FLAG_B_READY);               // Clear B-ready flag (consumed)
            }
        }
        /* else: error starting TX → silently drop (keeps code minimal) */ // If rc != 0 and != -EBUSY, drop it
		/* --- End soft half-duplex window --- */
		atomic_clear(&tx_active);
    }
}

/* ---------- Example producers (replace with your real ones) ---------- */
void producer_a(void *p1, void *p2, void *p3) // Example A-producer: continually publishes 'A' bytes
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3); // Unused args

    while (1) {                                      // Loop forever
        uint8_t v[4] = {                             // 4-byte payload for A (here constant 'A' = 0x41)
            (uint8_t)(0x41),
            (uint8_t)(0x41),
            (uint8_t)(0x41),
            (uint8_t)(0x41),
        };
        set_a(v);                                    // Publish A payload + set A-ready flag
        k_sleep(K_MSEC(10));                         // Sleep a bit before next publish
    }
}

void producer_b(void *p1, void *p2, void *p3) // Example B-producer: continually publishes 'B' bytes
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3); // Unused args

    while (1) {                                     // Loop forever
        uint8_t v[4] = {                            // 4-byte payload for B (here constant 'B' = 0x42)
            (uint8_t)(0x42),
            (uint8_t)(0x42),
            (uint8_t)(0x42),
            (uint8_t)(0x42),
        };
        set_b(v);                                    // Publish B payload + set B-ready flag
        k_sleep(K_MSEC(50));                         // Sleep a bit before next publish
    }
}

/* ---------- Threads ---------- */
K_THREAD_STACK_DEFINE(parser_stack, 1024);
K_THREAD_STACK_DEFINE(worker_stack, 1024);          // Stack for worker thread (TX serializer)
K_THREAD_STACK_DEFINE(prod_a_stack, 768);           // Stack for A-producer thread
K_THREAD_STACK_DEFINE(prod_b_stack, 768);           // Stack for B-producer thread
static struct k_thread parser_tcb;
static struct k_thread worker_tcb;
static struct k_thread prod_a_tcb;
static struct k_thread prod_b_tcb; // Thread control blocks (metadata)

/* ---------- Main ---------- */
int main(void) // Entry point (Zephyr apps still define main())
{
    if (!device_is_ready(uart_dev)) {               // Ensure UART device is probed and ready
        LOG_ERR("UART not ready");                  // Log error if not
        return 0;                                   // Exit main (thread ends)
    }

    /* Init flags/caches + sem */
    atomic_clear(&ready_flags);                     // Clear all ready flags at startup
	atomic_clear(&tx_active);
    atomic_set(&a_cache, 0);                        // Initialize A cache to 0
    atomic_set(&b_cache, 0);                        // Initialize B cache to 0
    k_sem_init(&tx_done_sem, 0, 1);                 // Initialize TX completion semaphore (count=0, max=1)

    /* UART async: set callback and enable RX (double-buffer) */
    uart_callback_set(uart_dev, uart_cb, NULL);     // Register ISR callback for async UART events
    rx_idx = 1;                                     // Next buffer to hand out will be index 1 (we start with 0)
    int rc = uart_rx_enable(uart_dev,               // Enable async RX:
                            rx_buf[0],              //  - initial buffer is rx_buf[0]
                            RX_CHUNK_LEN,           //  - buffer size per chunk
                            RX_IDLE_TIMEOUT_US);    //  - idle timeout to deliver UART_RX_RDY events
    if (rc) {                                       // If enabling RX failed
        LOG_ERR("uart_rx_enable failed (%d)", rc);  // Log the error code
        return 0;                                   // Exit main
    }

    /* Start worker + producers */
	k_thread_create(&parser_tcb, parser_stack, K_THREAD_STACK_SIZEOF(parser_stack),
                	parser_worker, NULL, NULL, NULL,
                	K_PRIO_PREEMPT(7), 0, K_NO_WAIT); // Start parser thread (higher prio than producers)

    k_thread_create(&worker_tcb, worker_stack, K_THREAD_STACK_SIZEOF(worker_stack),
                    tx_worker, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(7), 0, K_NO_WAIT); // Start worker thread (higher prio than producers)

    k_thread_create(&prod_a_tcb, prod_a_stack, K_THREAD_STACK_SIZEOF(prod_a_stack),
                    producer_a, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(8), 0, K_NO_WAIT); // Start A-producer thread

    k_thread_create(&prod_b_tcb, prod_b_stack, K_THREAD_STACK_SIZEOF(prod_b_stack),
                    producer_b, NULL, NULL, NULL,
                    K_PRIO_PREEMPT(8), 0, K_NO_WAIT); // Start B-producer thread

    LOG_INF("Worker-thread TX: send 'S' followed b 'A' or 'B' or 'C'."); // Informative startup log

    while (1) {                                   // Keep main thread alive
        k_sleep(K_FOREVER);                       // Park forever (all work is in other threads/ISR)
    }
}
