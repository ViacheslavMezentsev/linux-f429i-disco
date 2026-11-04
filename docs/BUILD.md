# Сборка и проверка

## Чистая локальная копия
В WSL нужны Git, GNU Make, Python 3.8+ и GNU Arm Embedded (arm-none-eabi-gcc и binutils в PATH). Проверено с GCC 10.2.1. При необходимости пакет Ubuntu называется gcc-arm-none-eabi.

```sh
src=~/github/ViacheslavMezentsev/linux-f429i-disco
dst=$(mktemp -d)
git clone --no-local -b codex/bringup "$src" "$dst/repo"
cd "$dst/repo"
make check
arm-none-eabi-readelf -hl out/probe.elf
git status --short
```
Ожидаются ноль замечаний по ТЗ, TC-01 PASS и пустой status. ELF связан для первых 4 КиБ внутренней SRAM. Два начальных слова задают стек и вход в Thumb-код, который маскирует прерывания, записывает маркер и остаётся в цикле. Образ предназначен для запуска отладчиком. Его сборка не требует Linux toolchain для userspace.

## Протокол подключения, 04.11.2026
Windows обнаружила ST-LINK V2 (0483:3748), WSL lsusb показал только корневые концентраторы. В Windows PowerShell:
```powershell
STM32_Programmer_CLI -c port=SWD mode=HOTPLUG freq=100 -r32 0xE0042000 4
```
CubeProgrammer 2.23.0, ST-LINK V2J48S0, 2.90 В. На 4000 кГц цель не идентифицировалась. При 100 кГц прочитано 0x10036419, Device ID 0x419, Rev Y, Cortex-M4, Flash 2 МиБ. TC-02 пройден. Идентификатор общий для семейства F42x/F43x. Ревизию PCB подтвердить по маркировке. Использовалось только чтение.

## Первая аппаратная проба
После согласования остановки текущей программы выбрать доступный GDB-сервер с SWD через Windows, использовать ELF именно из проверенной сборки. Остановить ядро в привилегированном Thread mode, загрузить сегменты в SRAM, выставить MSP, выбрать MSP как активный стек, выставить Thumb PC по векторам, обнулить probe_result и установить аппаратную точку останова на probe_done. Продолжить исполнение и считать слово из probe_result. Критерий TC-03 — 0x429 и остановка на probe_done. Для повторных прогонов оформить сценарий stm32-gdbtest. По окончании перезапустить штатную прошивку. Для TC-04 потребуется отдельный Linux-образ и протокол консоли.
