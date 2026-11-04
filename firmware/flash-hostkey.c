/* STM32F429ZI, dual-bank 2 MiB, 32-bit programming at VDD >= 2.7 V.
 * Sector 23 is private to this board. Never erase from the boot path. */
#include <stdint.h>
#include <string.h>
#include "monocypher-ed25519.h"

#define REG(addr) (*(volatile uint32_t *)(addr))
#define KEYR REG(0x40023c04)
#define SR REG(0x40023c0c)
#define CR REG(0x40023c10)
#define BUSY (1u << 16)
#define LOCK (1u << 31)
#define ERRORS 0x1f2u
#define MAGIC 0x314b5348u

struct record {
    uint32_t seed[8];
    uint32_t checksum;
    uint32_t committed;
};

extern uint32_t boot_crc(const unsigned char *, unsigned);
extern uint32_t rng_word(void);

static int program(volatile uint32_t *dest, uint32_t value)
{
    CR = (2u << 8) | 1u;
    *dest = value;
    __asm volatile ("dsb" ::: "memory");
    while (SR & BUSY) {}
    CR = 0;
    return !(SR & ERRORS) && *dest == value;
}

static int blank(const volatile struct record *r)
{
    for (unsigned i = 0; i < 8; ++i)
        if (r->seed[i] != UINT32_MAX) return 0;
    return r->checksum == UINT32_MAX && r->committed == UINT32_MAX;
}

int install_hostkey(unsigned char *private_data)
{
    volatile struct record *records = (void *)0x081e0000;
    uint32_t seed[8];
    unsigned char pub[32];
    for (unsigned slot = 0; slot < 0x20000 / sizeof(*records); ++slot) {
        volatile struct record *r = &records[slot];
        if (r->committed == MAGIC) {
            memcpy(seed, (const void *)r->seed, sizeof seed);
            if (r->checksum != boot_crc((void *)seed, sizeof seed)) return 0;
        } else if (blank(r)) {
            for (unsigned i = 0; i < 8; ++i) seed[i] = rng_word();
            while (SR & BUSY) {}
            KEYR = 0x45670123;
            KEYR = 0xcdef89ab;
            if (CR & LOCK) return 0;
            SR = ERRORS | 1;
            int ok = 1;
            for (unsigned i = 0; ok && i < 8; ++i) ok = program(&r->seed[i], seed[i]);
            if (ok) ok = program(&r->checksum, boot_crc((void *)seed, sizeof seed));
            if (ok) ok = program(&r->committed, MAGIC);
            CR = LOCK;
            if (!ok) return 0;
        } else {
            continue; /* Power loss before commit: try a fresh record. */
        }
        crypto_ed25519_key_pair(private_data, pub, (void *)seed);
        return memcmp(private_data + 32, pub, 32) == 0;
    }
    return 0;
}
