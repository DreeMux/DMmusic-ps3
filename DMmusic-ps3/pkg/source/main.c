/*
 * DMmusic - Frontend (DMmusic.pkg) - entry point
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
 *
 * Queue file format (/dev_hdd0/tmp/dmmusic_queue.txt), UTF-8, "\n" endings:
 *
 *     DMMUSIC_QUEUE_V1
 *     volume=<0..100>
 *     repeat=<0|1|2>          0 = off, 1 = all, 2 = one
 *     shuffle=<0|1>
 *     count=<N>
 *     track=<absolute path>   (exactly N lines, in play order)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sys/process.h>
#include <sysutil/sysutil.h>

#include "scanner.h"
#include "ui.h"

#define DM_QUEUE_DIR       "/dev_hdd0/tmp"
#define DM_QUEUE_FILE      "/dev_hdd0/tmp/dmmusic_queue.txt"
#define DM_QUEUE_FILE_TMP  "/dev_hdd0/tmp/dmmusic_queue.txt.tmp"
#define DM_QUEUE_MAGIC     "DMMUSIC_QUEUE_V1"

/* Set from the sysutil callback when the system asks us to quit (PS button). */
static volatile int g_exit_requested = 0;

static void sysutil_callback(uint64_t status, uint64_t param, void *usrdata)
{
    (void)param;
    (void)usrdata;

    switch (status) {
    case SYSUTIL_EXIT_GAME:
        g_exit_requested = 1;
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Queue persistence                                                  */
/* ------------------------------------------------------------------ */

static int write_queue_to(const char *path, const dm_ui_state_t *ui, const dm_scan_t *scan)
{
    FILE *f = fopen(path, "wb");
    int i, err = 0;

    if (!f)
        return -1;

    if (fprintf(f, "%s\n", DM_QUEUE_MAGIC) < 0) err = 1;
    if (fprintf(f, "volume=%d\n", ui->volume) < 0) err = 1;
    if (fprintf(f, "repeat=%d\n", ui->repeat) < 0) err = 1;
    if (fprintf(f, "shuffle=%d\n", ui->shuffle ? 1 : 0) < 0) err = 1;
    if (fprintf(f, "count=%d\n", ui->queue_len) < 0) err = 1;

    for (i = 0; i < ui->queue_len && !err; i++) {
        int idx = ui->queue[i];
        if (idx < 0 || idx >= scan->count)
            continue;
        if (fprintf(f, "track=%s\n", scan->tracks[idx].path) < 0)
            err = 1;
    }

    if (fflush(f) != 0)
        err = 1;
    if (ferror(f))
        err = 1;
    if (fclose(f) != 0)
        err = 1;

    return err ? -1 : 0;
}

/* Writes the queue file; uses a temp file + rename when possible. */
static int save_queue(const dm_ui_state_t *ui, const dm_scan_t *scan)
{
    mkdir(DM_QUEUE_DIR, 0777); /* normally exists already; ignore errors */

    if (write_queue_to(DM_QUEUE_FILE_TMP, ui, scan) == 0) {
        unlink(DM_QUEUE_FILE);
        if (rename(DM_QUEUE_FILE_TMP, DM_QUEUE_FILE) == 0)
            return 0;
        unlink(DM_QUEUE_FILE_TMP);
    }

    /* Fallback: write the final file directly. */
    return write_queue_to(DM_QUEUE_FILE, ui, scan);
}

static void rstrip(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
        s[--n] = '\0';
}

/* Restores volume/repeat/shuffle and still-existing tracks from a previous run. */
static void load_previous(dm_ui_state_t *ui, const dm_scan_t *scan)
{
    FILE *f = fopen(DM_QUEUE_FILE, "rb");
    char line[DM_PATH_MAX + 16];
    int magic_ok = 0, restored = 0;

    if (!f)
        return;

    if (fgets(line, sizeof(line), f)) {
        rstrip(line);
        magic_ok = (strcmp(line, DM_QUEUE_MAGIC) == 0);
    }

    while (magic_ok && fgets(line, sizeof(line), f)) {
        rstrip(line);
        if (strncmp(line, "volume=", 7) == 0) {
            int v = atoi(line + 7);
            if (v < DM_VOLUME_MIN) v = DM_VOLUME_MIN;
            if (v > DM_VOLUME_MAX) v = DM_VOLUME_MAX;
            ui->volume = v;
        } else if (strncmp(line, "repeat=", 7) == 0) {
            int v = atoi(line + 7);
            ui->repeat = (v >= 0 && v <= 2) ? v : 0;
        } else if (strncmp(line, "shuffle=", 8) == 0) {
            ui->shuffle = atoi(line + 8) ? 1 : 0;
        } else if (strncmp(line, "track=", 6) == 0) {
            int i;
            for (i = 0; i < scan->count; i++) {
                if (strcmp(scan->tracks[i].path, line + 6) == 0) {
                    restored += dm_ui_queue_add(ui, i);
                    break;
                }
            }
        }
    }
    fclose(f);

    if (restored > 0)
        dm_ui_set_status(ui, DM_STATUS_INFO, "Restored %d queued track(s) from last session", restored);
}

/* ------------------------------------------------------------------ */
/* Scan progress callback: keeps the screen alive during long scans   */
/* ------------------------------------------------------------------ */

static int scan_progress(int found, const char *dir, void *user)
{
    (void)user;
    sysUtilCheckCallback();
    if (g_exit_requested)
        return 1; /* abort scan */
    dm_ui_draw_scanning(found, dir);
    return 0;
}

/* Shows a message for roughly `frames` frames while servicing the system. */
static void show_message(int frames, const char *title, const char *l1, const char *l2, int level)
{
    int i;
    for (i = 0; i < frames; i++) {
        sysUtilCheckCallback();
        if (g_exit_requested)
            break;
        dm_ui_draw_message(title, l1, l2, level);
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    dm_scan_t scan;
    dm_ui_state_t ui;
    int ui_ready = 0, state_ready = 0;
    int i;

    (void)argc;
    (void)argv;

    dm_scan_init(&scan);
    memset(&ui, 0, sizeof(ui));

    /* Must be registered first so the PS button / XMB quit is always handled. */
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_callback, NULL);

    /* 1. Bring up video + pads BEFORE anything slow (anti black-screen). */
    if (dm_ui_init() != 0) {
        dm_ui_shutdown();
        sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
        sysProcessExit(1);
        return 1;
    }
    ui_ready = 1;

    /* 2. Put something on screen right away. */
    for (i = 0; i < 3; i++) {
        sysUtilCheckCallback();
        dm_ui_draw_scanning(0, "");
    }

    /* 3. Scan the music folders (missing folders are fine). */
    if (!g_exit_requested) {
        if (dm_scan_all(&scan, scan_progress, NULL) < 0) {
            show_message(180, "Out of memory while scanning", "DMmusic will list the tracks found so far.",
                         NULL, DM_STATUS_BAD);
        }
    }

    /* 4. Interactive UI. */
    if (!g_exit_requested && dm_ui_state_init(&ui, scan.count) == 0) {
        dm_ui_action_t action = DM_ACTION_NONE;
        int save_done = 0;

        state_ready = 1;
        load_previous(&ui, &scan);

        if (scan.count == 0)
            dm_ui_set_status(&ui, DM_STATUS_WARN, "No tracks found in the MUSIC folders");
        else if (ui.status_ttl == 0)
            dm_ui_set_status(&ui, DM_STATUS_INFO, "Found %d track(s)", scan.count);

        while (!g_exit_requested && !save_done && action != DM_ACTION_CANCEL) {
            uint32_t pressed;

            sysUtilCheckCallback();            /* EVERY frame */
            if (g_exit_requested)
                break;

            pressed = dm_ui_poll_pad();
            action  = dm_ui_update(&ui, &scan, pressed);

            if (action == DM_ACTION_SAVE) {
                char l1[96];

                dm_ui_draw_message("Saving queue...", NULL, NULL, DM_STATUS_INFO);

                if (save_queue(&ui, &scan) == 0) {
                    snprintf(l1, sizeof(l1), "%d track(s) saved. Volume %d%%.", ui.queue_len, ui.volume);
                    show_message(75, "Queue saved", l1, "Returning to the XMB...", DM_STATUS_GOOD);
                    save_done = 1;
                } else {
                    dm_ui_set_status(&ui, DM_STATUS_BAD, "Could not write %s", DM_QUEUE_FILE);
                    action = DM_ACTION_NONE;      /* stay in the app */
                }
            }

            if (!save_done && action != DM_ACTION_CANCEL)
                dm_ui_draw_main(&ui, &scan);      /* renders + flips one frame */
        }
    }

    /* 5. Clean shutdown: release everything, then leave to the XMB. */
    if (state_ready)
        dm_ui_state_free(&ui);
    dm_scan_free(&scan);
    if (ui_ready)
        dm_ui_shutdown();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);

    sysProcessExit(0);
    return 0;
}
