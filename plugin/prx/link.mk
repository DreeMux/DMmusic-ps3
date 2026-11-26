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

PS3DEV   ?= /usr/local/ps3dev
PSL1GHT  ?= $(PS3DEV)
export PS3DEV PSL1GHT

DM_BIN     := $(PS3DEV)/bin
DM_PPUBIN  := $(PS3DEV)/ppu/bin
export PATH := $(DM_BIN):$(DM_PPUBIN):$(PATH)

DM_CC      := ppu-gcc
DM_LD      := ppu-ld
DM_SPRXLNK := $(DM_BIN)/sprxlinker
DM_FSELF   := $(DM_BIN)/fself
DM_MAKESELF:= $(DM_BIN)/make_self

DM_SRC     := $(wildcard source/*.c)
DM_OBJDIR  := prx/obj
DM_OBJS    := $(patsubst source/%.c,$(DM_OBJDIR)/%.o,$(DM_SRC))
DM_PRXOBJS := $(DM_OBJDIR)/prx_entry.o $(DM_OBJDIR)/vsh_stubs.o
DM_LIBGCC  := $(shell $(DM_CC) -print-libgcc-file-name)

DM_CFLAGS  := -O2 -mcpu=cell -ffunction-sections -fdata-sections \
              -fno-builtin -fno-stack-protector -Wall \
              -Iinclude -I$(PSL1GHT)/ppu/include
DM_ASFLAGS := -mcpu=cell -Iinclude -I$(PSL1GHT)/ppu/include
DM_LDFLAGS := -q -T prx/prx.ld -e module_start

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
	$(DM_LD) $(DM_LDFLAGS) $(DM_OBJS) $(DM_PRXOBJS) $(DM_LIBGCC) -o $@
	$(DM_SPRXLNK) $@

$(DM_SPRX): $(DM_ELF)
	python3 -c 'import struct; d=bytearray(open("$<","rb").read()); struct.pack_into(">H",d,16,0xFFA4); open("$@","wb").write(d)'
	@ls -l $@

clean-prx:
	rm -rf $(DM_OBJDIR) $(DM_ELF) $(DM_SPRX)

clean: clean-prx

.PHONY: all clean clean-prx
