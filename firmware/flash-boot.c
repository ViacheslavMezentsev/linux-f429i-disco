#include <stdint.h>
#include "puff.h"
#include "sizes.h"
#define R(a) (*(volatile uint32_t *)(a))
#define RCC 0x40023800u
#define A 0x40020000u
#define C 0x40020800u
#define F 0x40021400u
extern const unsigned char compressed[], tree[];
extern unsigned sdram_init(void);
extern void enter_linux(void);
static void field(unsigned addr, unsigned mask, unsigned bits)
{
    R(addr) = (R(addr) & ~mask) | bits;
}
static void say(const char *s)
{
    while (*s) {
        while (!(R(0x40011000) & 128)) {}
        R(0x40011004) = *s++;
    }
}
static void fail(const char *s) { say(s); for (;;) {} }
static unsigned spi(unsigned output)
{
    unsigned value = 0;
    for (unsigned mask = 128; mask; mask >>= 1) {
        R(F+24) = 128u << 16;
        R(F+24) = output & mask ? 512 : 512u << 16;
        R(F+24) = 128;
        value = (value << 1) | ((R(F+16) >> 8) & 1);
    }
    return value;
}
static unsigned gyro(unsigned reg, unsigned data)
{
    R(C+24) = 2u << 16;
    spi(reg);
    unsigned v = spi(data);
    R(C+24) = 2;
    return v;
}
static uint32_t crc(const unsigned char *p, unsigned len)
{
    uint32_t v = ~0u;
    while (len--) {
        v ^= *p++;
        for (unsigned i = 0; i < 8; ++i)
            v = (v >> 1) ^ (0xedb88320u & -(v & 1));
    }
    return ~v;
}
void boot(void)
{
    R(RCC+0x30) |= 0x25;
    R(RCC+0x44) |= 0x4010;
    (void)R(RCC+0x44);
    field(A+0x24,0xf0,0x70);
    field(A,3u<<18,2u<<18);
    R(0x40011008) = 139;
    R(0x4001100c) = 0x200c;
    R(C+24) = 6;
    field(C,0x3c,0x14);
    R(F+24) = 128;
    field(F,0xfc000,0x44000);
    if (gyro(0x8f,0) != 0xd4) fail("GYRO ID\r\n");
    gyro(0x22,0x30);
    if (gyro(0xa2,0) != 0x30) fail("GYRO CTRL\r\n");
    say("PA1 RELEASED\r\n");
    if (sdram_init()) fail("FMC\r\n");
    field(0x40013800,0xc00,0x400);
    R(0x40023c00) = 5;
    R(RCC+4) = 0x27015410;
    field(RCC+8,0xfc00,0x1000);
    R(RCC) |= 0x01000000;
    while (!(R(RCC) & 0x02000000)) {}
    field(RCC+8,3,2);
    while ((R(RCC+8)&12) != 8) {}
    R(0xa0000154) = 1272;
    R(0x40011008) = 729;
    R(0xe000ed98) = 0;
    R(0xe000ed9c) = 0x90000000;
    R(0xe000eda0) = 0x0303002d;
    R(0xe000ed94) = 5;
    __asm volatile("dsb; isb" ::: "memory");
    unsigned char *kernel = (void *)0x90000000, *dt = (void *)0x902f0000;
    unsigned long out = IMAGE_SIZE, in = COMP_SIZE;
    if (puff(kernel,&out,compressed,&in) || out!=IMAGE_SIZE || in!=COMP_SIZE ||
        crc(kernel,out)!=IMAGE_CRC) fail("INFLATE/CRC\r\n");
    for (unsigned i=0; i<TREE_SIZE; i++) dt[i]=tree[i];
    if (crc(dt,TREE_SIZE)!=TREE_CRC) fail("DT CRC\r\n");
    /* Seed Linux from the hardware RNG; never credit placeholder bytes. */
    R(RCC+0x34) |= 64;
    (void)R(RCC+0x34);
    R(0x50060800) = 4;
    for (unsigned i=0; i<32; i+=4) {
        unsigned n=2000000;
        while (!(R(0x50060804)&1) && --n) {}
        if (!n || (R(0x50060804)&6)) fail("RNG ERROR\r\n");
        uint32_t v=R(0x50060808);
        for (unsigned j=0; j<4; j++) dt[SEED_OFFSET+i+j]=v>>(8*j);
    }
    R(0x50060800)=0;
    field(A+0x20,0xf0,0xb0);
    field(A,12,8);
    field(0x40013804,0x800000,0x800000);
    R(RCC+0x30)|=0x0e000000;
    (void)R(RCC+0x30);
    R(0x40029000)|=1;
    say("WAIT REF_CLK\r\n");
    while (R(0x40029000)&1) {}
    say("FLASH -> SDRAM OK\r\n");
    enter_linux();
}
