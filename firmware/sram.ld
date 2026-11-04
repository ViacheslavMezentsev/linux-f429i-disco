ENTRY(probe_start)
MEMORY { SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 4K }
SECTIONS {
    .vectors : { KEEP(*(.vectors)) } > SRAM
    .text : { *(.text*) } > SRAM
    .bss (NOLOAD) : { *(.bss*) } > SRAM
    stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
    ASSERT(. <= stack_top - 256, "insufficient stack space")
}
