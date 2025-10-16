/* serial_io.c - UART async I/O (ISR + thin TX start) */

#include "serial_io.h"
#include "ipc.h"                 /* our ISR <-> threads glue */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

/* ------------------- Build-time knobs (override in config.h) ------------------- */
#include "config.h"

/* Fallbacks if config.h didn’t provide them */
#ifndef AB_UART_NODE
#define AB_UART_NODE            DT_CHOSEN(zephyr_shell_uart)
#endif
#ifndef AB_RX_CHUNK_LEN
#define AB_RX_CHUNK_LEN         64
#endif
#ifndef AB_RX_IDLE_TIMEOUT_US
#define AB_RX_IDLE_TIMEOUT_US   20000  /* 20 ms */
#endif

LOG_MODULE_REGISTER(ab_serial, LOG_LEVEL_INF);

/* The UART device (picked at build-time) */
static const struct device *const uart_dev = DEVICE_DT_GET(AB_UART_NODE);

/* Double-buffered RX storage and index the driver will request next */
static uint8_t rx_buf[2][AB_RX_CHUNK_LEN];
static volatile uint8_t rx_idx; /* toggled 0<->1 when handing out buffers */

/* ------------------------------ ISR callback ------------------------------ */
static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    switch (evt->type) {
    case UART_RX_BUF_REQUEST: {
        /* Driver wants the next buffer to keep streaming without gaps */
        uint8_t i = rx_idx;
        int rc = uart_rx_buf_rsp(uart_dev, rx_buf[i], sizeof(rx_buf[0]));
        if (rc == 0) {
            rx_idx = i ^ 1; /* toggle to other buffer */
        } else {
            /* If this fails, RX may stall until next request; nothing else to do in ISR */
            LOG_DBG("uart_rx_buf_rsp() failed: %d", rc);
        }
        break;
    }

    case UART_RX_RDY: {
        /* A slice of bytes became ready (due to idle timeout or buffer getting filled) */
        const uint8_t *p = evt->data.rx.buf + evt->data.rx.offset;
        size_t n = evt->data.rx.len;

        /* Soft half-duplex: drop anything while TX window is open */
        if (ipc_is_tx_active()) {
            /* We intentionally do not log at INFO here to avoid log floods */
            break;
        }

        for (size_t i = 0; i < n; i++) {
            (void)ipc_rx_put(p[i]); /* non-blocking; OK to drop if queue is full */
        }
        break;
    }

    case UART_TX_DONE:
        /* Wake the worker that’s waiting for completion */
        ipc_tx_done_give_from_isr();
        break;

    case UART_TX_ABORTED:
        /* Also wake the worker so it can recover/retry */
        ipc_tx_done_give_from_isr();
        break;

    /* Unused events in this app; keep ISR short */
    case UART_RX_STOPPED:
    case UART_RX_DISABLED:
    default:
        break;
    }
}

/* ------------------------------ Public API ------------------------------ */

int serial_io_init(void)
{
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }

    /* App must have called ipc_init() already (in main) */
    uart_callback_set(uart_dev, uart_cb, NULL);

    /* Hand out buffer 0 first; set “next” to 1 */
    rx_idx = 1;
    int rc = uart_rx_enable(uart_dev,
                            rx_buf[0],
                            AB_RX_CHUNK_LEN,
                            AB_RX_IDLE_TIMEOUT_US);
    if (rc) {
        LOG_ERR("uart_rx_enable failed: %d", rc);
        return rc;
    }

    LOG_INF("serial_io initialized (chunk=%d, idle=%dus)",
            AB_RX_CHUNK_LEN, AB_RX_IDLE_TIMEOUT_US);
    return 0;
}

int serial_tx_start(const uint8_t *buf, size_t len)
{
    /* Non-blocking; worker handles -EBUSY and waits on ipc semaphore */
    return uart_tx(uart_dev, buf, len, /*timeout_us=*/0);
}
