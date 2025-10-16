/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include "payload.h"

#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>   /* BUILD_ASSERT */
#include <string.h>            /* memcpy, memset */

/* Ensure atomic_t is at least 32 bits on this target (true for nRF52, most 32-bit MCUs) */
BUILD_ASSERT(sizeof(atomic_t) >= sizeof(uint32_t), "atomic_t must be >= 32-bit");

/* Internals: two lock-free 4-byte caches + a ready-bitfield */
static atomic_t a_cache;      /* latest A payload (packed u32) */
static atomic_t b_cache;      /* latest B payload (packed u32) */
static atomic_t c_cache;      /* latest C payload (packed u32) */
static atomic_t ready_flags;  /* bitfield of which payloads are ready */

#define FLAG_A_READY BIT(0)
#define FLAG_B_READY BIT(1)
#define FLAG_C_READY BIT(2)

static inline uint32_t pack4(const uint8_t v[4])
{
    /* Use memcpy to avoid aliasing/endianness pitfalls of type-punning */
    uint32_t u32;
    memcpy(&u32, v, sizeof(u32));
    return u32;
}

static inline void unpack4(uint32_t u32, uint8_t out[4])
{
    memcpy(out, &u32, sizeof(u32));
}

void set_A(const uint8_t v[4])
{
    atomic_set(&a_cache, (atomic_val_t)pack4(v));  /* publish value first */
    atomic_or(&ready_flags, FLAG_A_READY);         /* then mark as ready */
}

void set_B(const uint8_t v[4])
{
    atomic_set(&b_cache, (atomic_val_t)pack4(v));  /* publish value first */
    atomic_or(&ready_flags, FLAG_B_READY);         /* then mark as ready */
}

void set_C(const uint8_t v[4])
{
    atomic_set(&c_cache, (atomic_val_t)pack4(v));  /* publish value first */
    atomic_or(&ready_flags, FLAG_C_READY);         /* then mark as ready */
}

bool is_ready(uint8_t cmd)
{
    uint32_t flags = (uint32_t)atomic_get(&ready_flags);
    if (cmd == CMD_A) 
    {
        return (flags & FLAG_A_READY) != 0u;
    } 
    else if (cmd == CMD_B) 
    {
        return (flags & FLAG_B_READY) != 0u;
    }
    else if (cmd == CMD_C) 
    {
        return (flags & FLAG_C_READY) != 0u;
    }
    return false;
}

void snapshot(uint8_t cmd, uint8_t out[4])
{
    if (cmd == CMD_A) 
    {
        uint32_t u = (uint32_t)atomic_get(&a_cache);
        unpack4(u, out);
    } 
    else if (cmd == CMD_B)
    {
        uint32_t u = (uint32_t)atomic_get(&b_cache);
        unpack4(u, out);
    } 
    else if (cmd == CMD_C) 
    {
        uint32_t u = (uint32_t)atomic_get(&c_cache);
        unpack4(u, out);
    } else 
    {
        /* Unknown command: return zeroes to be deterministic */
        memset(out, 0, 4);
    }
}

void clear_ready(uint8_t cmd)
{
    if (cmd == CMD_A) 
    {
        atomic_and(&ready_flags, ~FLAG_A_READY);
    } 
    else if (cmd == CMD_B) 
    {
        atomic_and(&ready_flags, ~FLAG_B_READY);
    }
    else if (cmd == CMD_C) 
    {
        atomic_and(&ready_flags, ~FLAG_C_READY);
    }
    /* Unknown cmd: ignore */
}
