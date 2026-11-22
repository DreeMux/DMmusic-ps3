# DMmusic - PS3 VSH background mp3 player plugin
# Made by DreeMux — Made for PS3 gamers
#
# Copyright (C) DreeMux (https://github.com/DreeMux)
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
# Included from plugin/Makefile with:  -include prx/link.mk
# Self-contained: builds its own objects in prx/obj/, so it does not depend
# on variable names used by the parent Makefile.
#
# Usage:  make -C plugin clean all
# Set USE_PSL1GHT_STUBS=1 to link -laudio -lio instead of prx/vsh_stubs.S
# (fallback if the hand-written stubs are rejected by sprxlinker).

PS3DEV   ?= /usr/local/ps3dev
PSL1GHT  ?= $(PS3DEV)
export PS3DEV PSL1GHT

DM_BIN     := $(PS3DEV)/bin
DM_PPUBIN  := $(PS3DEV)/ppu/bin
export PATH := $(DM_BIN):$(DM_PPUBIN):$(PATH)

DM_PPULIB  := $(PS3DEV)/ppu/powerpc64-ps3-elf/lib
DM_SPRX_O  := $(DM_PPULIB)/lv2-sprx.o
DM_LV2_LD  := $(DM_PPULIB)/lv2.ld

DM_CC      := ppu-gcc
DM_STRIP   := ppu-strip
DM_SPRXLNK := $(DM_BIN)/sprxlinker
DM_FSELF   := $(DM_BIN)/fself
DM_MAKESELF:= $(DM_BIN)/make_self

DM_SRC     := $(wildcard source/*.c)
DM_OBJDIR  := prx/obj
DM_OBJS    := $(patsubst source/%.c,$(DM_OBJDIR)/%.o,$(DM_SRC))
DM_PRXOBJS := $(DM_OBJDIR)/prx_entry.o
ifneq ($(USE_PSL1GHT_STUBS),1)
DM_PRXOBJS += $(DM_OBJDIR)/vsh_stubs.o
DM_LIBS    := -lgcc
else
DM_LIBS    := -L$(PSL1GHT)/ppu/lib -laudio -lio -lsysutil -llv2 -lgcc
endif

DM_CFLAGS  := -Os -mcpu=cell -ffunction-sections -fdata-sections \
              -fno-builtin -fno-stack-protector -Wall \
              -Iinclude -I$(PSL1GHT)/ppu/include
DM_ASFLAGS := -mcpu=cell -Iinclude -I$(PSL1GHT)/ppu/include
DM_LDFLAGS := -nostartfiles -nostdlib -Wl,--gc-sections -Wl,-T,$(DM_LV2_LD)

DM_ELF     := prx/dmmusic.elf
DM_SPRX    := dmmusic.sprx

all: $(DM_SPRX)

$(DM_OBJDIR):
	@mkdir -p $@

$(DM_OBJDIR)/%.o: source/%.c | $(DM_OBJDIR)
	$(DM_CC) $(DM_CFLAGS) -c $< -o $@

$(DM_OBJDIR)/prx_entry.o: prx/prx_entry.c | $(DM_OBJDIR)
	$(DM_CC) $(DM_CFLAGS) -c $< -o $@

$(DM_OBJDIR)/vsh_stubs.o: prx/vsh_stubs.S | $(DM_OBJDIR)
	$(DM_CC) $(DM_ASFLAGS) -c $< -o $@

$(DM_ELF): $(DM_OBJS) $(DM_PRXOBJS)
	@test -f $(DM_SPRX_O) || { echo "missing $(DM_SPRX_O)"; exit 1; }
	@test -f $(DM_LV2_LD) || { echo "missing $(DM_LV2_LD)"; exit 1; }
	$(DM_CC) $(DM_LDFLAGS) $(DM_SPRX_O) $(DM_OBJS) $(DM_PRXOBJS) $(DM_LIBS) -o $@
	$(DM_STRIP) -s $@
	$(DM_SPRXLNK) $@

$(DM_SPRX): $(DM_ELF)
	@if [ -x $(DM_FSELF) ]; then \
	  echo "FSELF  $@"; $(DM_FSELF) $< $@; \
	else \
	  echo "MAKE_SELF  $@"; $(DM_MAKESELF) $< $@; \
	fi
	@ls -l $@

clean-prx:
	rm -rf $(DM_OBJDIR) $(DM_ELF) $(DM_SPRX)

clean: clean-prx

.PHONY: all clean clean-prx