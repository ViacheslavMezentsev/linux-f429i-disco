CROSS_COMPILE ?= arm-none-eabi-
PROBE_CC ?= $(CROSS_COMPILE)gcc
SPEC := docs/requirements/TECHNICAL_SPECIFICATION_LINUX.md

.PHONY: check
check: out/probe.elf
	python3 tools/check_spec.py $(SPEC) --strict
	python3 tools/check_probe.py $<

out/probe.elf: firmware/probe.S firmware/sram.ld
	mkdir -p out
	$(PROBE_CC) -mcpu=cortex-m4 -mthumb -g -nostdlib -Wl,--build-id=none,-T,firmware/sram.ld -o $@ $<
