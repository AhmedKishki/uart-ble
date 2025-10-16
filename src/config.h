/* config.h */
#pragma once
#define AB_RX_CHUNK_LEN          64
#define AB_RX_IDLE_TIMEOUT_US    20000
#define STACK_PARSER             1024
#define STACK_TX_WORKER          1024
#define STACK_PROD               768
#define PRIO_WORKERS             7
#define PRIO_PRODUCERS           8
/* Periods for the producers */
#define AB_PROD_A_PERIOD_MS 10
#define AB_PROD_B_PERIOD_MS 50
