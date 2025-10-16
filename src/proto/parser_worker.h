/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <zephyr/kernel.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the parser worker thread (bytes -> commands).
 *
 * @param stack       Thread stack (from K_THREAD_STACK_DEFINE)
 * @param stack_size  Size of stack (K_THREAD_STACK_SIZEOF(...))
 * @param priority    Thread priority (e.g., K_PRIO_PREEMPT(7))
 */
void parser_worker_start(k_thread_stack_t *stack, size_t stack_size, int priority);

#ifdef __cplusplus
}
#endif
