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

/* Engraved in the binary so it survives strip. */
const char dmmusic_copyright[] __attribute__((used)) = DMMUSIC_COPYRIGHT;

typedef struct {
    uint16_t modattribute;
    uint8_t  modversion[2];
    char     modname[27];
    uint8_t  terminal;
    void    *gp_value;
    void    *ent_top;
    void    *ent_end;
    void    *stub_top;
    void    *stub_end;
} _scemoduleinfo;

extern uint32_t __libc_start_libent __attribute__((weak));
extern uint32_t __libc_stop_libent  __attribute__((weak));
extern uint32_t __libc_start_libstub __attribute__((weak));
extern uint32_t __libc_stop_libstub  __attribute__((weak));

const _scemoduleinfo module_info
    __attribute__((section(".rodata.sceModuleInfo"), used)) = {
    0,
    { 1, 0 },
    "DMmusic",
    0,
    0,
    &__libc_start_libent,
    &__libc_stop_libent,
    &__libc_start_libstub,
    &__libc_stop_libstub
};
