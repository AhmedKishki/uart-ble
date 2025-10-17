#pragma once
#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>

/* ------------ RX message (event + data byte) ------------ */
enum {
    IPC_RX_EVT_BYTE  = 0,  /* 'data' holds a received byte */
    IPC_RX_EVT_BREAK = 1,
};

struct ipc_rx_msg {
    uint8_t evt;
    uint8_t data;
};

/* Init */
void ipc_init(void);

/* ---- RX queue (pair API) ---- */
bool ipc_rx_put_pair(uint8_t evt, uint8_t data);             /* ISR-safe */
bool ipc_rx_get_pair(struct ipc_rx_msg *m, k_timeout_t to);  /* thread */

/* ---- Command queue ---- */
bool ipc_cmd_put(uint8_t cmd);
bool ipc_cmd_get(uint8_t *cmd, k_timeout_t to);
void ipc_cmd_purge(void);

/* ---- RX purge ---- */
void ipc_rx_purge(void);

/* ---- TX completion ---- */
void ipc_tx_done_give_from_isr(void);
bool ipc_tx_done_take(k_timeout_t to);

/* ---- Soft half-duplex flag ---- */
void ipc_set_tx_active(bool on);
bool ipc_is_tx_active(void);

/* Tunables */
#ifndef IPC_RX_Q_LEN
#define IPC_RX_Q_LEN   32   /* number of (evt,data) messages */
#endif

#ifndef IPC_CMD_Q_LEN
#define IPC_CMD_Q_LEN   8   /* commands */
#endif
