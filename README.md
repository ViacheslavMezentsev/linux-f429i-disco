# Linux на STM32F429I-DISCO
[English](README.en.md)

Цель — загрузить встраиваемый Linux на Discovery и подтвердить запуск на плате. Linux 6.14.5 запущен через SWD из SDRAM. На COM21 получены баннер ядра, модель платы и сообщения инициализации таймеров. Решения по загрузчику, ядру и корневой ФС принимает владелец.

[ТЗ](docs/requirements/TECHNICAL_SPECIFICATION_LINUX.md) · [Сборка и подключение](docs/BUILD.md) · [Решения](TODO.md) · [Источники](docs/SOURCES.md). В WSL выполните `make check`.
