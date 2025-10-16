/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ble_service.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(ab_ble, LOG_LEVEL_INF);

/* Queue of inbound BLE writes → consumed by thread C */
#ifndef BLE_RX_Q_DEPTH
#define BLE_RX_Q_DEPTH 8
#endif
K_MSGQ_DEFINE(ble_rx_q, sizeof(struct ble_msg), BLE_RX_Q_DEPTH, 4);

/* 128-bit UUIDs for service and RX characteristic */
#define BT_UUID_ASYNC_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x23d1bcea, 0x5f78, 0x2315, 0xdeef, 0x1212deadbeefULL)
#define BT_UUID_ASYNC_RX_CHAR_VAL \
    BT_UUID_128_ENCODE(0x23d1bcea, 0x5f78, 0x2315, 0xdeef, 0x1212deadbe01ULL)

static struct bt_uuid_128 BT_UUID_ASYNC_SERVICE =
    BT_UUID_INIT_128(BT_UUID_ASYNC_SERVICE_VAL);
static struct bt_uuid_128 BT_UUID_ASYNC_RX_CHAR =
    BT_UUID_INIT_128(BT_UUID_ASYNC_RX_CHAR_VAL);

/* Write handler: forward payload to msgq */
#include <string.h>  /* for memcpy */

static ssize_t rx_char_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    ARG_UNUSED(conn); ARG_UNUSED(attr); ARG_UNUSED(flags);

    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if (len != 4) { /* or BLE_RX_MAX if you #define it to 4 */
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    struct ble_msg m = { .len = 4 };
    memcpy(m.data, buf, 4);
    (void)k_msgq_put(&ble_rx_q, &m, K_NO_WAIT);
    return len;
}

/* Primary service with a single writeable characteristic */
BT_GATT_SERVICE_DEFINE(async_svc,
    BT_GATT_PRIMARY_SERVICE(&BT_UUID_ASYNC_SERVICE.uuid),
    BT_GATT_CHARACTERISTIC(&BT_UUID_ASYNC_RX_CHAR.uuid,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_WRITE,
        NULL /* read */, rx_char_write /* write */,
        NULL /* user data */)
);

/* Simple connection logs (optional) */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("BT connected failed (err %u)", err);
    } else {
        LOG_INF("BT connected");
    }
}
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("BT disconnected (reason 0x%02x)", reason);
}
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* Advertising payload: flags + 128-bit service UUID + (optional) name */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_ASYNC_SERVICE_VAL),
};
static const struct bt_data sd[] = {
#ifdef CONFIG_BT_DEVICE_NAME
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
#endif
};

int ble_service_init(void)
{
    int rc = bt_enable(NULL);
    if (rc) {
        LOG_ERR("bt_enable failed (%d)", rc);
        return rc;
    }
    LOG_INF("Bluetooth initialized");
    return 0;
}

int ble_service_start(void)
{
    /* Legacy, connectable, fast interval */
    const struct bt_le_adv_param adv_param = {
        .options     = BT_LE_ADV_OPT_CONNECTABLE,   /* deprecated tag in your rev, but valid */
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .peer        = NULL,
    };

    int rc = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (rc) {
        LOG_ERR("Advertising start failed (%d)", rc);
        return rc;
    }
    LOG_INF("Advertising (connectable) started");
    return 0;
}

/* Consumer API */
bool ble_rx_get(struct ble_msg *out, k_timeout_t to)
{
    return k_msgq_get(&ble_rx_q, out, to) == 0;
}
void ble_rx_purge(void)
{
    k_msgq_purge(&ble_rx_q);
}
