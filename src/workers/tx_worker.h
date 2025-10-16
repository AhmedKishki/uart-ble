/* tx_worker.h - Serial TX worker: serializes payload sends */
#pragma once

#include <zephyr/kernel.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the TX worker thread.
 *
 * @param stack      Pointer to thread stack (K_THREAD_STACK_DEFINE)
 * @param stack_size Size of the provided stack (K_THREAD_STACK_SIZEOF)
 * @param priority   Thread priority (e.g., K_PRIO_PREEMPT(7))
 */
void tx_worker_start(k_thread_stack_t *stack, size_t stack_size, int priority);

#ifdef __cplusplus
}
#endif
