/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - streaming decoder API
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

#ifndef DM_DECODER_H
#define DM_DECODER_H

#include <stdint.h>

/*
 * Streaming decoder front end: PCM 16-bit WAV (own parser) and MP3 (dr_mp3).
 * One track is open at a time; all memory is static (< 100 KB in total) and
 * the file is only ever read in DM_IO_CHUNK-byte pieces.
 *
 * Output is always interleaved stereo int16 at the track's native sample rate
 * (mono sources are duplicated to both channels).
 */

#define DM_IO_CHUNK 4096u

/* Opens a .wav or .mp3 (by extension, case-insensitive).  Returns 0 on
 * success, negative on error.  Any previously open track is closed first. */
int      dm_dec_open(const char *path);

/* Native sample rate of the open track in Hz (0 when nothing is open). */
uint32_t dm_dec_rate(void);

/* Decodes up to max_frames stereo frames into out (2 * max_frames int16).
 * Returns the number of frames produced, 0 at end of track, negative on a
 * read error. */
int      dm_dec_read(int16_t *out, uint32_t max_frames);

/* Closes the open track (safe to call when nothing is open). */
void     dm_dec_close(void);

#endif /* DM_DECODER_H */
