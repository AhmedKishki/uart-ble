/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Command bytes this module understands */
#define CMD_A 'A'
#define CMD_B 'B'

/* Producers publish 4-byte payloads and set the corresponding "ready" flag */
void ab_set_A(const uint8_t v[4]);
void ab_set_B(const uint8_t v[4]);

/* Query if a payload for CMD_A / CMD_B is ready (true = ready) */
bool ab_is_ready(uint8_t cmd);

/* Copy out the latest 4-byte snapshot for cmd (A/B). No effect if cmd unknown. */
void ab_snapshot(uint8_t cmd, uint8_t out[4]);

/* Clear the ready flag after a successful send (A/B). No effect if cmd unknown. */
void ab_clear_ready(uint8_t cmd);

#ifdef __cplusplus
}
#endif
