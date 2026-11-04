#include <stdint.h>

#define REG(a) (*(volatile uint32_t *)(a))
#define RCC 0x40023800u
#define FMC 0xa0000140u

/* Configure only MB1075 B-01 pins connected to SDRAM. */
static void configure_pins(unsigned port, unsigned pins)
{
    unsigned base = 0x40020000u + port * 0x400u;
    for (unsigned pin = 0; pin < 16; ++pin) {
        if (!(pins & (1u << pin))) continue;
        unsigned shift = pin * 2;
        unsigned af = base + 0x20 + 4 * (pin / 8);
        unsigned nibble = 4 * (pin % 8);
        REG(af) = (REG(af) & ~(15u << nibble)) | (12u << nibble);
        REG(base + 4) &= ~(1u << pin);
        REG(base + 8) |= 3u << shift;
        REG(base + 12) &= ~(3u << shift);
        REG(base) = (REG(base) & ~(3u << shift)) | (2u << shift);
    }
}

static void command(unsigned value)
{
    REG(FMC + 0x10) = value | (1u << 3); /* CTB2 */
    while (REG(FMC + 0x18) & 0x20) {}
}

unsigned sdram_init(void)
{
    if ((REG(RCC + 8) & 0xfc) || (REG(RCC + 0x38) & 1)) return 1;
    REG(RCC + 0x44) |= 1u << 14;
    (void)REG(RCC + 0x44);
    if (REG(0x40013800u) & (3u << 10)) return 2;
    REG(RCC + 0x30) |= 0x7e;
    (void)REG(RCC + 0x30);
    configure_pins(1, 0x0060);
    configure_pins(2, 0x0001);
    configure_pins(3, 0xc703);
    configure_pins(4, 0xff83);
    configure_pins(5, 0xf83f);
    configure_pins(6, 0x8133);
    REG(RCC + 0x38) |= 1;
    (void)REG(RCC + 0x38);
    REG(FMC) = 2u << 10; /* SDCLK = HCLK / 2 */
    REG(FMC + 4) = (1u << 2) | (1u << 4) | (1u << 6) | (3u << 7);
    /* Timing fields encode cycles minus one: MRD=2, XSR=7, RAS=4,
       RC=7, WR=3, RP=2, RCD=2. RC and RP are common to both banks. */
    REG(FMC + 8) = (6u << 12) | (1u << 20);
    REG(FMC + 12) = 0x01126361;
    command(1); /* clock enable */
    for (volatile unsigned delay = 0; delay < 16000; ++delay) {}
    command(2); /* precharge all */
    command(3 | (7u << 5)); /* eight auto-refresh cycles */
    command(4 | (0x230u << 9)); /* BL1, CAS3, single write */
    REG(FMC + 0x14) = 105u << 1; /* 8 MHz * 64 ms / 4096 - 20 */
    return 0;
}
