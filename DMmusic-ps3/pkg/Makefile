#---------------------------------------------------------------------------------
# DMmusic - Frontend (DMmusic.pkg) - PSL1GHT Makefile
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
# Usage:
#   make        -> builds DMmusic.elf / DMmusic.self
#   make pkg    -> builds the installable DMmusic.pkg (PARAM.SFO is generated
#                  from sfo.xml: TITLE_ID, APP_VER, CATEGORY=HG, ...)
#   make clean
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(PSL1GHT)),)
$(error "PSL1GHT is not set. Use the ps3dev/ps3dev Docker image or: export PSL1GHT=<path to PSL1GHT>")
endif

include $(PSL1GHT)/ppu_rules

#---------------------------------------------------------------------------------
# Project settings
#---------------------------------------------------------------------------------
TARGET		:=	DMmusic
BUILD		:=	build
SOURCES		:=	source
INCLUDES	:=	include

# Package metadata (keep TITLE_ID in sfo.xml in sync with APPID)
TITLE		:=	DMmusic
APPID		:=	DMMUS0001
CONTENTID	:=	UP0001-$(APPID)_00-DREEMUXDMMUSIC00
SFOXML		:=	sfo.xml
ICON0		:=	$(CURDIR)/ICON0.PNG

CFLAGS		=	-O2 -Wall -std=gnu99 -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS)
LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS		:=	-lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export BUILDDIR	:=	$(CURDIR)/$(BUILD)
export DEPSDIR	:=	$(BUILDDIR)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))

export OFILES	:=	$(CFILES:.c=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(LIBPSL1GHT_INC) \
					-I$(PSL1GHT)/ppu/include \
					$(if $(PORTLIBS),-I$(PORTLIBS)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(LIBPSL1GHT_LIB) \
					-L$(PSL1GHT)/ppu/lib \
					$(if $(PORTLIBS),-L$(PORTLIBS)/lib)

export LD		:=	$(CC)

.PHONY: $(BUILD) clean pkg run

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg

#---------------------------------------------------------------------------------
run:
	ps3load $(OUTPUT).self

#---------------------------------------------------------------------------------
pkg:	$(BUILD) $(OUTPUT).pkg

#---------------------------------------------------------------------------------
else

DEPENDS		:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf:	$(OFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
