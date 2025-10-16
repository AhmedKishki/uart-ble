/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One fixed-size BLE RX message (fits default ATT payload = 20 bytes) */
#ifndef BLE_RX_MAX
#define BLE_RX_MAX 20
#endif

struct ble_msg {
    uint8_t len;
    uint8_t data[BLE_RX_MAX];
};

/* Initialize BT stack, register GATT service/char */
int ble_service_init(void);

/* Start connectable advertising */
int ble_service_start(void);

/* Consumer API: fetch next BLE RX message (blocks with timeout) */
bool ble_rx_get(struct ble_msg *out, k_timeout_t to);

/* Optional: drop all pending BLE packets */
void ble_rx_purge(void);

#ifdef __cplusplus
}
#endif
