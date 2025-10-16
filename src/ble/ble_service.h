/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: include/ble/ble_service.h
 */
#ifndef BLE_SERVICE_H_
#define BLE_SERVICE_H_

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------- UUIDs ----------
 * Replace these placeholders with your actual UUIDs.
 * Keep the *_VAL (encoded) form for AD/SD usage and the BT_UUID_* handle
 * form for GATT declarations.
 */
#ifndef BT_UUID_MY_SERVICE_VAL
#define BT_UUID_MY_SERVICE_VAL \
    BT_UUID_128_ENCODE(0xf0debc9a, 0x7856, 0x3412, 0xba98, 0x76543210fedcULL)
#endif
#define BT_UUID_MY_SERVICE BT_UUID_DECLARE_128(BT_UUID_MY_SERVICE_VAL)

/* Optional: 4-byte “C” payload characteristic UUID (used by write_c_payload) */
#ifndef BT_UUID_MY_CHAR_C_VAL
#define BT_UUID_MY_CHAR_C_VAL \
    BT_UUID_128_ENCODE(0xc0ffee00, 0x0000, 0x0000, 0xbeef, 0x001122334455ULL)
#endif
#define BT_UUID_MY_CHAR_C BT_UUID_DECLARE_128(BT_UUID_MY_CHAR_C_VAL)

/* --------- API ----------
 * Call ble_start() once to bring up the Bluetooth stack.
 * Then call adv_start() to register callbacks and start (or restart) advertising.
 */
void ble_start(void);
void adv_start(void);

/* Back-compat alias if other code still calls this name */
static inline void ble_service_adv_start(void) { adv_start(); }

#ifdef __cplusplus
}
#endif

#endif /* BLE_SERVICE_H_ */
