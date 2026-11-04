# Linux on STM32F429I-DISCO
[Русский](README.md)

The project aims to boot embedded Linux on the Discovery board and verify it on hardware. Linux 6.14.5 has booted from SDRAM via SWD. The kernel launched /init from the built-in initramfs. The BusyBox hush shell accepts commands over COM21. Running uname -r returned 6.14.5. The owner selects the bootloader, kernel and root filesystem.

Run `make check` in WSL. See the [build and SWD record](docs/BUILD.md), [specification](docs/requirements/TECHNICAL_SPECIFICATION_LINUX.md), [decisions](TODO.md) and [sources](docs/SOURCES.md) (Russian).
