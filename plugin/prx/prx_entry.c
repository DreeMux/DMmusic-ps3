/*
 * DMmusic - PS3 VSH background mp3 player plugin
 * Made by DreeMux — Made for PS3 gamers
 *
 * Copyright (C) DreeMux (https://github.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://gnu.org>.
 *
 * PRX module descriptor (module_start / module_stop are in source/main.c).
 */

#include <stddef.h>
#include <stdint.h>

#define DMMUSIC_COPYRIGHT "Made by DreeMux - Made for PS3 gamers (GPL-3.0-or-later)"
const char dmmusic_copyright[] __attribute__((used)) = DMMUSIC_COPYRIGHT;

typedef struct {
    uint16_t modattribute;
    uint8_t  modversion;
    char     modname;
    uint8_t  terminal;
    void    *gp_value;
    void    *ent_top;
    void    *ent_end;
    void    *stub_top;
    void    *stub_end;
} _scemoduleinfo;

extern uint32_t _dm_ent_top;
extern uint32_t _dm_ent_end;
extern uint32_t _dm_stub_top;
extern uint32_t _dm_stub_end;

const _scemoduleinfo module_info
    __attribute__((section(".rodata.sceModuleInfo"), used)) = {
    0,
    { 1, 0 },
    "DMmusic",
    0,
    0,
    &_dm_ent_top,
    &_dm_ent_end,
    &_dm_stub_top,
    &_dm_stub_end
};

/* Standard 40-byte LV2 PRX parameter header (replaces lv2-sprx.o) */
typedef struct {
    uint32_t size;
    uint32_t magic;
    uint32_t version;
    uint32_t sdk_version;
    uint32_t module_info;
    uint32_t reserved;
} _sys_prx_param_t;

const _sys_prx_param_t _sys_proc_prx_param
    __attribute__((section(".sys_proc_prx_param"), used)) = {
    0x28,
    0x1B434CEC,
    0x00000001,
    0x00400001,
    0,
    { 0, 0, 0, 0, 0 }
};
