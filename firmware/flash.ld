ENTRY(reset)
MEMORY {
 FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2048K
 SRAM (rw) : ORIGIN = 0x20000000, LENGTH = 192K
}
SECTIONS {
 .text : { KEEP(*(.vectors)) *(.text*) *(.rodata*) } > FLASH
 .data : { data_start = .; *(.data*) . = ALIGN(4); data_end = .; } > SRAM AT> FLASH
 data_rom = LOADADDR(.data);
 .payload : { *(.payload) } > FLASH
 .bss (NOLOAD) : { bss_start = .; *(.bss*) *(COMMON) . = ALIGN(4); bss_end = .; } > SRAM
 ASSERT(bss_end < 0x20010000, "loader stack overlap")
}
