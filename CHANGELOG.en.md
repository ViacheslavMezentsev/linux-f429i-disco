# Changelog
[Русский](CHANGELOG.md)

## Unreleased
Verified that the LAN8720 clocks the MAC through PA1 after configuring the gyroscope to release INT1.

Added specification 1.0, an offline check of the linked Cortex-M4 ELF and a record of the MCU identification over SWD.

TC-03 passed on hardware. The execution marker, hardware stop address and restoration of all 4 KiB of internal SRAM were verified.

Verified FMC initialization and sampled external SDRAM reads and writes on MB1075 B-01.

Verified all 8 MiB of external SDRAM and built a Linux 6.14.5 kernel and DTB for Discovery.

Linux 6.14.5 booted from SDRAM on the Discovery board via SWD. COM21 captured the early kernel log for TC-05.

TC-06: the Discovery board ran the built-in initramfs /init. Windows captured its PID 1 confirmation on COM21.

TC-07 passed. Windows sent uname -r to BusyBox hush over COM21 and received 6.14.5 and a new prompt.
