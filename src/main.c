#include "config.h"
#include "ipc.h"
#include "serial/serial_io.h"
#include "proto/parser_worker.h"
#include "workers/tx_worker.h"
#include "producers/producers.h"

K_THREAD_STACK_DEFINE(parser_stack,  STACK_PARSER);
K_THREAD_STACK_DEFINE(worker_stack,  STACK_TX_WORKER);
K_THREAD_STACK_DEFINE(prod_a_stack,  STACK_PROD);
K_THREAD_STACK_DEFINE(prod_b_stack,  STACK_PROD);

int main(void)
{
    ipc_init();

    if (serial_io_init()) {
        return 0;
    }

    parser_worker_start(parser_stack, K_THREAD_STACK_SIZEOF(parser_stack), PRIO_WORKERS);
    tx_worker_start(worker_stack, K_THREAD_STACK_SIZEOF(worker_stack), PRIO_WORKERS);
    producer_a_start(prod_a_stack, K_THREAD_STACK_SIZEOF(prod_a_stack), PRIO_PRODUCERS);
    producer_b_start(prod_b_stack, K_THREAD_STACK_SIZEOF(prod_b_stack), PRIO_PRODUCERS);

    for (;;) { k_sleep(K_FOREVER); }
}
