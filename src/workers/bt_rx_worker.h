/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <zephyr/kernel.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start the BLE RX consumer thread ("thread C"). */
void bt_rx_worker_start(k_thread_stack_t *stack, size_t stack_size, int priority);

#ifdef __cplusplus
}
#endif
