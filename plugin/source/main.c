/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - entry points, worker thread, queue watcher
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "audio_engine.h"
#include "decoder.h"
#include "dm_sys.h"
#include "pad_hook.h"

/*
 * Plugin entry points and background worker.
 *
 *   module_start : spawns the worker thread and returns immediately.
 *   module_stop  : clears g_running, joins the worker (which in turn stops the
 *                  audio thread, stops/closes the cellAudio port and closes the
 *                  open file) and returns STOP_OK, so a webMAN MOD unload or a
 *                  shutdown never leaves threads or audio ports behind.
 *
 * Queue file contract with DMmusic.pkg (see top-level README):
 *
 *     DMMUSIC_QUEUE_V1
 *     volume=<0..100>
 *     repeat=<0|1|2>        0 off, 1 all, 2 one
 *     shuffle=<0|1>
 *     count=<n>             informational; the track= lines are authoritative
 *     track=<absolute path>
 *
 * volume/repeat/shuffle must precede the first track= line (the frontend
 * always writes them in that order).
 */

#define DM_QUEUE_FILE    "/dev_hdd0/tmp/dmmusic_queue.txt"
#define DM_QUEUE_MAGIC   "DMMUSIC_QUEUE_V1"

#define DM_PRX_START_OK  0
#define DM_PRX_STOP_OK   0

#define WORKER_STACK     0x8000u
#define WORKER_PRIO      2000
#define BOOT_DELAY_MS    3000u
#define WATCH_TICKS      125u        /* 125 * 16 ms = 2 s between file checks */
#define LINE_MAX_LEN     (DM_PATH_MAX + 16u)

/* ------------------------------------------------------------------------ */
/* Chunked line reader (4 KB reads) that also fingerprints the file          */
/* ------------------------------------------------------------------------ */

typedef struct {
    int      fd;
    uint8_t  buf[DM_IO_CHUNK];
    uint32_t len;
    uint32_t pos;
    uint32_t hash;      /* FNV-1a over every byte read */
    uint64_t size;
} reader_t;

static int rd_open(reader_t *r, const char *path)
{
    r->len = r->pos = 0;
    r->hash = 2166136261u;
    r->size = 0;
    return dm_fs_open_ro(path, &r->fd);
}

static int rd_refill(reader_t *r)
{
    uint32_t got = 0;
    if (dm_fs_read(r->fd, r->buf, DM_IO_CHUNK, &got) != 0 || got == 0)
        return 0;
    r->len = got;
    r->pos = 0;
    return 1;
}

static int rd_byte(reader_t *r)
{
    uint8_t c;
    if (r->pos >= r->len && !rd_refill(r))
        return -1;
    c = r->buf[r->pos++];
    r->hash = (r->hash ^ (uint32_t)c) * 16777619u;
    r->size = r->size + 1u;
    return (int)c;
}

/* Reads one line (without the line ending).  Over-long lines are truncated.
 * Returns 1 when a line was read, 0 at end of file. */
static int rd_line(reader_t *r, char *line, uint32_t cap)
{
    uint32_t n = 0;
    int c, any = 0;
    for (;;) {
        c = rd_byte(r);
        if (c < 0) break;
        any = 1;
        if (c == '\n') break;
        if (c == '\r') continue;
        if (n + 1u < cap) line[n++] = (char)c;
    }
    line[n] = '\0';
    return any;
}

static int parse_int(const char *s, int lo, int hi, int dflt)
{
    int v = 0, any = 0;
    while (*s == ' ') s++;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        if (v > 100000) v = 100000;
        any = 1;
        s++;
    }
    if (!any) return dflt;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

static int starts_with(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

/* Fingerprint of the queue file (size + FNV-1a).  Returns 0 if unreadable.
 * Hashing the content instead of reading st_mtime keeps this independent of
 * the CellFsStat layout and also catches same-second rewrites. */
static int queue_signature(const char *path, uint64_t *size, uint32_t *hash)
{
    reader_t *r;
    static reader_t rd;
    r = &rd;
    if (rd_open(r, path) != 0) return 0;
    while (rd_byte(r) >= 0) { }
    dm_fs_close(r->fd);
    *size = r->size;
    *hash = r->hash;
    return 1;
}

/* Parses the queue file and hands it to the engine.  Returns 0 on success;
 * on failure the currently playing queue is left untouched. */
int dm_queue_load(const char *path)
{
    static reader_t rd;
    char line[LINE_MAX_LEN];
    int volume = 80, repeat = 0, shuffle = 0, begun = 0;

    if (rd_open(&rd, path) != 0) return -1;

    /* first non-empty line must be the magic */
    do {
        if (!rd_line(&rd, line, sizeof(line))) { dm_fs_close(rd.fd); return -1; }
    } while (line[0] == '\0');
    if (strlen(line) != strlen(DM_QUEUE_MAGIC) || !starts_with(line, DM_QUEUE_MAGIC)) {
        dm_fs_close(rd.fd);
        return -1;
    }

    while (rd_line(&rd, line, sizeof(line))) {
        if (starts_with(line, "volume="))       volume  = parse_int(line + 7, 0, 100, volume);
        else if (starts_with(line, "repeat="))  repeat  = parse_int(line + 7, 0, 2, repeat);
        else if (starts_with(line, "shuffle=")) shuffle = parse_int(line + 8, 0, 1, shuffle);
        else if (starts_with(line, "track=")) {
            if (!begun) {
                dm_engine_queue_begin(volume, repeat, shuffle);
                begun = 1;
            }
            if (line[6] != '\0')
                dm_engine_queue_add(line + 6);   /* a full queue just ignores the rest */
        }
    }
    dm_fs_close(rd.fd);

    if (!begun)
        dm_engine_queue_begin(volume, repeat, shuffle);   /* valid file, empty list */
    dm_engine_queue_commit();
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Worker thread                                                             */
/* ------------------------------------------------------------------------ */

static volatile int g_running;
static dm_thread_t  g_worker;
static int          g_worker_valid;

static int      g_sig_valid;
static uint64_t g_sig_size;
static uint32_t g_sig_hash;

/* Reloads the queue when the file's size or content fingerprint changed. */
static void queue_watch(void)
{
    uint64_t size = 0;
    uint32_t hash = 0;

    if (!queue_signature(DM_QUEUE_FILE, &size, &hash))
        return;                                  /* missing file: keep playing */
    if (g_sig_valid && size == g_sig_size && hash == g_sig_hash)
        return;
    g_sig_valid = 1;                             /* remember even on parse error */
    g_sig_size = size;
    g_sig_hash = hash;
    dm_queue_load(DM_QUEUE_FILE);
}

static void worker_thread(uint64_t arg)
{
    uint32_t i, tick = 0, cmds;
    (void)arg;

    dm_log("worker_thread: entered\n");

    /* Let the XMB finish booting before touching audio. */
    for (i = 0; i < BOOT_DELAY_MS / 100u && g_running; i++)
        dm_sleep_ms(100);

    if (g_running) {
        dm_log("worker_thread: starting engine\n");
        dm_pad_reset();
        dm_engine_start();
        queue_watch();
    }

    while (g_running) {
        cmds = dm_pad_poll();
        if (cmds)
            dm_engine_command(cmds);
        if (++tick >= WATCH_TICKS) {
            tick = 0;
            queue_watch();
        }
        dm_sleep_ms(DM_PAD_POLL_MS);
    }

    dm_engine_stop();            /* joins the audio thread, closes port + file */
    dm_thread_exit();
}

/* ------------------------------------------------------------------------ */
/* PRX entry points                                                          */
/* ------------------------------------------------------------------------ */

__attribute__((used))
int module_start(uint64_t args, void *argp)
{
    (void)args; (void)argp;
    dm_log("module_start: called\n");
    if (g_worker_valid)
        return DM_PRX_START_OK;
    g_running = 1;
    g_sig_valid = 0;
    if (dm_thread_create(&g_worker, worker_thread, 0, WORKER_PRIO, WORKER_STACK, "dmmusic_main") != 0) {
        dm_log("module_start: thread_create FAILED\n");
        g_running = 0;
        return DM_PRX_START_OK;      /* stay loadable; nothing is running */
    }
    dm_log("module_start: thread_create OK\n");
    g_worker_valid = 1;
    return DM_PRX_START_OK;
}

__attribute__((used))
int module_stop(uint64_t args, void *argp)
{
    (void)args; (void)argp;
    dm_log("module_stop: called\n");
    g_running = 0;
    if (g_worker_valid) {
        dm_thread_join(g_worker);
        g_worker_valid = 0;
    }
    return DM_PRX_STOP_OK;
}
