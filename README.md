# Linux на STM32F429I-DISCO
[English](README.en.md)

Цель — загрузить встраиваемый Linux на Discovery и подтвердить запуск на плате. Linux 6.14.5 запущен через SWD из SDRAM. Ядро запустило минимальный /init из встроенного initramfs. Оболочка BusyBox hush принимает команды через COM21; uname -r вернула 6.14.5. Решения по загрузчику, ядру и корневой ФС принимает владелец.

[ТЗ](docs/requirements/TECHNICAL_SPECIFICATION_LINUX.md) · [Сборка и подключение](docs/BUILD.md) · [Решения](TODO.md) · [Источники](docs/SOURCES.md). В WSL выполните `make check`.

[Проверенный пакет top для установки по SCP](artifacts/armv7m-bflt/busybox-top/1.36.1-r1/README.md)
