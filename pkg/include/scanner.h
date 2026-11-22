/*
 * DMmusic - Frontend (DMmusic.pkg) - music library scanner (header)
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

#ifndef DMMUSIC_SCANNER_H
#define DMMUSIC_SCANNER_H

#ifdef __cplusplus
extern "C" {
#endif

#define DM_PATH_MAX          512   /* max length of a full track path        */
#define DM_SCAN_MAX_DEPTH    8     /* max directory recursion depth          */
#define DM_SCAN_MAX_TRACKS   4096  /* hard cap on number of tracks           */
#define DM_SCAN_ROOT_COUNT   3     /* number of music root folders           */

typedef struct dm_track {
    char path[DM_PATH_MAX];  /* absolute path, e.g. /dev_hdd0/MUSIC/a.mp3    */
    int  name_off;           /* offset of the file name inside path          */
    int  root;               /* index of the root folder it was found in     */
} dm_track_t;

typedef struct dm_scan {
    dm_track_t *tracks;                      /* sorted track array (heap)    */
    int         count;                       /* number of valid tracks       */
    int         capacity;                    /* allocated entries            */
    int         truncated;                   /* 1 if DM_SCAN_MAX_TRACKS hit  */
    int         aborted;                     /* 1 if progress cb aborted     */
    int         dirs_visited;                /* statistics                   */
    int         root_present[DM_SCAN_ROOT_COUNT]; /* 1 if root dir opened    */
    unsigned    tick;                        /* internal progress counter    */
} dm_scan_t;

/*
 * Progress callback. Called regularly while scanning so the caller can keep
 * the screen alive and service system callbacks. Return non-zero to abort.
 */
typedef int (*dm_scan_progress_fn)(int found, const char *current_dir, void *user);

/* Returns the path of root folder i (0 <= i < DM_SCAN_ROOT_COUNT). */
const char *dm_scan_root_path(int i);

/* Zero-initialises a scan result. */
void dm_scan_init(dm_scan_t *scan);

/*
 * Recursively scans every root folder for .mp3 / .wav files. Missing roots are
 * not an error. Results are sorted by path (case-insensitive).
 * Returns the number of tracks found (>= 0) or -1 on out-of-memory.
 */
int dm_scan_all(dm_scan_t *scan, dm_scan_progress_fn progress, void *user);

/* Frees all memory owned by the scan result. */
void dm_scan_free(dm_scan_t *scan);

/* Returns the file-name part of a track. */
const char *dm_track_name(const dm_track_t *track);

#ifdef __cplusplus
}
#endif

#endif /* DMMUSIC_SCANNER_H */
