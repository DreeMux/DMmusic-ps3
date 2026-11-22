/*
 * DMmusic - Frontend (DMmusic.pkg) - music library scanner
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "scanner.h"

static const char *const k_roots[DM_SCAN_ROOT_COUNT] = {
    "/dev_hdd0/MUSIC",
    "/dev_usb000/MUSIC",
    "/dev_usb001/MUSIC"
};

/* scan_dir() result codes */
#define SCAN_OK      0
#define SCAN_ABORT   1  /* progress callback asked to stop           */
#define SCAN_FULL    2  /* track cap reached                         */
#define SCAN_NOMEM   3  /* out of memory                             */

const char *dm_scan_root_path(int i)
{
    if (i < 0 || i >= DM_SCAN_ROOT_COUNT)
        return "";
    return k_roots[i];
}

void dm_scan_init(dm_scan_t *scan)
{
    if (scan)
        memset(scan, 0, sizeof(*scan));
}

void dm_scan_free(dm_scan_t *scan)
{
    if (!scan)
        return;
    free(scan->tracks);
    memset(scan, 0, sizeof(*scan));
}

const char *dm_track_name(const dm_track_t *track)
{
    if (!track)
        return "";
    return track->path + track->name_off;
}

static int has_audio_ext(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot)
        return 0;
    return (strcasecmp(dot, ".mp3") == 0) || (strcasecmp(dot, ".wav") == 0);
}

static int add_track(dm_scan_t *scan, const char *path, int name_off, int root)
{
    dm_track_t *t;

    if (scan->count >= DM_SCAN_MAX_TRACKS) {
        scan->truncated = 1;
        return SCAN_FULL;
    }

    if (scan->count >= scan->capacity) {
        int new_cap = scan->capacity ? scan->capacity * 2 : 256;
        dm_track_t *p;
        if (new_cap > DM_SCAN_MAX_TRACKS)
            new_cap = DM_SCAN_MAX_TRACKS;
        p = (dm_track_t *)realloc(scan->tracks, (size_t)new_cap * sizeof(dm_track_t));
        if (!p)
            return SCAN_NOMEM;
        scan->tracks = p;
        scan->capacity = new_cap;
    }

    t = &scan->tracks[scan->count];
    memset(t, 0, sizeof(*t));
    strncpy(t->path, path, DM_PATH_MAX - 1);
    t->path[DM_PATH_MAX - 1] = '\0';
    t->name_off = name_off;
    t->root = root;
    scan->count++;
    return SCAN_OK;
}

static int cmp_tracks(const void *a, const void *b)
{
    const dm_track_t *ta = (const dm_track_t *)a;
    const dm_track_t *tb = (const dm_track_t *)b;
    int r = strcasecmp(ta->path, tb->path);
    if (r == 0)
        r = strcmp(ta->path, tb->path);
    return r;
}

static int scan_dir(dm_scan_t *scan, const char *dir, int root, int depth,
                    dm_scan_progress_fn progress, void *user)
{
    DIR *d;
    struct dirent *e;
    int rc = SCAN_OK;

    d = opendir(dir);
    if (!d)
        return SCAN_OK; /* unreadable / missing directory: skip silently */

    scan->dirs_visited++;

    while ((e = readdir(d)) != NULL) {
        char path[DM_PATH_MAX];
        struct stat st;
        int n;
        const char *name = e->d_name;

        /* skip ".", "..", hidden files and macOS "._" resource forks */
        if (name[0] == '\0' || name[0] == '.')
            continue;

        n = snprintf(path, sizeof(path), "%s/%s", dir, name);
        if (n < 0 || n >= (int)sizeof(path))
            continue; /* path too long */

        scan->tick++;
        if (progress && (scan->tick & 63u) == 0u) {
            if (progress(scan->count, dir, user)) {
                scan->aborted = 1;
                rc = SCAN_ABORT;
                break;
            }
        }

        memset(&st, 0, sizeof(st));
        if (stat(path, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode)) {
            if (depth < DM_SCAN_MAX_DEPTH) {
                rc = scan_dir(scan, path, root, depth + 1, progress, user);
                if (rc != SCAN_OK)
                    break;
            }
        } else if (has_audio_ext(name)) {
            int name_off = (int)(strlen(dir) + 1);
            rc = add_track(scan, path, name_off, root);
            if (rc != SCAN_OK)
                break;
        }
    }

    closedir(d);
    return rc;
}

int dm_scan_all(dm_scan_t *scan, dm_scan_progress_fn progress, void *user)
{
    int i;
    int rc = SCAN_OK;

    if (!scan)
        return -1;

    for (i = 0; i < DM_SCAN_ROOT_COUNT; i++) {
        DIR *probe = opendir(k_roots[i]);
        if (!probe) {
            scan->root_present[i] = 0;
            continue;
        }
        closedir(probe);
        scan->root_present[i] = 1;

        if (progress && progress(scan->count, k_roots[i], user)) {
            scan->aborted = 1;
            rc = SCAN_ABORT;
            break;
        }

        rc = scan_dir(scan, k_roots[i], i, 0, progress, user);
        if (rc == SCAN_NOMEM)
            return -1;
        if (rc != SCAN_OK)
            break; /* abort or track cap */
    }

    if (scan->count > 1)
        qsort(scan->tracks, (size_t)scan->count, sizeof(dm_track_t), cmp_tracks);

    return scan->count;
}
