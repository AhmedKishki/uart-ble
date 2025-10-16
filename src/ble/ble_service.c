/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble/ble_service.h"
#include "payload.h"  /* set_C(), etc. */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#if defined(CONFIG_SETTINGS) && defined(CONFIG_BT_SETTINGS)
#include <zephyr/settings/settings.h>
#endif

LOG_MODULE_REGISTER(ble_service, LOG_LEVEL_INF);

/* Use the configured device name unless you already #define DEVICE_NAME elsewhere */
#ifndef DEVICE_NAME
#define DEVICE_NAME      CONFIG_BT_DEVICE_NAME
#endif
#ifndef DEVICE_NAME_LEN
#define DEVICE_NAME_LEN  (sizeof(DEVICE_NAME) - 1)
#endif

/* =======================
 *  GATT: Service + Char
 * ======================= */

/* Write handler: accept exactly 4 bytes and publish via set_C() */
static ssize_t write_c_payload(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr,
                               const void *buf,
                               uint16_t len,
                               uint16_t offset,
                               uint8_t flags)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(flags);

    LOG_DBG("C payload write: len=%u, offset=%u", len, offset);

    /* Require a single, atomic 4-byte write (no long/offset writes). */
    if (offset != 0) {
        LOG_DBG("C payload: non-zero offset not supported");
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if (len != 4U) {
        LOG_DBG("C payload: invalid length %u (need 4)", len);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    uint8_t tmp[4];
    memcpy(tmp, buf, sizeof(tmp));

    set_C(tmp);  /* publish to lock-free cache + mark ready */

    LOG_HEXDUMP_DBG(tmp, sizeof(tmp), "C payload");
    return len; /* success */
}

/* Primary service + a single write-only characteristic for the 4-byte C payload */
BT_GATT_SERVICE_DEFINE(my_service_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_MY_SERVICE),

    BT_GATT_CHARACTERISTIC(BT_UUID_MY_CHAR_C,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_WRITE,
        NULL,               /* read */
        write_c_payload,    /* write */
        NULL)               /* user_data */
);

/* =======================
 *  Advertising
 * ======================= */

/* 500–500.625 ms legacy, undirected, identity address, connectable */
static const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
    (BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY),
    800,   /* 800 * 0.625 ms = 500 ms */
    801,   /* 801 * 0.625 ms = 500.625 ms */
    NULL   /* undirected advertising */
);

static struct k_work adv_work;

/* Advertising data (AD) */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Scan response (SD): advertise your custom 128-bit service UUID for filtering */
static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_MY_SERVICE_VAL),
};

static void advertising_start_internal(void)
{
    int err = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err == -EALREADY) {
        LOG_DBG("Advertising already running");
        return;
    }
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return;
    }
    LOG_INF("Advertising successfully started");
}

static void adv_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    advertising_start_internal();
}

static void advertising_start(void)
{
    (void)k_work_submit(&adv_work);
}

/* =======================
 *  Connection callbacks
 * ======================= */

static void on_connected(struct bt_conn *conn, uint8_t err)
{
    ARG_UNUSED(conn);

    if (err) {
        LOG_WRN("Connection failed (err %u), restarting advertising", err);
        advertising_start();
        return;
    }
    LOG_INF("Connected");
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);

    LOG_INF("Disconnected (reason %u); restarting advertising", reason);
    advertising_start();
}

static struct bt_conn_cb connection_callbacks = {
    .connected    = on_connected,
    .disconnected = on_disconnected,
    /* If your SDK adds a '.recycled' callback, you can add it here. */
};

/* =======================
 *  Public API
 * ======================= */

/* Bring up the Bluetooth stack */
void ble_start(void)
{
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        k_panic();
    }
#if defined(CONFIG_SETTINGS) && defined(CONFIG_BT_SETTINGS)
    /* In some Zephyr/NCS versions, bt_enable() already triggers settings load.
     * This is safe to call even if already loaded.
     */
    (void)settings_load();
#endif
    LOG_INF("Bluetooth ready");
}

/* Start advertising (register callbacks on first call) */
void adv_start(void)
{
    static bool inited;

    if (!inited) {
        k_work_init(&adv_work, adv_work_handler);
        bt_conn_cb_register(&connection_callbacks);
        inited = true;
    }

    advertising_start();
    LOG_INF("Advertising kick requested");
}
