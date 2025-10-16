/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <zephyr/kernel.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional custom fill function: write 4 bytes into out[] */
typedef void (*ab_fill_fn_t)(uint8_t out[4]);

/* Start producer threads (provide stacks from K_THREAD_STACK_DEFINE) */
void producer_a_start(k_thread_stack_t *stack, size_t stack_size, int priority);
void producer_b_start(k_thread_stack_t *stack, size_t stack_size, int priority);

/* (Optional) Override the default payload generators */
void producer_a_set_fill(ab_fill_fn_t fn);
void producer_b_set_fill(ab_fill_fn_t fn);

#ifdef __cplusplus
}
#endif
