# ZBC Semihosting MAME Integration Tests
#
# Generic Makefile with parameterized platform support.
# To add a new platform, add a configuration block and append to PLATFORMS.

# ============================================================
# Configuration
# ============================================================

SEMIHOST ?= ../..
MAME ?= ~/git/mame/mame

# Semihost share directory for file I/O operations
SHARE_DIR ?= $(HOME)/.mame/semihost

# MAME options for automated testing (no user interaction)
# -window: don't take over the display
# -skip_gameinfo: skip the game info screen
# -share_directory: root directory for semihost file I/O
MAME_OPTS = -window -skip_gameinfo -share_directory $(SHARE_DIR)

# ============================================================
# Platform Definitions
# Each platform needs: CC, CFLAGS, OBJCOPY, MAME_MACHINE
# ============================================================

PLATFORMS = i386

# --- i386 (32-bit x86) ---
# Load address = 1 << (1 + addr_bits/2) = 1 << 17 = 0x20000 for 32-bit
i386_CC       = gcc
i386_CFLAGS   = -m32
i386_LDFLAGS  = -Wl,-Ttext=0x20000
i386_OBJCOPY  = objcopy
i386_MAME     = zbci386

# --- amd64 (64-bit x86) ---
amd64_CC      = gcc
amd64_CFLAGS  =
amd64_LDFLAGS =
amd64_OBJCOPY = objcopy
amd64_MAME    = zbcamd64

# --- m6502 (MOS Technology 6502) ---
# Load address = 1 << (1 + addr_bits/2) = 1 << 9 = 0x200 for 16-bit
# Uses llvm-mos compiler with custom linker script
# Requires imag.S for imaginary register definitions
PLATFORMS += m6502
LLVM_MOS   = $(HOME)/git/llvm-mos/build/install/bin
m6502_CC      = $(LLVM_MOS)/clang
# Removed -flto to prevent excessive inlining; added -fno-inline-functions
m6502_CFLAGS  = --target=mos -Oz -fno-inline-functions -T zbc6502.ld
m6502_LDFLAGS =
m6502_OBJCOPY = $(LLVM_MOS)/llvm-objcopy
m6502_MAME    = zbcm6502
m6502_EXTRA   = crt6502.S crt6502.c

# To add ARM support:
# PLATFORMS += arm
# arm_CC       = arm-none-eabi-gcc
# arm_CFLAGS   = -mcpu=arm7tdmi -mthumb
# arm_OBJCOPY  = arm-none-eabi-objcopy
# arm_MAME     = zbcarm7

# ============================================================
# Common Build Settings
# ============================================================

COMMON_CFLAGS  = -nostdlib -nostartfiles -ffreestanding -O2 -static
COMMON_CFLAGS += -fno-pie -no-pie
COMMON_CFLAGS += -I$(SEMIHOST)/include
COMMON_CFLAGS += -DZBC_CLIENT
COMMON_CFLAGS += -Wall -Wextra

COMMON_LDFLAGS = -Wl,-e_start -Wl,--build-id=none

# Client library sources
CLIENT_SRCS = $(SEMIHOST)/src/zbc_client.c \
              $(SEMIHOST)/src/zbc_riff.c \
              $(SEMIHOST)/src/zbc_opcode_table.c

# ============================================================
# Generic Build Rules
# ============================================================

define PLATFORM_RULES

hello_$(1).elf: hello.c $$(CLIENT_SRCS) $$($(1)_EXTRA)
	$$($(1)_CC) $$($(1)_CFLAGS) $$(COMMON_CFLAGS) $$($(1)_LDFLAGS) $$(COMMON_LDFLAGS) \
		-o $$@ hello.c $$(CLIENT_SRCS) $$($(1)_EXTRA)

hello_$(1).bin: hello_$(1).elf
	$$($(1)_OBJCOPY) -O binary -j .text -j .rodata -j .data $$< $$@

test-$(1): hello_$(1).bin
	@echo "Testing $(1) on $$($(1)_MAME)..."
	@$$(MAME) $$($(1)_MAME) $$(MAME_OPTS) -quik $$< -seconds_to_run 5 && echo "PASS: $(1)" || (echo "FAIL: $(1)"; exit 1)

clean-$(1):
	rm -f hello_$(1).elf hello_$(1).bin

.PHONY: test-$(1) clean-$(1)

endef

$(foreach p,$(PLATFORMS),$(eval $(call PLATFORM_RULES,$(p))))

# ============================================================
# Aggregate Targets
# ============================================================

all: $(foreach p,$(PLATFORMS),hello_$(p).bin)

test: $(foreach p,$(PLATFORMS),test-$(p))
	@echo "=========================================="
	@echo "All $(words $(PLATFORMS)) platform tests passed"
	@echo "=========================================="

clean: $(foreach p,$(PLATFORMS),clean-$(p))

.PHONY: all test clean

# ============================================================
# Help
# ============================================================

help:
	@echo "ZBC Semihosting MAME Integration Tests"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build test binaries for all platforms"
	@echo "  test         - Run tests on all platforms"
	@echo "  test-<plat>  - Run test on specific platform (e.g., test-i386)"
	@echo "  clean        - Remove all build artifacts"
	@echo ""
	@echo "Configured platforms: $(PLATFORMS)"
	@echo ""
	@echo "Variables:"
	@echo "  SEMIHOST=$(SEMIHOST)"
	@echo "  MAME=$(MAME)"

.PHONY: help
