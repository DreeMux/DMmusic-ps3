/*
 * DMmusic - PS3 VSH background mp3 player plugin
 * Made by DreeMux — Made for PS3 gamers
 *
 * Copyright (C) DreeMux (https://github.com/DreeMux)
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * PRX module descriptor. The entry/exit bodies live in source/main.c.
 */

#include <stddef.h>
#include <lv2/prx.h>

#define DMMUSIC_COPYRIGHT "Made by DreeMux - Made for PS3 gamers (GPL-3.0-or-later)"

/* Engraved in the binary so it survives strip. */
const char dmmusic_copyright[] __attribute__((used)) = DMMUSIC_COPYRIGHT;

extern int dmmusic_start(size_t args, void *argp);
extern int dmmusic_stop(size_t args, void *argp);

SYS_MODULE_INFO(DMmusic, 0, 1, 0);
SYS_MODULE_START(dmmusic_start);
SYS_MODULE_STOP(dmmusic_stop);
