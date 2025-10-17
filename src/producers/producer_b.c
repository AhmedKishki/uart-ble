/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "producers.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "config.h"
#include "proto/payload.h"

LOG_MODULE_REGISTER(prod_b, LOG_LEVEL_INF);

/* Fallback period if not set in config.h */
#ifndef PROD_B_PERIOD_MS
#define PROD_B_PERIOD_MS 50
#endif

/* Thread control block (owned here) */
static struct k_thread prod_b_tcb;

/* Optional name */
#define PROD_B_THREAD_NAME "producer_b"

/* Pluggable fill function; default fills 'B' bytes */
static void default_fill_B(uint8_t out[4])
{
    out[0] = 0x42; out[1] = 0x42; out[2] = 0x42; out[3] = 0x42; /* 'B' */
}
static fill_fn_t fill_B = default_fill_B;

void producer_b_set_fill(fill_fn_t fn)
{
    fill_B = (fn != NULL) ? fn : default_fill_B;
}

static void producer_b_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    uint8_t v[4];

    for (;;) {
        fill_B(v);         /* produce 4 bytes */
        set_B(v);       /* publish + set ready flag */
        k_sleep(K_MSEC(PROD_B_PERIOD_MS));
    }
}

void producer_b_start(k_thread_stack_t *stack, size_t stack_size, int priority)
{
    (void)k_thread_create(
        &prod_b_tcb, stack, stack_size,
        producer_b_thread, NULL, NULL, NULL,
        priority, 0, K_NO_WAIT);
}