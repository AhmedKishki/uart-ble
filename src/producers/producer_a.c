/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "producers.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "config.h"
#include "proto/ab_payload.h"

LOG_MODULE_REGISTER(ab_prod_a, LOG_LEVEL_INF);

/* Fallback period if not set in config.h */
#ifndef AB_PROD_A_PERIOD_MS
#define AB_PROD_A_PERIOD_MS 10
#endif

/* Thread control block (owned here) */
static struct k_thread prod_a_tcb;

/* Optional name */
#define PROD_A_THREAD_NAME "producer_a"

/* Pluggable fill function; default fills 'A' bytes */
static void default_fill_A(uint8_t out[4])
{
    out[0] = 0x41; out[1] = 0x41; out[2] = 0x41; out[3] = 0x41; /* 'A' */
}
static ab_fill_fn_t fill_A = default_fill_A;

void producer_a_set_fill(ab_fill_fn_t fn)
{
    fill_A = (fn != NULL) ? fn : default_fill_A;
}

static void producer_a_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    uint8_t v[4];

    for (;;) {
        fill_A(v);         /* produce 4 bytes */
        ab_set_A(v);       /* publish + set ready flag */
        k_sleep(K_MSEC(AB_PROD_A_PERIOD_MS));
    }
}

void producer_a_start(k_thread_stack_t *stack, size_t stack_size, int priority)
{
    (void)k_thread_create(
        &prod_a_tcb, stack, stack_size,
        producer_a_thread, NULL, NULL, NULL,
        priority, 0, K_NO_WAIT);

#if defined(CONFIG_THREAD_NAME)
    k_thread_name_set(&prod_a_tcb, PROD_A_THREAD_NAME);
#endif
}
