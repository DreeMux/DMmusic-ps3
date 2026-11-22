/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - WAV parser + dr_mp3 streaming wrapper
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

#include "decoder.h"
#include "dm_sys.h"

#include <stddef.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Tiny first-fit heap for dr_mp3 (no libc malloc inside the VSH process).   */
/* ------------------------------------------------------------------------ */

#define DM_HEAP_SIZE (64u * 1024u)

typedef struct dm_blk {
    uint32_t size;      /* payload bytes, multiple of 16 */
    uint32_t used;
    uint32_t pad[2];    /* keeps the payload 16-byte aligned */
} dm_blk_t;

static uint8_t g_heap[DM_HEAP_SIZE] __attribute__((aligned(16)));

static void heap_reset(void)
{
    dm_blk_t *b = (dm_blk_t *)g_heap;
    b->size = DM_HEAP_SIZE - (uint32_t)sizeof(dm_blk_t);
    b->used = 0;
}

static dm_blk_t *blk_next(dm_blk_t *b)
{
    uint8_t *n = (uint8_t *)b + sizeof(dm_blk_t) + b->size;
    return (n >= g_heap + DM_HEAP_SIZE) ? NULL : (dm_blk_t *)n;
}

void *dm_heap_alloc(size_t n)
{
    dm_blk_t *b = (dm_blk_t *)g_heap;
    if (n == 0) n = 1;
    n = (n + 15u) & ~(size_t)15u;
    while (b) {
        if (!b->used && b->size >= n) {
            if (b->size >= n + sizeof(dm_blk_t) + 16u) {
                dm_blk_t *r = (dm_blk_t *)((uint8_t *)b + sizeof(dm_blk_t) + n);
                r->size = b->size - (uint32_t)n - (uint32_t)sizeof(dm_blk_t);
                r->used = 0;
                b->size = (uint32_t)n;
            }
            b->used = 1;
            return (uint8_t *)b + sizeof(dm_blk_t);
        }
        b = blk_next(b);
    }
    return NULL;
}

void dm_heap_free(void *p)
{
    dm_blk_t *b, *n;
    if (!p) return;
    ((dm_blk_t *)((uint8_t *)p - sizeof(dm_blk_t)))->used = 0;
    /* coalesce every run of free blocks */
    b = (dm_blk_t *)g_heap;
    while (b) {
        n = blk_next(b);
        if (!b->used && n && !n->used)
            b->size += (uint32_t)sizeof(dm_blk_t) + n->size;
        else
            b = n;
    }
}

void *dm_heap_realloc(void *p, size_t n)
{
    dm_blk_t *old;
    void *q;
    if (!p) return dm_heap_alloc(n);
    old = (dm_blk_t *)((uint8_t *)p - sizeof(dm_blk_t));
    if (old->size >= n) return p;
    q = dm_heap_alloc(n);
    if (!q) return NULL;
    memcpy(q, p, old->size);
    dm_heap_free(p);
    return q;
}

/* ------------------------------------------------------------------------ */
/* dr_mp3 (fetched by CI into plugin/include/dr_mp3.h)                       */
/* ------------------------------------------------------------------------ */

#ifndef DM_NO_MP3
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DR_MP3_NO_SIMD
#define DR_MP3_ONLY_MP3
#define DRMP3_MALLOC(sz)        dm_heap_alloc(sz)
#define DRMP3_REALLOC(p, sz)    dm_heap_realloc((p), (sz))
#define DRMP3_FREE(p)           dm_heap_free(p)
#define DRMP3_ASSERT(expr)      ((void)0)
#include "dr_mp3.h"
#endif

/* ------------------------------------------------------------------------ */
/* Decoder state                                                             */
/* ------------------------------------------------------------------------ */

typedef enum { DEC_NONE = 0, DEC_WAV, DEC_MP3 } dec_kind_t;

static dec_kind_t g_kind;
static int        g_fd = -1;
static uint32_t   g_rate;

/* WAV */
static uint32_t   g_wav_left;      /* PCM bytes still to read from the file */
static uint32_t   g_wav_ch;        /* 1 or 2 */

#ifndef DM_NO_MP3
static drmp3      g_mp3;
static int        g_mp3_open;
#endif

/* Reads exactly n bytes (or until EOF) in DM_IO_CHUNK pieces. */
static int io_read_full(int fd, void *dst, uint32_t n, uint32_t *total)
{
    uint8_t *p = (uint8_t *)dst;
    uint32_t done = 0;
    while (done < n) {
        uint32_t want = n - done, got = 0;
        if (want > DM_IO_CHUNK) want = DM_IO_CHUNK;
        if (dm_fs_read(fd, p + done, want, &got) != 0) { *total = done; return -1; }
        if (got == 0) break;
        done += got;
    }
    *total = done;
    return 0;
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t le16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static int has_ext(const char *path, const char *ext3)
{
    size_t n = strlen(path);
    const char *e;
    int i;
    if (n < 4 || path[n - 4] != '.') return 0;
    e = path + n - 3;
    for (i = 0; i < 3; i++) {
        char c = e[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != ext3[i]) return 0;
    }
    return 1;
}

/* ------------------------------ WAV ------------------------------------- */

static int wav_open(void)
{
    uint8_t hdr[12], ck[8], fmt[40];
    uint32_t got, size, fmt_tag = 0, ch = 0, bits = 0, rate = 0;
    int have_fmt = 0;

    if (io_read_full(g_fd, hdr, 12, &got) != 0 || got != 12) return -1;
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return -1;

    for (;;) {
        if (io_read_full(g_fd, ck, 8, &got) != 0 || got != 8) return -1;
        size = le32(ck + 4);

        if (memcmp(ck, "fmt ", 4) == 0) {
            uint32_t take = size > sizeof(fmt) ? (uint32_t)sizeof(fmt) : size;
            if (take < 16) return -1;
            if (io_read_full(g_fd, fmt, take, &got) != 0 || got != take) return -1;
            fmt_tag = le16(fmt);
            ch      = le16(fmt + 2);
            rate    = le32(fmt + 4);
            bits    = le16(fmt + 14);
            /* WAVE_FORMAT_EXTENSIBLE: the real tag is the first two bytes of
             * the sub-format GUID at offset 24 (cbSize/valid bits/mask first). */
            if (fmt_tag == 0xFFFEu && take >= 26) fmt_tag = le16(fmt + 24);
            have_fmt = 1;
            if (size > take) {
                if (dm_fs_seek(g_fd, (int64_t)(size - take) + (size & 1u), DM_SEEK_CUR, NULL) != 0)
                    return -1;
            } else if (size & 1u) {
                if (dm_fs_seek(g_fd, 1, DM_SEEK_CUR, NULL) != 0) return -1;
            }
        } else if (memcmp(ck, "data", 4) == 0) {
            if (!have_fmt) return -1;
            if (fmt_tag != 1u || bits != 16u || (ch != 1u && ch != 2u) || rate < 8000u || rate > 192000u)
                return -1;
            g_wav_ch = ch;
            g_rate = rate;
            /* streamed / unknown length (0 or 0xFFFFFFFF): read until EOF */
            g_wav_left = (size == 0u) ? 0xFFFFFFFFu : size;
            return 0;
        } else {
            if (dm_fs_seek(g_fd, (int64_t)size + (size & 1u), DM_SEEK_CUR, NULL) != 0) return -1;
        }
    }
}

static int wav_read(int16_t *out, uint32_t max_frames)
{
    uint32_t bpf = 2u * g_wav_ch;              /* bytes per source frame */
    uint32_t want_bytes, got = 0, frames, i;
    uint8_t *raw = (uint8_t *)out;

    if (g_wav_left == 0 || max_frames == 0) return 0;
    want_bytes = max_frames * bpf;
    if (want_bytes > g_wav_left) want_bytes = g_wav_left - (g_wav_left % bpf);
    if (want_bytes == 0) { g_wav_left = 0; return 0; }

    if (io_read_full(g_fd, raw, want_bytes, &got) != 0) return -1;
    frames = got / bpf;
    if (g_wav_left != 0xFFFFFFFFu) g_wav_left -= frames * bpf;
    if (frames == 0) { g_wav_left = 0; return 0; }

    /* little-endian file data -> host int16 */
    for (i = 0; i < frames * g_wav_ch; i++) {
        uint16_t v = (uint16_t)(raw[2 * i] | (raw[2 * i + 1] << 8));
        ((int16_t *)raw)[i] = (int16_t)v;
    }
    if (g_wav_ch == 1) {                       /* expand mono in place, backwards */
        for (i = frames; i-- > 0;) {
            int16_t s = out[i];
            out[2 * i]     = s;
            out[2 * i + 1] = s;
        }
    }
    return (int)frames;
}

/* ------------------------------ MP3 ------------------------------------- */

#ifndef DM_NO_MP3

static size_t mp3_on_read(void *user, void *dst, size_t n)
{
    uint32_t total = 0;
    (void)user;
    if (io_read_full(g_fd, dst, (uint32_t)n, &total) != 0 && total == 0) return 0;
    return (size_t)total;
}

static drmp3_bool32 mp3_on_seek(void *user, int offset, drmp3_seek_origin origin)
{
    (void)user;
    return dm_fs_seek(g_fd, offset,
                      origin == drmp3_seek_origin_start ? DM_SEEK_SET : DM_SEEK_CUR, NULL) == 0
               ? DRMP3_TRUE : DRMP3_FALSE;
}

#if (DRMP3_VERSION_MAJOR > 0) || (DRMP3_VERSION_MINOR >= 7)
/* dr_mp3 0.7+ adds tell/metadata callbacks to drmp3_init(). */
static drmp3_bool32 mp3_on_tell(void *user, drmp3_int64 *cursor)
{
    uint64_t pos = 0;
    (void)user;
    if (dm_fs_seek(g_fd, 0, DM_SEEK_CUR, &pos) != 0) return DRMP3_FALSE;
    *cursor = (drmp3_int64)pos;
    return DRMP3_TRUE;
}
#endif

static int mp3_open(void)
{
#if (DRMP3_VERSION_MAJOR > 0) || (DRMP3_VERSION_MINOR >= 7)
    if (!drmp3_init(&g_mp3, mp3_on_read, mp3_on_seek, mp3_on_tell, NULL, NULL, NULL))
        return -1;
#else
    if (!drmp3_init(&g_mp3, mp3_on_read, mp3_on_seek, NULL, NULL))
        return -1;
#endif
    g_mp3_open = 1;
    if (g_mp3.channels < 1 || g_mp3.channels > 2 || g_mp3.sampleRate < 8000u)
        return -1;
    g_rate = g_mp3.sampleRate;
    return 0;
}

static int mp3_read(int16_t *out, uint32_t max_frames)
{
    uint32_t ch = g_mp3.channels, frames, i;
    frames = (uint32_t)drmp3_read_pcm_frames_s16(&g_mp3, max_frames, out);
    if (frames == 0) return 0;
    if (ch == 1) {
        for (i = frames; i-- > 0;) {
            int16_t s = out[i];
            out[2 * i]     = s;
            out[2 * i + 1] = s;
        }
    }
    return (int)frames;
}

#endif /* DM_NO_MP3 */

/* ------------------------------ public API ------------------------------ */

void dm_dec_close(void)
{
#ifndef DM_NO_MP3
    if (g_mp3_open) {
        drmp3_uninit(&g_mp3);
        g_mp3_open = 0;
    }
#endif
    if (g_fd >= 0) {
        dm_fs_close(g_fd);
        g_fd = -1;
    }
    g_kind = DEC_NONE;
    g_rate = 0;
    g_wav_left = 0;
}

int dm_dec_open(const char *path)
{
    int rc;

    dm_dec_close();
    heap_reset();

    if (dm_fs_open_ro(path, &g_fd) != 0) {
        g_fd = -1;
        return -1;
    }

    if (has_ext(path, "wav")) {
        rc = wav_open();
        g_kind = DEC_WAV;
    }
#ifndef DM_NO_MP3
    else if (has_ext(path, "mp3")) {
        rc = mp3_open();
        g_kind = DEC_MP3;
    }
#endif
    else {
        rc = -1;
    }

    if (rc != 0) {
        dm_dec_close();
        return -1;
    }
    return 0;
}

uint32_t dm_dec_rate(void)
{
    return g_rate;
}

int dm_dec_read(int16_t *out, uint32_t max_frames)
{
    switch (g_kind) {
    case DEC_WAV: return wav_read(out, max_frames);
#ifndef DM_NO_MP3
    case DEC_MP3: return mp3_read(out, max_frames);
#endif
    default:      return 0;
    }
}
