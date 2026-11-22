/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - cellAudio engine, resampler, queue
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

#include "audio_engine.h"
#include "decoder.h"
#include "dm_sys.h"

#include <stddef.h>
#include <string.h>

/* cellAudio output format: 48 kHz, stereo float, 256 frames per block. */
#define OUT_RATE        48000u
#define OUT_CHANNELS    2u
#define BLOCK_FRAMES    256u
#define BLOCK_COUNT     8u
#define BLOCK_FLOATS    (BLOCK_FRAMES * OUT_CHANNELS)

/* Source staging buffer: 1024 stereo frames = 4 KB of disk data per refill,
 * plus one carry frame for the linear resampler. */
#define SRC_FRAMES      1024u

#define ENGINE_STACK    0x10000u   /* dr_mp3 needs ~30 KB of stack at peak */
#define ENGINE_PRIO     1001

typedef enum { ADV_AUTO = 0, ADV_NEXT, ADV_PREV } adv_t;

/* ------------------------------ queue ----------------------------------- */

static char      g_paths[DM_MAX_TRACKS][DM_PATH_MAX];
static uint16_t  g_order[DM_MAX_TRACKS];
static uint32_t  g_count;
static int       g_ord_pos = -1;
static int       g_repeat;
static int       g_shuffle;
static uint32_t  g_rng = 0x2545F491u;

static volatile int g_lock;                 /* spin lock around the queue */

static void lock(void)
{
    while (__sync_lock_test_and_set(&g_lock, 1))
        dm_sleep_ms(1);
}
static void unlock(void)
{
    __sync_lock_release(&g_lock);
}

static uint32_t rng_next(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static void rebuild_order(void)
{
    uint32_t i, j;
    uint16_t t;
    for (i = 0; i < g_count; i++) g_order[i] = (uint16_t)i;
    if (g_shuffle && g_count > 1) {
        for (i = g_count - 1; i > 0; i--) {
            j = rng_next() % (i + 1);
            t = g_order[i]; g_order[i] = g_order[j]; g_order[j] = t;
        }
    }
    g_ord_pos = -1;
}

/* ------------------------------ engine state ---------------------------- */

static volatile int      g_run;
static volatile uint32_t g_cmd;
static volatile int      g_reload;
static volatile int      g_paused;
static volatile int      g_volume = 80;
static volatile int      g_track_pos = -1;
static dm_thread_t       g_thread;
static int               g_thread_valid;

static int               g_have_track;
static int               g_idle;
static uint32_t          g_fail;
static float             g_gain;

/* cellAudio port */
static int               g_audio_we_inited;
static int               g_port_open;
static uint32_t          g_port;
static dm_audio_port_config_t g_cfg;

/* resampler state */
static int16_t           g_src[(SRC_FRAMES + 1u) * 2u];
static uint32_t          g_src_len;
static uint32_t          g_pos;          /* 16.16 index into g_src */
static uint32_t          g_step;         /* 16.16 source frames per output frame */
static int               g_src_eof;

static void update_gain(void)
{
    int v = g_volume;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    /* quadratic curve feels more natural than linear on a volume knob */
    g_gain = ((float)v * (float)v) / 10000.0f * (1.0f / 32768.0f);
}

/* ------------------------------ track control --------------------------- */

static void close_track(void)
{
    dm_dec_close();
    g_have_track = 0;
    g_src_len = 0;
    g_pos = 0;
    g_src_eof = 0;
}

/* Picks the next/previous entry; copies its path to out.
 * Returns 0 on success, -1 if the queue is empty, -2 if the list ended. */
static int pick_track(adv_t how, char *out)
{
    int pos;
    int rc = 0;

    lock();
    if (g_count == 0) {
        unlock();
        return -1;
    }
    if (how == ADV_AUTO && g_repeat == 2 && g_ord_pos >= 0) {
        pos = g_ord_pos;
    } else if (how == ADV_PREV) {
        pos = g_ord_pos - 1;
        if (pos < 0) pos = (int)g_count - 1;
    } else {
        pos = g_ord_pos + 1;
        if (pos >= (int)g_count) {
            if (how == ADV_AUTO && g_repeat == 0) {
                rc = -2;
                pos = -1;
            } else {
                pos = 0;
            }
        }
    }
    if (rc == 0) {
        g_ord_pos = pos;
        g_track_pos = pos;
        memcpy(out, g_paths[g_order[pos]], DM_PATH_MAX);
        out[DM_PATH_MAX - 1] = '\0';
    }
    unlock();
    return rc;
}

/* Opens the next playable track.  Returns 1 when a track is playing. */
static int start_track(adv_t how)
{
    char path[DM_PATH_MAX];
    uint32_t tries = 0;

    for (;;) {
        int rc = pick_track(how, path);
        if (rc == -1) { g_idle = 1; return 0; }
        if (rc == -2) { g_idle = 1; return 0; }          /* end of list, repeat off */

        if (dm_dec_open(path) == 0 && dm_dec_rate() != 0) {
            g_step = (uint32_t)((((uint64_t)dm_dec_rate()) << 16) / OUT_RATE);
            g_src_len = 0;
            g_pos = 0;
            g_src_eof = 0;
            g_have_track = 1;
            g_fail = 0;
            return 1;
        }
        /* unreadable or unsupported file: skip it, but never spin forever */
        lock();
        tries = g_count;
        unlock();
        if (++g_fail >= tries) { g_idle = 1; g_fail = 0; return 0; }
        if (how != ADV_PREV) how = ADV_NEXT;             /* keep walking, wrapping if needed */
    }
}

static void refill_source(void)
{
    uint32_t carry = 0, old = g_src_len;
    int n;

    if (old > 0) {                       /* keep the last frame for interpolation */
        g_src[0] = g_src[2u * (old - 1u)];
        g_src[1] = g_src[2u * (old - 1u) + 1u];
        carry = 1;
        g_pos -= (old - 1u) << 16;
    }
    n = dm_dec_read(g_src + 2u * carry, SRC_FRAMES);
    if (n <= 0) {
        n = 0;
        g_src_eof = 1;
    }
    g_src_len = carry + (uint32_t)n;
}

/* Produces one resampled stereo frame.  Returns 0 when nothing is playing. */
static int next_frame(int16_t *l, int16_t *r)
{
    for (;;) {
        uint32_t idx;

        if (!g_have_track) {
            if (g_idle || !start_track(ADV_AUTO))
                return 0;
        }

        idx = g_pos >> 16;
        while (idx + 1u >= g_src_len) {
            if (g_src_eof) break;
            refill_source();
            idx = g_pos >> 16;
        }
        if (idx + 1u >= g_src_len) {          /* end of this track */
            close_track();
            continue;
        }
        {
            int32_t frac = (int32_t)(g_pos & 0xFFFFu);
            int32_t a0 = g_src[2u * idx],      a1 = g_src[2u * (idx + 1u)];
            int32_t b0 = g_src[2u * idx + 1u], b1 = g_src[2u * (idx + 1u) + 1u];
            *l = (int16_t)(a0 + (((a1 - a0) * frac) >> 16));
            *r = (int16_t)(b0 + (((b1 - b0) * frac) >> 16));
        }
        g_pos += g_step;
        return 1;
    }
}

static void render_block(float *out)
{
    uint32_t i;
    if (g_paused) {
        for (i = 0; i < BLOCK_FLOATS; i++) out[i] = 0.0f;
        return;
    }
    for (i = 0; i < BLOCK_FRAMES; i++) {
        int16_t l, r;
        if (!next_frame(&l, &r)) {
            for (; i < BLOCK_FRAMES; i++) {
                out[2u * i] = 0.0f;
                out[2u * i + 1u] = 0.0f;
            }
            return;
        }
        out[2u * i]      = (float)l * g_gain;
        out[2u * i + 1u] = (float)r * g_gain;
    }
}

static void handle_commands(void)
{
    uint32_t c = __sync_lock_test_and_set(&g_cmd, 0u);
    int v;

    if (g_reload) {
        g_reload = 0;
        close_track();
        g_idle = 0;
        g_paused = 0;
        g_fail = 0;
        update_gain();
    }
    if (!c) return;

    if (c & DM_CMD_PAUSE) g_paused = !g_paused;
    if (c & (DM_CMD_VOL_UP | DM_CMD_VOL_DOWN)) {
        v = g_volume;
        if (c & DM_CMD_VOL_UP)   v += DM_VOLUME_STEP;
        if (c & DM_CMD_VOL_DOWN) v -= DM_VOLUME_STEP;
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        g_volume = v;
        update_gain();
    }
    if (c & DM_CMD_NEXT) {
        close_track();
        g_idle = 0;
        if (!start_track(ADV_NEXT)) { /* idle until reload */ }
    } else if (c & DM_CMD_PREV) {
        close_track();
        g_idle = 0;
        if (!start_track(ADV_PREV)) { /* idle until reload */ }
    }
}

/* ------------------------------ cellAudio ------------------------------- */

static int audio_open(void)
{
    dm_audio_port_param_t p;
    int32_t rc;

    rc = cellAudioInit();
    if (rc == 0) g_audio_we_inited = 1;
    else if ((uint32_t)rc != DM_AUDIO_ERR_ALREADY_INIT) return -1;

    p.nChannel = OUT_CHANNELS;
    p.nBlock   = BLOCK_COUNT;
    p.attr     = 0;
    p.level    = 1.0f;
    if (cellAudioPortOpen(&p, &g_port) != 0) goto fail;
    g_port_open = 1;
    memset(&g_cfg, 0, sizeof(g_cfg));
    if (cellAudioGetPortConfig(g_port, &g_cfg) != 0) goto fail;
    if (g_cfg.portAddr == 0 || g_cfg.readIndexAddr == 0) goto fail;
    return 0;

fail:
    if (g_port_open) { cellAudioPortClose(g_port); g_port_open = 0; }
    if (g_audio_we_inited) { cellAudioQuit(); g_audio_we_inited = 0; }
    return -1;
}

static void audio_close(void)
{
    if (g_port_open) {
        cellAudioPortStop(g_port);
        cellAudioPortClose(g_port);
        g_port_open = 0;
    }
    if (g_audio_we_inited) {
        cellAudioQuit();
        g_audio_we_inited = 0;
    }
}

static float *block_ptr(uint32_t blk)
{
    return (float *)(uintptr_t)(g_cfg.portAddr + blk * BLOCK_FLOATS * (uint32_t)sizeof(float));
}

static void engine_thread(uint64_t arg)
{
    uint32_t wr, b;
    (void)arg;

    /* VSH may not have its audio stack ready yet right after boot: retry. */
    while (g_run && audio_open() != 0)
        dm_sleep_ms(1000);

    if (g_run) {
        update_gain();
        for (b = 0; b < BLOCK_COUNT; b++) {
            handle_commands();
            render_block(block_ptr(b));
        }
        wr = 0;
        if (cellAudioPortStart(g_port) != 0) {
            g_run = 0;
        }
        while (g_run) {
            uint32_t rd = (uint32_t)(*(volatile uint64_t *)(uintptr_t)g_cfg.readIndexAddr);
            handle_commands();
            rd %= BLOCK_COUNT;
            while (wr != rd) {
                render_block(block_ptr(wr));
                wr = (wr + 1u) % BLOCK_COUNT;
            }
            dm_sleep_ms(2);
        }
    }

    close_track();
    audio_close();
    dm_thread_exit();
}

/* ------------------------------ public API ------------------------------ */

void dm_engine_queue_begin(int volume, int repeat, int shuffle)
{
    lock();
    g_count = 0;
    g_ord_pos = -1;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    g_volume = volume;
    g_repeat = (repeat < 0 || repeat > 2) ? 0 : repeat;
    g_shuffle = shuffle ? 1 : 0;
}

int dm_engine_queue_add(const char *path)
{
    size_t n;
    if (!path || g_count >= DM_MAX_TRACKS) return -1;
    n = strlen(path);
    if (n == 0 || n >= DM_PATH_MAX) return -1;
    memcpy(g_paths[g_count], path, n + 1u);
    g_count++;
    return 0;
}

void dm_engine_queue_commit(void)
{
    g_rng ^= g_count * 2654435761u;
    if (g_rng == 0) g_rng = 0x2545F491u;
    rebuild_order();
    g_track_pos = -1;
    g_reload = 1;
    unlock();
}

int dm_engine_start(void)
{
    if (g_thread_valid) return 0;
    g_run = 1;
    g_cmd = 0;
    if (dm_thread_create(&g_thread, engine_thread, 0, ENGINE_PRIO, ENGINE_STACK, "dmmusic_audio") != 0) {
        g_run = 0;
        return -1;
    }
    g_thread_valid = 1;
    return 0;
}

void dm_engine_stop(void)
{
    if (!g_thread_valid) return;
    g_run = 0;
    dm_thread_join(g_thread);
    g_thread_valid = 0;
}

void dm_engine_command(uint32_t cmd)
{
    __sync_fetch_and_or(&g_cmd, cmd);
}

int dm_engine_volume(void)      { return g_volume; }
int dm_engine_paused(void)      { return g_paused; }
int dm_engine_track_index(void) { return g_track_pos; }
