/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "bt_rx_worker.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "ble/ble_service.h"   /* ble_rx_get() */

LOG_MODULE_REGISTER(ab_bt_rx, LOG_LEVEL_INF);

static struct k_thread tcb;
#define THREAD_NAME "bt_rx_worker"

static void thread_entry(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    struct ble_msg m;

    for (;;) {
        if (!ble_rx_get(&m, K_FOREVER)) {
            continue;
        }
        /* For now: just log it. Replace with your own handling. */
        LOG_INF("BLE RX: %u bytes", m.len);
        LOG_HEXDUMP_INF(m.data, m.len, "data");
    }
}

void bt_rx_worker_start(k_thread_stack_t *stack, size_t stack_size, int priority)
{
    (void)k_thread_create(&tcb, stack, stack_size,
                          thread_entry, NULL, NULL, NULL,
                          priority, 0, K_NO_WAIT);
#if defined(CONFIG_THREAD_NAME)
    k_thread_name_set(&tcb, THREAD_NAME);
#endif
}
