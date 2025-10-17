/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "parser_worker.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ipc.h"
#include "proto/payload.h"   /* for CMD_A / CMD_B */

LOG_MODULE_REGISTER(parser, LOG_LEVEL_INF);

/* Thread control block owned here */
static struct k_thread parser_tcb;

#define PARSER_THREAD_NAME "parser_worker"

/* Simple two-byte protocol: 'S' <cmd> where cmd ∈ {'A','B'} */
enum parse_state {
    Break = 0x00,
    Sync  = 0x55,
    Poll,
    Reply
};

static void parser_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    enum parse_state state = Break;
    uint8_t ch;

    for (;;) {
        /* Block until a byte is available from ISR */
        if (!ipc_rx_get(&ch, K_FOREVER)) {
            continue; /* defensive: should not happen with FOREVER */
        }

        switch (state) 
        {
            case Break:
                if (ch == 0x00) 
                {
                    state = Sync; /* got Break */
                }
                break;
            
            case Sync:
                if (ch == 0x55) 
                {
                    state = Poll; /* got Sync */
                } 
                break;

            case Poll:
                if (ch == CMD_A || ch == CMD_B || ch == CMD_C)
                {
                    (void)ipc_cmd_put(ch);  /* non-blocking; drops if full */
                }
                /* Regardless of what arrived, reset and look for next frame */
                state = Reply;
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
