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
 * PRX module descriptor compatible with PSL1GHT lv2-sprx.o & sprxlinker.
 */

#include <stddef.h>
#include <stdint.h>

#define DMMUSIC_COPYRIGHT "Made by DreeMux - Made for PS3 gamers (GPL-3.0-or-later)"

/* Engraved in the binary so it survives strip. */
const char dmmusic_copyright[] __attribute__((used)) = DMMUSIC_COPYRIGHT;

extern int dmmusic_start(size_t args, void *argp);
extern int dmmusic_stop(size_t args, void *argp);

/* Standard PS3 PRX module_info structure expected by sprxlinker / lv2-sprx.o */
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

/* Entry points called by lv2-sprx.o */
int module_start(size_t args, void *argp)
{
    return dmmusic_start(args, argp);
}

int module_stop(size_t args, void *argp)
{
    return dmmusic_stop(args, argp);
}
