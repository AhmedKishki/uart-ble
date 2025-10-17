/* ipc.c - implementation of inter-thread/ISR communication helpers */
#include "ipc.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

/* --------- Storage: kept private to this translation unit --------- */

/* RX messages: each element is {evt, data} */
K_MSGQ_DEFINE(ipc_rx_q, sizeof(struct ipc_rx_msg), IPC_RX_Q_LEN, 4);

/* Commands from parser to TX worker */
K_MSGQ_DEFINE(ipc_cmd_q, sizeof(uint8_t), IPC_CMD_Q_LEN, 4);

/* TX completion semaphore (ISR gives, worker takes) */
static struct k_sem tx_done_sem;

/* Soft half-duplex flag (worker sets during TX; ISR checks to drop bytes) */
static atomic_t tx_active;

void ipc_init(void)
{
    k_msgq_purge(&ipc_rx_q);
    k_msgq_purge(&ipc_cmd_q);
    atomic_clear(&tx_active);
    k_sem_init(&tx_done_sem, 0, 1);
}

/* ---- RX queue (pair API) ---- */

bool ipc_rx_put_pair(uint8_t evt, uint8_t data)
{
    struct ipc_rx_msg m = { .evt = evt, .data = data };
    return k_msgq_put(&ipc_rx_q, &m, K_NO_WAIT) == 0;
}

bool ipc_rx_get_pair(struct ipc_rx_msg *m, k_timeout_t to)
{
    return k_msgq_get(&ipc_rx_q, m, to) == 0;
}

/* ---- RX purge ---- */

void ipc_rx_purge(void)
{
    k_msgq_purge(&ipc_rx_q);
}

/* ---- Command queue ---- */

bool ipc_cmd_put(uint8_t cmd)
{
    return k_msgq_put(&ipc_cmd_q, &cmd, K_NO_WAIT) == 0;
}

bool ipc_cmd_get(uint8_t *cmd, k_timeout_t to)
{
    return k_msgq_get(&ipc_cmd_q, cmd, to) == 0;
}

void ipc_cmd_purge(void)
{
    k_msgq_purge(&ipc_cmd_q);
}

/* ---- TX completion ---- */

void ipc_tx_done_give_from_isr(void)
{
    k_sem_give(&tx_done_sem);
}

bool ipc_tx_done_take(k_timeout_t to)
{
    return k_sem_take(&tx_done_sem, to) == 0;
}

/* ---- Soft half-duplex flag ---- */

void ipc_set_tx_active(bool on)
{
    if (on) {
        atomic_set(&tx_active, 1);
    } else {
        atomic_clear(&tx_active);
    }
}

bool ipc_is_tx_active(void)
{
    return atomic_get(&tx_active) != 0;
}
