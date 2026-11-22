/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - audio engine API
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

#ifndef DM_AUDIO_ENGINE_H
#define DM_AUDIO_ENGINE_H

#include <stdint.h>

/*
 * Audio engine: owns the track queue, the decoder and the cellAudio port.
 *
 * Threading model (small, fixed footprint):
 *   - one audio thread (created by dm_engine_start) renders 256-frame float
 *     blocks into the cellAudio port;
 *   - any other thread may call dm_engine_command() and the queue functions.
 */

#define DM_MAX_TRACKS   1000u
#define DM_PATH_MAX     224u

/* Hotkey / control commands (bit mask, may be OR-ed). */
#define DM_CMD_NEXT       0x01u
#define DM_CMD_PREV       0x02u
#define DM_CMD_PAUSE      0x04u   /* toggle pause / resume */
#define DM_CMD_VOL_UP     0x08u
#define DM_CMD_VOL_DOWN   0x10u

#define DM_VOLUME_STEP    5

/* Queue replacement: begin -> add* -> commit.  Playback restarts from the
 * first track of the new list on commit.  begin/commit must be paired. */
void dm_engine_queue_begin(int volume, int repeat, int shuffle);
int  dm_engine_queue_add(const char *path);      /* 0 ok, -1 full / bad path */
void dm_engine_queue_commit(void);

/* Starts / stops the audio thread.  Stop is synchronous: it signals the
 * thread, joins it, stops and closes the cellAudio port and closes the file. */
int  dm_engine_start(void);
void dm_engine_stop(void);

void dm_engine_command(uint32_t cmd);

/* Introspection (used by the host test). */
int  dm_engine_volume(void);
int  dm_engine_paused(void);
int  dm_engine_track_index(void);   /* position in play order, -1 if none */

#endif /* DM_AUDIO_ENGINE_H */
