# Changelog
[Русский](CHANGELOG.md)

## Unreleased
Added specification 1.0, an offline check of the linked Cortex-M4 ELF and a record of the MCU identification over SWD.

TC-03 passed on hardware. The execution marker, hardware stop address and restoration of all 4 KiB of internal SRAM were verified.

Verified FMC initialization and sampled external SDRAM reads and writes on MB1075 B-01.

Verified all 8 MiB of external SDRAM and built a Linux 6.14.5 kernel and DTB for Discovery.

Linux 6.14.5 booted from SDRAM on the Discovery board via SWD. COM21 captured the early kernel log for TC-05.
