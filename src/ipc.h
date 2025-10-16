/* ipc.h - RTOS-friendly inter-thread/ISR communication helpers */
#pragma once
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call once early in main(), before starting threads or enabling UART RX */
void ipc_init(void);

/* ---------------- RX byte stream (ISR -> parser thread) ---------------- */

/* ISR/non-ISR: enqueue one received byte (non-blocking). Returns true if queued. */
bool ipc_rx_put(uint8_t c);

/* Parser thread: dequeue one byte (blocking with timeout). Returns true on success. */
bool ipc_rx_get(uint8_t *c, k_timeout_t to);

/* Optional: clear any pending RX bytes (e.g., right before TX to mute self-echo). */
void ipc_rx_purge(void);

/* ---------------- Command queue (parser -> TX worker) ------------------ */

/* Parser thread: enqueue one command byte (non-blocking). */
bool ipc_cmd_put(uint8_t cmd);

/* TX worker: dequeue one command (blocking with timeout). */
bool ipc_cmd_get(uint8_t *cmd, k_timeout_t to);

/* Optional: clear any pending commands. */
void ipc_cmd_purge(void);

/* ---------------- TX completion signaling (ISR -> worker) -------------- */

/* ISR/non-ISR: signal that UART TX finished or was aborted. */
void ipc_tx_done_give_from_isr(void);

/* Worker: wait for TX completion; returns true if taken before timeout. */
bool ipc_tx_done_take(k_timeout_t to);

/* ---------------- Soft half-duplex flag (worker <-> ISR) --------------- */

/* Set/clear the “TX active” window; used to drop self-echo in ISR. */
void ipc_set_tx_active(bool on);

/* Query current state (true while TX window is open). */
bool ipc_is_tx_active(void);

#ifdef __cplusplus
}
#endif
