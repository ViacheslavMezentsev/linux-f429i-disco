# Changelog
[Русский](CHANGELOG.md)

## Unreleased
Added an optional 40×40 text fbdev and lcd-text for the demo image. Verified TCC output on SPI5 with DMA, a 0.416 s full-screen fill and concurrent Ethernet traffic.

Added a Linux GPIO probe for 5×8 text on the ILI9341. Windows received 36 of 36 ping replies and PG11 retained AF11. Host checks now build the SRAM probe without installing GCC via apt.

Enabled interactive top in the compact BusyBox build. Verified screen refresh, q to exit and shell memory on the STM32 over SSH.

The demo firmware accepts user/user with UID 1000, disables root SSH login and keeps its per-board Ed25519 identity in sector 23. The build produces BIN and Intel HEX images.

Cold boot now provides compact BusyBox and native TCC in /bin. Verified on-board Hello World compilation over SSH and a 16 KiB SCP round trip.

The flash bootloader now selects 168 MHz. Verified interactive SSH and a 16 KiB SCP round trip after both a push-button reset and a full power cycle.

Verified an SWD-only 168 MHz profile with a full SDRAM test, Linux boot, SSH login and SCP round trip.

Verified boot from internal flash after a power cycle, an interactive SSH terminal and a fresh 16 KiB SCP round trip with matching hashes.

Linux 6.14.5, Dropbear, SCP and a DEFLATE bootloader fit in internal flash. Verified public-key SSH login and a 16 KiB SCP round trip with matching SHA-256 on the board and host.

Verified ICMP echo over eth0 on Linux 6.14.5 with the LAN8720. Added the diagnostic configuration and Cortex-M receive alignment fix.

The MAC received two frames with verified FCS using PC4/PC5 without board rework.

Sampled PC4 and PC5 before connecting the RMII receive lines.

Verified that the LAN8720 clocks the MAC through PA1 after configuring the gyroscope to release INT1.

Added specification 1.0, an offline check of the linked Cortex-M4 ELF and a record of the MCU identification over SWD.

TC-03 passed on hardware. The execution marker, hardware stop address and restoration of all 4 KiB of internal SRAM were verified.

Verified FMC initialization and sampled external SDRAM reads and writes on MB1075 B-01.

Verified all 8 MiB of external SDRAM and built a Linux 6.14.5 kernel and DTB for Discovery.

Linux 6.14.5 booted from SDRAM on the Discovery board via SWD. COM21 captured the early kernel log for TC-05.

TC-06: the Discovery board ran the built-in initramfs /init. Windows captured its PID 1 confirmation on COM21.

TC-07 passed. Windows sent uname -r to BusyBox hush over COM21 and received 6.14.5 and a new prompt.
