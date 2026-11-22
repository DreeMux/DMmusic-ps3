#---------------------------------------------------------------------------------
# DMmusic - VSH background plugin (dmmusic.sprx) - Makefile
# Made by DreeMux — Made for PS3 gamers
#
# Copyright (C) 2026 DreeMux
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Targets (run from the repository root as "make -C plugin <target>"):
#   objs       compile every plugin source with the PS3 toolchain (no link)
#   all        objs + link/sign dmmusic.sprx (needs prx/link.mk, see below)
#   host-test  build and run the x86 unit test (WAV, queue, pad, engine)
#   clean
#
# prx/link.mk is the ONLY file that knows how a VSH PRX is laid out (module
# info, export table, import stubs for cellAudio/cellPad, linker script and
# signing).  It is deliberately separate: it must be written against the
# real files of the toolchain image (see the "PRX toolchain probe" step of
# .github/workflows/build.yml) instead of from memory.  When it exists it must
# define a rule that builds $(TARGET) from $(OBJS).
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(PSL1GHT)),)
ifeq ($(filter host-test clean,$(MAKECMDGOALS)),)
$(error "PSL1GHT is not set. Use the ps3dev/ps3dev Docker image or: export PSL1GHT=<path to PSL1GHT>")
endif
else
include $(PSL1GHT)/ppu_rules
endif

TARGET   := dmmusic.sprx
BUILD    := build
SRCDIR   := source
INCDIR   := include

SRCS     := main.c audio_engine.c pad_hook.c decoder.c dm_sys.c dm_libc.c
OBJS     := $(addprefix $(BUILD)/,$(SRCS:.c=.o))

# Small, freestanding, position-independent friendly flags.
DM_CFLAGS := -O2 -Wall -Wextra -Wno-unused-function -std=gnu99 \
             -ffunction-sections -fdata-sections \
             -I$(INCDIR) -I$(SRCDIR)

.DEFAULT_GOAL := all
.PHONY: all objs clean host-test check-deps

check-deps:
	@test -f $(INCDIR)/dr_mp3.h || { \
	  echo "error: $(INCDIR)/dr_mp3.h is missing."; \
	  echo "       CI downloads it; locally run:"; \
	  echo "       curl -fsSL https://raw.githubusercontent.com/mackron/dr_libs/master/dr_mp3.h -o $(INCDIR)/dr_mp3.h"; \
	  exit 1; }

objs: check-deps $(OBJS)

$(BUILD):
	@mkdir -p $@

# dm_libc.c must not be "optimised" into calls to the functions it defines.
$(BUILD)/dm_libc.o: $(SRCDIR)/dm_libc.c | $(BUILD)
	@echo "  CC   $<"
	@$(CC) $(DM_CFLAGS) -fno-builtin -fno-tree-loop-distribute-patterns -c $< -o $@

$(BUILD)/%.o: $(SRCDIR)/%.c | $(BUILD)
	@echo "  CC   $<"
	@$(CC) $(DM_CFLAGS) -c $< -o $@

ifneq ($(wildcard prx/link.mk),)
include prx/link.mk
all: $(TARGET)
else
all: objs
	@echo "error: prx/link.mk not found - the PRX packaging layer has not been written yet."
	@echo "       All plugin sources compiled ($(words $(OBJS)) objects); only the final link/sign step is missing."
	@exit 1
endif

#---------------------------------------------------------------------------------
# Host unit test (needs only gcc + libm; MP3 is excluded because dr_mp3 is
# fetched by CI, WAV/queue/pad/engine logic is fully exercised)
#---------------------------------------------------------------------------------
HOST_BIN ?= /tmp/dmmusic_host_test

host-test:
	gcc -O1 -g -Wall -DDM_HOST -DDM_NO_MP3 -I$(INCDIR) -o $(HOST_BIN) \
	    test/host_test.c $(SRCDIR)/main.c $(SRCDIR)/audio_engine.c $(SRCDIR)/pad_hook.c \
	    $(SRCDIR)/decoder.c $(SRCDIR)/dm_sys.c -lm -lpthread
	$(HOST_BIN)

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET)
