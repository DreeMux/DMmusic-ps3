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
# Usage:  make -C plugin clean all

PS3DEV   ?= /usr/local/ps3dev
PSL1GHT  ?= $(PS3DEV)
export PS3DEV PSL1GHT

DM_BIN     := $(PS3DEV)/bin
DM_PPUBIN  := $(PS3DEV)/ppu/bin
export PATH := $(DM_BIN):$(DM_PPUBIN):$(PATH)

DM_PPULIB  := $(PS3DEV)/ppu/powerpc64-ps3-elf/lib
DM_LV2_LD  := $(DM_PPULIB)/lv2.ld

DM_CC      := ppu-gcc
DM_LD      := ppu-ld
DM_SPRXLNK := $(DM_BIN)/sprxlinker
DM_FSELF   := $(DM_BIN)/fself

DM_SRC     := $(wildcard source/*.c)
DM_OBJDIR  := prx/obj
DM_OBJS    := $(patsubst source/%.c,$(DM_OBJDIR)/%.o,$(DM_SRC))
DM_PRXOBJS := $(DM_OBJDIR)/prx_entry.o $(DM_OBJDIR)/vsh_stubs.o
DM_LIBGCC  := $(shell $(DM_CC) -print-libgcc-file-name)

DM_CFLAGS  := -O2 -mcpu=cell -ffunction-sections -fdata-sections \
              -fno-builtin -fno-stack-protector -Wall \
              -Iinclude -I$(PSL1GHT)/ppu/include
DM_ASFLAGS := -mcpu=cell -Iinclude -I$(PSL1GHT)/ppu/include

DM_AUTO_LD := prx/auto_prx.ld
DM_LDFLAGS := -q -T $(DM_AUTO_LD) -e module_start

DM_ELF     := prx/dmmusic.elf
DM_SPRX    := dmmusic.sprx

all: $(DM_SPRX)

$(DM_OBJDIR):
	@mkdir -p $@

$(DM_AUTO_LD): $(DM_LV2_LD)
	sed -e 's/0x00010000/0x00000000/g' -e 's/0x10000000/0x00020000/g' $< > $@

$(DM_OBJDIR)/%.o: source/%.c | $(DM_OBJDIR)
	$(DM_CC) $(DM_CFLAGS) -c $< -o $@

$(DM_OBJDIR)/prx_entry.o: prx/prx_entry.c | $(DM_OBJDIR)
	$(DM_CC) $(DM_CFLAGS) -c $< -o $@

$(DM_OBJDIR)/vsh_stubs.o: prx/vsh_stubs.S | $(DM_OBJDIR)
	$(DM_CC) $(DM_ASFLAGS) -c $< -o $@

$(DM_ELF): $(DM_AUTO_LD) $(DM_OBJS) $(DM_PRXOBJS)
	$(DM_LD) $(DM_LDFLAGS) $(DM_OBJS) $(DM_PRXOBJS) $(DM_LIBGCC) -o $@
	$(DM_SPRXLNK) $@

$(DM_SPRX): $(DM_ELF)
	python3 -c 'import struct; d=bytearray(open("$<","rb").read()); struct.pack_into(">H",d,16,0xFFA4); open("$@","wb").write(d)'
	@ls -l $@

clean-prx:
	rm -rf $(DM_OBJDIR) $(DM_AUTO_LD) $(DM_ELF) $(DM_SPRX)

clean: clean-prx

.PHONY: all clean clean-prx
