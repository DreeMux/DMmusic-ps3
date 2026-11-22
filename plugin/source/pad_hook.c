/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - hotkey listener
 * Made by DreeMux — Made for PS3 gamers
 *
 * Copyright (C) 2026 DreeMux
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
 */

#include "pad_hook.h"
#include "audio_engine.h"

#define REPEAT_DELAY_TICKS    22u   /* ~350 ms before a held volume key repeats */
#define REPEAT_INTERVAL_TICKS  8u   /* ~128 ms between repeats */

static uint32_t g_prev;             /* combo bits active on the previous tick */
static uint32_t g_hold_up;
static uint32_t g_hold_left;

void dm_pad_reset(void)
{
    g_prev = 0;
    g_hold_up = 0;
    g_hold_left = 0;
}

uint32_t dm_pad_step(uint16_t d1, uint16_t d2)
{
    uint32_t now = 0, fired = 0, edge;
    int sel = (d1 & DM_PAD_SELECT) != 0;

    if (sel) {
        if (d2 & DM_PAD_R1)   now |= DM_CMD_NEXT;
        if (d2 & DM_PAD_L1)   now |= DM_CMD_PREV;
        if (d1 & DM_PAD_DOWN) now |= DM_CMD_PAUSE;
        if (d1 & DM_PAD_UP)   now |= DM_CMD_VOL_UP;
        if (d1 & DM_PAD_LEFT) now |= DM_CMD_VOL_DOWN;
    }

    edge = now & ~g_prev;           /* newly pressed combos only */
    fired = edge;

    /* auto-repeat for the volume keys only */
    if (now & DM_CMD_VOL_UP) {
        if (edge & DM_CMD_VOL_UP) g_hold_up = 0;
        else if (++g_hold_up >= REPEAT_DELAY_TICKS &&
                 ((g_hold_up - REPEAT_DELAY_TICKS) % REPEAT_INTERVAL_TICKS) == 0)
            fired |= DM_CMD_VOL_UP;
    } else {
        g_hold_up = 0;
    }
    if (now & DM_CMD_VOL_DOWN) {
        if (edge & DM_CMD_VOL_DOWN) g_hold_left = 0;
        else if (++g_hold_left >= REPEAT_DELAY_TICKS &&
                 ((g_hold_left - REPEAT_DELAY_TICKS) % REPEAT_INTERVAL_TICKS) == 0)
            fired |= DM_CMD_VOL_DOWN;
    } else {
        g_hold_left = 0;
    }

    g_prev = now;
    return fired;
}

uint32_t dm_pad_poll(void)
{
    dm_pad_data_t pd;

    pd.len = 0;
    if (cellPadGetData(0, &pd) != 0 || pd.len < 4) {
        g_prev = 0;                 /* no data: forget held keys */
        return 0;
    }
    return dm_pad_step(pd.button[2], pd.button[3]);
}
