/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - host unit test
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

/*
 * Host unit test (x86 Linux).  Build and run:  make -C plugin host-test
 * Exercises the real decoder / resampler / engine / queue parser / pad state
 * machine with a fake cellAudio port and a fake "hardware" read thread.
 * MP3 is not covered here (dr_mp3 is fetched by CI); WAV is.
 */
#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "audio_engine.h"
#include "dm_sys.h"
#include "pad_hook.h"

int dm_queue_load(const char *path);

static int g_fail;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); g_fail++; } else printf("ok:   %s\n", msg); } while (0)

/* ----------------------------- fake cellAudio ---------------------------- */
#define NBLK 8
#define BFL  512
static float    *g_port_mem;
static uint64_t *g_read_idx;
static volatile int g_hw_run;
static pthread_t g_hw;
static int g_started;
static int g_quit_called, g_close_called, g_stop_called;

#define CAP_FRAMES (48000 * 30)
static float *g_cap;                      /* interleaved stereo */
static volatile long g_cap_n;             /* frames captured */

int32_t cellAudioInit(void) { return 0; }
int32_t cellAudioQuit(void) { g_quit_called++; return 0; }
int32_t cellAudioPortOpen(dm_audio_port_param_t *p, uint32_t *n)
{
    (void)p;
    g_port_mem = mmap(NULL, 65536, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    g_read_idx = (uint64_t *)((char *)g_port_mem + 32768);
    *g_read_idx = 0;
    *n = 0;
    return g_port_mem == MAP_FAILED ? -1 : 0;
}
int32_t cellAudioPortClose(uint32_t n) { (void)n; g_close_called++; return 0; }
int32_t cellAudioPortStop(uint32_t n) { (void)n; g_stop_called++; g_hw_run = 0; return 0; }
int32_t cellAudioGetPortConfig(uint32_t n, dm_audio_port_config_t *c)
{
    (void)n;
    memset(c, 0, sizeof(*c));
    c->readIndexAddr = (uint32_t)(uintptr_t)g_read_idx;
    c->portAddr = (uint32_t)(uintptr_t)g_port_mem;
    c->nChannel = 2; c->nBlock = NBLK; c->portSize = NBLK * BFL * 4;
    return 0;
}
static void *hw_thread(void *a)
{
    (void)a;
    while (g_hw_run) {
        uint64_t rd = *g_read_idx;
        float *blk = g_port_mem + rd * BFL;
        if (g_cap_n + 256 <= CAP_FRAMES) {
            memcpy(g_cap + g_cap_n * 2, blk, 256 * 2 * sizeof(float));
            g_cap_n += 256;
        }
        *g_read_idx = (rd + 1) % NBLK;
        usleep(1000);
    }
    return NULL;
}
int32_t cellAudioPortStart(uint32_t n)
{
    (void)n;
    g_hw_run = 1;
    g_started = 1;
    pthread_create(&g_hw, NULL, hw_thread, NULL);
    return 0;
}
int32_t cellPadGetData(uint32_t port, dm_pad_data_t *d) { (void)port; (void)d; return -1; }

/* ----------------------------- WAV writers ------------------------------- */
static void put32(FILE *f, uint32_t v) { fputc(v, f); fputc(v >> 8, f); fputc(v >> 16, f); fputc(v >> 24, f); }
static void put16(FILE *f, uint32_t v) { fputc(v, f); fputc(v >> 8, f); }
static void wav_begin(FILE *f, int ch, int rate, int frames)
{
    fwrite("RIFF", 1, 4, f); put32(f, 36 + frames * ch * 2); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, ch); put32(f, rate); put32(f, rate * ch * 2);
    put16(f, ch * 2); put16(f, 16);
    fwrite("LIST", 1, 4, f); /* extra chunk before data must be skipped: patch size below */
}
static void make_sine_mono_44k(const char *path)    /* 1 s, 440 Hz, amp 16000 */
{
    FILE *f = fopen(path, "wb");
    int i, n = 44100;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + 12 + n * 2); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, 44100); put32(f, 88200); put16(f, 2); put16(f, 16);
    fwrite("LIST", 1, 4, f); put32(f, 4); fwrite("INFO", 1, 4, f);   /* skipped chunk */
    fwrite("data", 1, 4, f); put32(f, n * 2);
    for (i = 0; i < n; i++) put16(f, (uint16_t)(int16_t)(16000.0 * sin(2 * M_PI * 440.0 * i / 44100.0)));
    fclose(f);
}
static void make_const_stereo_48k(const char *path, int l, int r, int frames)
{
    FILE *f = fopen(path, "wb");
    int i;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + frames * 4); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, 48000); put32(f, 192000); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4);
    for (i = 0; i < frames; i++) { put16(f, (uint16_t)(int16_t)l); put16(f, (uint16_t)(int16_t)r); }
    fclose(f);
}
static void write_queue(const char *path, int vol, int rep, int shuf, const char **tracks, int n)
{
    FILE *f = fopen(path, "wb");
    int i;
    fprintf(f, "DMMUSIC_QUEUE_V1\nvolume=%d\nrepeat=%d\nshuffle=%d\ncount=%d\n", vol, rep, shuf, n);
    for (i = 0; i < n; i++) fprintf(f, "track=%s\n", tracks[i]);
    fclose(f);
}

static float peak(long from, long to, int ch)
{
    float m = 0; long i;
    for (i = from; i < to && i < g_cap_n; i++) { float v = fabsf(g_cap[i * 2 + ch]); if (v > m) m = v; }
    return m;
}
static long first_nonzero(long from)
{
    long i;
    for (i = from; i < g_cap_n; i++) if (g_cap[i * 2] != 0 || g_cap[i * 2 + 1] != 0) return i;
    return -1;
}
static void wait_frames(long n) { while (g_cap_n < n) usleep(2000); }

int main(void)
{
    const char *tr[2] = { "/tmp/dm_t1.wav", "/tmp/dm_t2.wav" };
    long i, a, zc;
    int rc;

    g_cap = calloc(CAP_FRAMES, 2 * sizeof(float));

    /* ---- pad state machine ---- */
    {
        uint32_t fired = 0, total_next = 0, total_up = 0;
        dm_pad_reset();
        for (i = 0; i < 40; i++) {                                 /* hold SELECT+R1 */
            fired = dm_pad_step(DM_PAD_SELECT, DM_PAD_R1);
            if (fired & DM_CMD_NEXT) total_next++;
        }
        CHECK(total_next == 1, "SELECT+R1 held 40 ticks fires NEXT exactly once");
        dm_pad_step(0, 0);
        CHECK(dm_pad_step(DM_PAD_SELECT, DM_PAD_L1) == DM_CMD_PREV, "SELECT+L1 -> PREV");
        dm_pad_step(0, 0);
        CHECK(dm_pad_step(DM_PAD_SELECT | DM_PAD_DOWN, 0) == DM_CMD_PAUSE, "SELECT+DOWN -> PAUSE");
        dm_pad_step(0, 0);
        CHECK(dm_pad_step(DM_PAD_R1 | DM_PAD_L1, 0) == 0 && dm_pad_step(0, DM_PAD_R1) == 0, "no SELECT -> nothing");
        for (i = 0; i < 100; i++) if (dm_pad_step(DM_PAD_SELECT | DM_PAD_UP, 0) & DM_CMD_VOL_UP) total_up++;
        CHECK(total_up >= 8 && total_up <= 12, "SELECT+UP held ~1.6 s auto-repeats volume (edge + repeats)");
        dm_pad_step(0, 0);
        CHECK(dm_pad_step(DM_PAD_SELECT | DM_PAD_LEFT, 0) == DM_CMD_VOL_DOWN, "SELECT+LEFT -> VOL_DOWN");
        dm_pad_step(0, 0);
    }

    /* ---- queue + engine ---- */
    make_sine_mono_44k(tr[0]);
    make_const_stereo_48k(tr[1], 8000, -8000, 24000);
    write_queue("/tmp/dm_queue.txt", 80, 0, 0, tr, 2);
    (void)wav_begin;

    CHECK(dm_queue_load("/tmp/dm_queue.txt") == 0, "queue file parsed (DMMUSIC_QUEUE_V1)");
    CHECK(dm_engine_volume() == 80, "volume taken from queue file");
    rc = dm_engine_start();
    CHECK(rc == 0, "engine thread started");
    wait_frames(48000 * 3);

    a = first_nonzero(0);
    CHECK(a >= 0, "audio produced");
    {
        float exp1 = 16000.0f / 32768.0f * 0.64f;
        float p = peak(a, a + 40000, 0);
        CHECK(fabsf(p - exp1) < exp1 * 0.03f, "track 1 amplitude matches volume 80 (quadratic curve)");
        CHECK(fabsf(peak(a, a + 40000, 0) - peak(a, a + 40000, 1)) < 1e-6f, "mono duplicated to both channels");
        zc = 0;
        for (i = a + 1; i < a + 40000; i++)
            if ((g_cap[(i - 1) * 2] < 0) != (g_cap[i * 2] < 0)) zc++;
        /* 40000 frames at 48 kHz = 0.8333 s -> 440 Hz gives ~733 crossings */
        CHECK(zc > 725 && zc < 742, "44.1 kHz -> 48 kHz resample keeps pitch (440 Hz)");
        {
            float exp2 = 8000.0f / 32768.0f * 0.64f;
            long b = a + 48000 + 2000;      /* well inside track 2 */
            CHECK(fabsf(g_cap[b * 2] - exp2) < exp2 * 0.03f && fabsf(g_cap[b * 2 + 1] + exp2) < exp2 * 0.03f,
                  "track 2 (stereo 48 kHz) follows track 1, L/R preserved");
        }
        /* total of ~1.0 s + 0.5 s then silence (repeat off) */
        {
            long end = a + 48000 + 24000 + 2000;
            wait_frames(end + 4000);
            CHECK(peak(end, end + 3000, 0) == 0.0f, "silence after last track with repeat=off");
        }
    }

    /* ---- reload with repeat=all, then commands ---- */
    make_const_stereo_48k("/tmp/dm_t3.wav", 20000, 20000, 48000 * 4);
    {
        const char *t3[2] = { "/tmp/dm_t3.wav", "/tmp/dm_t2.wav" };
        long base;
        write_queue("/tmp/dm_queue.txt", 100, 1, 0, t3, 2);
        base = g_cap_n;
        CHECK(dm_queue_load("/tmp/dm_queue.txt") == 0, "changed queue file reloads live (no restart)");
        wait_frames(base + 48000);
        a = first_nonzero(base);
        CHECK(a >= 0 && fabsf(g_cap[(a + 5000) * 2] - 20000.0f / 32768.0f) < 0.01f, "new queue plays from its first track at volume 100");

        dm_engine_command(DM_CMD_VOL_DOWN);
        base = g_cap_n; wait_frames(base + 4000);
        {
            float exp = 20000.0f / 32768.0f * 0.9025f;          /* 95 % squared */
            CHECK(fabsf(g_cap[(g_cap_n - 100) * 2] - exp) < 0.01f && dm_engine_volume() == 95, "VOL_DOWN lowers volume by 5");
        }
        dm_engine_command(DM_CMD_PAUSE);
        base = g_cap_n; wait_frames(base + 6000);
        CHECK(peak(base + 3000, g_cap_n, 0) == 0.0f && dm_engine_paused(), "PAUSE silences output");
        dm_engine_command(DM_CMD_PAUSE);
        base = g_cap_n; wait_frames(base + 6000);
        CHECK(peak(base + 3000, g_cap_n, 0) > 0.1f && !dm_engine_paused(), "second PAUSE resumes");
        dm_engine_command(DM_CMD_NEXT);
        base = g_cap_n; wait_frames(base + 9000);
        CHECK(dm_engine_track_index() == 1 && fabsf(g_cap[(g_cap_n - 100) * 2] - 8000.0f / 32768.0f * 0.9025f) < 0.01f,
              "NEXT jumps to track 2");
        dm_engine_command(DM_CMD_PREV);
        base = g_cap_n; wait_frames(base + 9000);
        CHECK(dm_engine_track_index() == 0, "PREV jumps back to track 1");
    }

    /* ---- clean stop ---- */
    dm_engine_stop();
    CHECK(g_stop_called == 1 && g_close_called == 1 && g_quit_called == 1, "stop: port stopped+closed, cellAudioQuit called, thread joined");

    /* ---- bad inputs ---- */
    {
        FILE *f = fopen("/tmp/dm_bad.txt", "wb"); fputs("NOT_A_QUEUE\ntrack=/x.wav\n", f); fclose(f);
        CHECK(dm_queue_load("/tmp/dm_bad.txt") != 0, "queue without magic is rejected");
        CHECK(dm_queue_load("/tmp/does_not_exist.txt") != 0, "missing queue file is rejected");
    }

    printf(g_fail ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", g_fail);
    return g_fail != 0;
}
