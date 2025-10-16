/* serial_io.h - UART async I/O (ISR + thin TX start) */
#pragma once

#include <zephyr/device.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize UART async I/O:
 * - sets ISR callback
 * - enables double-buffered RX
 * Returns 0 on success, <0 on error.
 */
void serial_io_init(void);

/**
 * Start a TX (non-blocking). Returns:
 *  0       : accepted; completion will be signaled via ipc_tx_done_give_from_isr()
 * -EBUSY   : UART is already transmitting
 *  <0 other: error
 */
int serial_tx_start(const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif
