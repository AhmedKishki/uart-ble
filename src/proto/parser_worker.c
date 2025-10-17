/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "parser_worker.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ipc.h"
#include "proto/payload.h"

LOG_MODULE_REGISTER(parser, LOG_LEVEL_INF);

/* Thread control block owned here */
static struct k_thread parser_tcb;

#define PARSER_THREAD_NAME "parser_worker"

/* Simple two-byte protocol: 'S' <cmd> where cmd ∈ {'A','B'} */
enum parse_state {
    Break,
    Sync,
    Poll
};

static void parser_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    uint8_t v[4];
    uint8_t idx = 0;
    struct ipc_rx_msg msg;
    enum parse_state state = Break;

    for (;;) {
        /* Block until a byte is available from ISR */
        if (!ipc_rx_get_pair(&msg, K_FOREVER)) 
        {
            continue;
        }

        if (msg.evt == IPC_RX_EVT_BREAK)
        {
            idx = 0;
            state = Break;
            continue;
        }

        switch (state) 
        {
            case Break:
                if (msg.data == 'S') 
                {
                    state = Sync;
                }
                break;

            case Sync:
                if (msg.data == CMD_A || msg.data == CMD_B || msg.data == CMD_C) 
                {
                    (void)ipc_cmd_put(msg.data);
                    state = Break;
                }
                else if (msg.data == CMD_D)
                {
                    state = Poll;
                }
                break;

            case Poll:
                LOG_INF("D received: 0x%02x", msg.data);
                v[idx] = msg.data;
                idx++;
                if (idx >= 4)
                {
                    set_D(v);
                    state = Break;
                }
                break;
        }
    }
}

void parser_worker_start(k_thread_stack_t *stack, size_t stack_size, int priority)
{
    (void)k_thread_create(
        &parser_tcb, stack, stack_size,
        parser_thread, NULL, NULL, NULL,
        priority, 0, K_NO_WAIT);
}
