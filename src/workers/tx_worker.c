/* tx_worker.c - Serial TX worker: serializes payload sends */
#include "tx_worker.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ipc.h"
#include "serial_io.h"
#include "config.h"
#include "proto/payload.h"

LOG_MODULE_REGISTER(tx, LOG_LEVEL_INF);

/* Thread control block (owned here) */
static struct k_thread tx_tcb;

/* ---------------- Internal helpers ---------------- */

static inline bool snapshot_payload(uint8_t cmd, uint8_t out[4])
{
    if (!is_ready(cmd)) {
        return false;
    }
    snapshot(cmd, out);
    return true;
}

/* ---------------- Thread entry ---------------- */

static void tx_worker_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    uint8_t cmd;
    uint8_t payload[4];

    for (;;) {

        /* Wait for a command from parser */
        if (!ipc_cmd_get(&cmd, K_FOREVER)) {
            continue; /* should not happen with FOREVER, but be defensive */
        }

        LOG_INF("tx_worker: unknown cmd 0x%02x", cmd);

        /* Only handle known commands */
        if (cmd != CMD_A && cmd != CMD_B && cmd != CMD_C) {
            LOG_DBG("tx_worker: unknown cmd 0x%02x", cmd);
            continue;
        }

        /* Take an atomic snapshot if data is ready. If not, skip. */
        if (!snapshot_payload(cmd, payload)) {
            /* No new data flagged for this command; nothing to send. */
            continue;
        }

        /* ---- Begin soft half-duplex window ---- */
        ipc_set_tx_active(true);
        /* Drop any bytes queued just before the window closed (mute self-echo). */
        ipc_rx_purge();

        /* Start TX (non-blocking). If busy, wait for completion and retry. */
        int rc;
        do 
        {
            rc = serial_tx_start(payload, sizeof(payload));
            if (rc == -EBUSY) 
            {
                (void)ipc_tx_done_take(K_FOREVER); /* wait previous TX to complete */
            }
        } 
        while (rc == -EBUSY);

        if (rc == 0) 
        {
            /* Ensure payload buffer lifetime until hardware is done */
            (void)ipc_tx_done_take(K_FOREVER);

            /* Mark this command’s data as consumed */
            clear_ready(cmd);
        }
        else 
        {
            /* Start failed; keep flag as-is so another attempt can try again */
            LOG_DBG("serial_tx_start failed: %d", rc);
        }

        /* ---- End soft half-duplex window ---- */
        ipc_set_tx_active(false);
    }
}

/* ---------------- Public API ---------------- */

void tx_worker_start(k_thread_stack_t *stack, size_t stack_size, int priority)
{
    (void)k_thread_create(
            &tx_tcb, stack, stack_size,
            tx_worker_thread, NULL, NULL, NULL,
            priority, 0, K_NO_WAIT);
}
