/*
 * DMmusic - Frontend (DMmusic.pkg) - video, pad input and user interface
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
 * Rendering design
 * ----------------
 * Only PSL1GHT's own libraries are used (librsx, libgcm_sys, libio,
 * libsysutil): a double-buffered XRGB framebuffer is allocated in RSX local
 * memory, drawn on by the PPU and flipped with gcmSetFlip() every frame. The
 * 8x8 bitmap font below is the public-domain "font8x8_basic" set by Daniel
 * Hepper (ASCII 32..126) plus one extra glyph for the em dash used in the
 * branding tagline. No external font or Tiny3D dependency is required.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>

#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#include "ui.h"

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define DM_CB_SIZE      0x100000            /* RSX command buffer size    */
#define DM_HOST_SIZE    (32 * 1024 * 1024)  /* RSX host (IO) memory       */
#define DM_LABEL_INDEX  255                 /* GCM label used for idle    */
#define DM_FOOTER_ROWS  5                   /* status,path,3 control rows */

#define COL_BG        0x000E1220u
#define COL_HEADER    0x001B2A5Cu
#define COL_ACCENT    0x004FC3F7u
#define COL_TEXT      0x00E8ECF4u
#define COL_DIM       0x008892A8u
#define COL_LINE      0x002A3350u
#define COL_SEL_BG    0x002B3C7Au
#define COL_GOOD      0x0066E08Au
#define COL_WARN      0x00FFC857u
#define COL_BAD       0x00FF6B6Bu
#define COL_QUEUED    0x00B388FFu

/* ------------------------------------------------------------------ */
/* 8x8 bitmap font: index 0..94 = ASCII 32..126, index 95 = em dash.   */
/* Each byte is one row, bit 0 is the left-most pixel.                */
/* ------------------------------------------------------------------ */

static const uint8_t k_font[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* '!' */
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, /* '"' */
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, /* '#' */
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, /* '$' */
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, /* '%' */
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, /* '&' */
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, /* '\'' */
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, /* '(' */
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, /* ')' */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* '*' */
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, /* '+' */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, /* ',' */
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, /* '-' */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, /* '.' */
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, /* '/' */
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, /* '0' */
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, /* '1' */
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, /* '2' */
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, /* '3' */
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, /* '4' */
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, /* '5' */
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, /* '6' */
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, /* '7' */
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, /* '8' */
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, /* '9' */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, /* ':' */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, /* ';' */
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, /* '<' */
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, /* '=' */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* '>' */
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, /* '?' */
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, /* '@' */
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, /* 'A' */
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, /* 'B' */
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, /* 'C' */
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, /* 'D' */
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, /* 'E' */
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, /* 'F' */
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, /* 'G' */
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, /* 'H' */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 'I' */
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, /* 'J' */
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, /* 'K' */
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, /* 'L' */
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, /* 'M' */
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, /* 'N' */
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, /* 'O' */
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, /* 'P' */
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, /* 'Q' */
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, /* 'R' */
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, /* 'S' */
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 'T' */
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, /* 'U' */
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* 'V' */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* 'W' */
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, /* 'X' */
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, /* 'Y' */
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, /* 'Z' */
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, /* '[' */
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* '\\' */
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, /* ']' */
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, /* '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* '_' */
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, /* '`' */
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, /* 'a' */
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, /* 'b' */
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, /* 'c' */
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, /* 'd' */
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, /* 'e' */
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, /* 'f' */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, /* 'g' */
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, /* 'h' */
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, /* 'i' */
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, /* 'j' */
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, /* 'k' */
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* 'l' */
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, /* 'm' */
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, /* 'n' */
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, /* 'o' */
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, /* 'p' */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, /* 'q' */
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, /* 'r' */
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, /* 's' */
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, /* 't' */
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, /* 'u' */
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* 'v' */
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* 'w' */
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, /* 'x' */
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, /* 'y' */
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, /* 'z' */
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, /* '{' */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* '|' */
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, /* '}' */
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, /* '~' */
    {0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00}  /* em dash (index 95) */
};

#define GLYPH_QMARK  ('?' - 32)
#define GLYPH_EMDASH 95

/* ------------------------------------------------------------------ */
/* Video state                                                        */
/* ------------------------------------------------------------------ */

typedef struct dm_fb {
    uint32_t *ptr;
    uint32_t  offset;
} dm_fb_t;

static gcmContextData *g_ctx        = NULL;
static void           *g_host       = NULL;
static dm_fb_t         g_fb[2];
static int             g_cur        = 0;
static int             g_width      = 1280;
static int             g_height     = 720;
static int             g_scale      = 2;   /* font pixel scale             */
static int             g_cols       = 80;  /* text cells across            */
static int             g_rows       = 45;  /* text cells down              */
static int             g_mx         = 5;   /* horizontal safe margin cells */
static int             g_my         = 3;   /* vertical safe margin cells   */
static int             g_gfx_ready  = 0;
static int             g_pad_ready  = 0;
static uint32_t        g_label_val  = 1;
static uint32_t        g_anim       = 0;   /* animation counter (spinner)  */

#define CELL_W (8 * g_scale)
#define CELL_H (9 * g_scale)   /* 8 px glyph + 1 px line spacing */

/* ------------------------------------------------------------------ */
/* RSX helpers                                                        */
/* ------------------------------------------------------------------ */

static void wait_rsx_idle(void)
{
    int guard = 0;

    if (!g_ctx)
        return;

    rsxSetWriteBackendLabel(g_ctx, DM_LABEL_INDEX, g_label_val);
    rsxFlushBuffer(g_ctx);

    while (*(volatile uint32_t *)gcmGetLabelAddress(DM_LABEL_INDEX) != g_label_val) {
        usleep(30);
        if (++guard > 200000) /* ~6 s safety net: never hang forever */
            break;
    }
    g_label_val++;
}

static void wait_flip(void)
{
    int guard = 0;

    while (gcmGetFlipStatus() != 0) {
        usleep(200);
        if (++guard > 100000) /* ~20 s safety net */
            break;
    }
    gcmResetFlipStatus();
}

static int do_flip(int buffer_id)
{
    if (gcmSetFlip(g_ctx, (uint8_t)buffer_id) == 0) {
        rsxFlushBuffer(g_ctx);
        gcmSetWaitFlip(g_ctx);
        return 1;
    }
    return 0;
}

static void fb_fill_all(const dm_fb_t *fb, uint32_t color)
{
    size_t i, n = (size_t)g_width * (size_t)g_height;
    for (i = 0; i < n; i++)
        fb->ptr[i] = color;
}

static int make_fb(dm_fb_t *fb, int id)
{
    uint32_t pitch = (uint32_t)g_width * 4u;
    uint32_t size  = pitch * (uint32_t)g_height;

    fb->ptr = (uint32_t *)rsxMemalign(64, size);
    if (!fb->ptr)
        return -1;
    if (rsxAddressToOffset(fb->ptr, &fb->offset) != 0)
        return -1;
    if (gcmSetDisplayBuffer((uint8_t)id, fb->offset, pitch,
                            (uint32_t)g_width, (uint32_t)g_height) != 0)
        return -1;

    fb_fill_all(fb, COL_BG);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public: init / shutdown                                            */
/* ------------------------------------------------------------------ */

int dm_ui_init(void)
{
    videoState      state;
    videoResolution res;
    videoConfiguration vconfig;
    int resolution_id = VIDEO_RESOLUTION_720;
    int aspect        = VIDEO_ASPECT_AUTO;
    int have_res      = 0;

    memset(&state, 0, sizeof(state));
    memset(&res, 0, sizeof(res));

    /* Pads first: cheap, and independent of video. */
    if (ioPadInit(MAX_PADS) == 0)
        g_pad_ready = 1;

    /* RSX / GCM initialisation */
    g_host = memalign(1024 * 1024, DM_HOST_SIZE);
    if (!g_host)
        return -1;

    rsxInit(&g_ctx, DM_CB_SIZE, DM_HOST_SIZE, g_host);
    if (!g_ctx)
        return -2;

    /* Query the current display mode; fall back to 720p if unavailable. */
    if (videoGetState(0, 0, &state) == 0 && state.displayMode.resolution != 0) {
        if (videoGetResolution(state.displayMode.resolution, &res) == 0 &&
            res.width > 0 && res.height > 0) {
            resolution_id = state.displayMode.resolution;
            aspect        = state.displayMode.aspect;
            have_res      = 1;
        }
    }
    if (!have_res) {
        if (videoGetResolution(VIDEO_RESOLUTION_720, &res) != 0 ||
            res.width == 0 || res.height == 0) {
            res.width  = 1280;
            res.height = 720;
        }
        resolution_id = VIDEO_RESOLUTION_720;
        aspect        = VIDEO_ASPECT_AUTO;
    }

    g_width  = res.width;
    g_height = res.height;

    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = resolution_id;
    vconfig.format     = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch      = (uint32_t)g_width * 4u;
    vconfig.aspect     = aspect;

    wait_rsx_idle();

    if (videoConfigure(0, &vconfig, NULL, 0) != 0)
        return -3;

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    /* Text grid derived from the real resolution. */
    g_scale = (g_height >= 1000) ? 3 : ((g_height >= 700) ? 2 : 1);
    g_cols  = g_width  / (8 * g_scale);
    g_rows  = g_height / (9 * g_scale);
    g_mx    = g_cols / 20 + 1;
    g_my    = g_rows / 20 + 1;

    if (make_fb(&g_fb[0], 0) != 0)
        return -4;
    if (make_fb(&g_fb[1], 1) != 0)
        return -5;

    gcmResetFlipStatus();
    do_flip(1);          /* show a clean (dark) buffer immediately */
    g_cur = 0;
    g_gfx_ready = 1;
    return 0;
}

void dm_ui_shutdown(void)
{
    int i;

    if (g_pad_ready) {
        ioPadEnd();
        g_pad_ready = 0;
    }

    if (g_ctx) {
        if (g_gfx_ready)
            wait_flip();
        wait_rsx_idle();
        for (i = 0; i < 2; i++) {
            if (g_fb[i].ptr) {
                rsxFree(g_fb[i].ptr);
                g_fb[i].ptr = NULL;
            }
        }
    }
    g_gfx_ready = 0;
}

/* ------------------------------------------------------------------ */
/* Pad input                                                          */
/* ------------------------------------------------------------------ */

#define REPEAT_DELAY   20   /* frames before auto-repeat starts */
#define REPEAT_RATE    4    /* frames between repeats           */

uint32_t dm_ui_poll_pad(void)
{
    static uint32_t prev = 0;
    static int hold[4] = {0, 0, 0, 0};
    static const uint32_t dpad[4] = {DM_BTN_UP, DM_BTN_DOWN, DM_BTN_LEFT, DM_BTN_RIGHT};
    padInfo info;
    padData data;
    uint32_t held = 0, pressed;
    int i;

    if (!g_pad_ready)
        return 0;

    memset(&info, 0, sizeof(info));
    ioPadGetInfo(&info);

    for (i = 0; i < MAX_PADS; i++) {
        if (!info.status[i])
            continue;
        memset(&data, 0, sizeof(data));
        if (ioPadGetData(i, &data) != 0 || data.len <= 0)
            continue;

        if (data.BTN_UP)       held |= DM_BTN_UP;
        if (data.BTN_DOWN)     held |= DM_BTN_DOWN;
        if (data.BTN_LEFT)     held |= DM_BTN_LEFT;
        if (data.BTN_RIGHT)    held |= DM_BTN_RIGHT;
        if (data.BTN_CROSS)    held |= DM_BTN_CROSS;
        if (data.BTN_CIRCLE)   held |= DM_BTN_CIRCLE;
        if (data.BTN_TRIANGLE) held |= DM_BTN_TRIANGLE;
        if (data.BTN_SQUARE)   held |= DM_BTN_SQUARE;
        if (data.BTN_START)    held |= DM_BTN_START;
        if (data.BTN_SELECT)   held |= DM_BTN_SELECT;
        if (data.BTN_L1)       held |= DM_BTN_L1;
        if (data.BTN_R1)       held |= DM_BTN_R1;
        if (data.BTN_L2)       held |= DM_BTN_L2;
        if (data.BTN_R2)       held |= DM_BTN_R2;
    }

    pressed = held & ~prev;

    for (i = 0; i < 4; i++) {
        if (held & dpad[i]) {
            hold[i]++;
            if (hold[i] > REPEAT_DELAY && ((hold[i] - REPEAT_DELAY) % REPEAT_RATE) == 0)
                pressed |= dpad[i];
        } else {
            hold[i] = 0;
        }
    }

    prev = held;
    return pressed;
}

/* ------------------------------------------------------------------ */
/* Drawing primitives                                                 */
/* ------------------------------------------------------------------ */

static void fb_rect(int x, int y, int w, int h, uint32_t color)
{
    int x1, y1, yy, xx;
    uint32_t *row;

    if (w <= 0 || h <= 0)
        return;
    x1 = x + w;
    y1 = y + h;
    if (x < 0)        x = 0;
    if (y < 0)        y = 0;
    if (x1 > g_width)  x1 = g_width;
    if (y1 > g_height) y1 = g_height;
    if (x >= x1 || y >= y1)
        return;

    for (yy = y; yy < y1; yy++) {
        row = g_fb[g_cur].ptr + (size_t)yy * (size_t)g_width;
        for (xx = x; xx < x1; xx++)
            row[xx] = color;
    }
}

static void draw_glyph(int px, int py, int glyph, uint32_t color)
{
    const uint8_t *rows;
    int r, c, start;

    if (glyph < 0 || glyph > 95)
        glyph = GLYPH_QMARK;
    rows = k_font[glyph];

    for (r = 0; r < 8; r++) {
        uint8_t bits = rows[r];
        c = 0;
        while (c < 8) {
            if (bits & (1u << c)) {
                start = c;
                while (c < 8 && (bits & (1u << c)))
                    c++;
                fb_rect(px + start * g_scale, py + r * g_scale,
                        (c - start) * g_scale, g_scale, color);
            } else {
                c++;
            }
        }
    }
}

/*
 * Decodes the next glyph from a UTF-8 string. Returns the glyph index or -1
 * at end of string. The em dash (U+2014) maps to its own glyph; any other
 * non-ASCII sequence is shown as '?'.
 */
static int next_glyph(const unsigned char **pp)
{
    const unsigned char *p = *pp;
    unsigned c = *p;
    int len, i;

    if (c == 0)
        return -1;

    if (c < 0x80) {
        *pp = p + 1;
        return (c >= 32 && c <= 126) ? (int)c - 32 : GLYPH_QMARK;
    }

    len = (c >= 0xF0) ? 4 : ((c >= 0xE0) ? 3 : ((c >= 0xC0) ? 2 : 1));
    for (i = 1; i < len; i++) {
        if ((p[i] & 0xC0) != 0x80)
            break;
    }

    if (len == 3 && p[0] == 0xE2 && p[1] == 0x80 && p[2] == 0x94) {
        *pp = p + 3;
        return GLYPH_EMDASH;
    }

    *pp = p + i;
    return GLYPH_QMARK;
}

static int text_cells(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int n = 0;
    while (next_glyph(&p) >= 0)
        n++;
    return n;
}

/* Draws text at cell position (cx, cy). maxcells < 0 = unlimited; longer
 * text is shortened with "...". */
static void put_text(int cx, int cy, uint32_t color, int maxcells, const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int total, limit, dots = 0, g, i = 0;
    int px = cx * CELL_W, py = cy * CELL_H;

    if (!s || maxcells == 0)
        return;

    total = text_cells(s);
    limit = total;
    if (maxcells > 0 && total > maxcells) {
        if (maxcells >= 4) {
            limit = maxcells - 3;
            dots = 1;
        } else {
            limit = maxcells;
        }
    }

    while (i < limit && (g = next_glyph(&p)) >= 0) {
        draw_glyph(px + i * CELL_W, py, g, color);
        i++;
    }
    if (dots) {
        for (g = 0; g < 3; g++)
            draw_glyph(px + (i + g) * CELL_W, py, '.' - 32, color);
    }
}

static void put_textf(int cx, int cy, uint32_t color, int maxcells, const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';
    put_text(cx, cy, color, maxcells, buf);
}

static void put_center(int cy, uint32_t color, const char *s)
{
    int n = text_cells(s);
    int cx = (g_cols - n) / 2;
    if (cx < 0)
        cx = 0;
    put_text(cx, cy, color, g_cols - 2 * g_mx, s);
}

static void hline(int cy_pixels, uint32_t color)
{
    fb_rect(g_mx * CELL_W - CELL_W / 2, cy_pixels,
            g_width - 2 * (g_mx * CELL_W - CELL_W / 2), g_scale, color);
}

/* ------------------------------------------------------------------ */
/* Frame begin / end                                                  */
/* ------------------------------------------------------------------ */

static void frame_begin(void)
{
    wait_flip();                         /* previous flip is finished   */
    fb_fill_all(&g_fb[g_cur], COL_BG);   /* clear back buffer           */
    g_anim++;
}

static void frame_end(void)
{
    do_flip(g_cur);                      /* queue flip of drawn buffer  */
    g_cur ^= 1;                          /* draw into the other next    */
}

static void draw_header(void)
{
    int band_y = g_my * CELL_H - CELL_H / 2;

    fb_rect(0, band_y, g_width, 3 * CELL_H, COL_HEADER);
    fb_rect(0, band_y + 3 * CELL_H, g_width, 2 * g_scale, COL_ACCENT);

    put_textf(g_mx, g_my, COL_ACCENT, g_cols - 2 * g_mx, "%s v%s", DM_APP_NAME, DM_APP_VERSION);
    put_text(g_mx, g_my + 1, COL_TEXT, g_cols - 2 * g_mx, DM_TAGLINE);
}

/* ------------------------------------------------------------------ */
/* UI state                                                           */
/* ------------------------------------------------------------------ */

int dm_ui_state_init(dm_ui_state_t *ui, int track_capacity)
{
    int cap = track_capacity > 0 ? track_capacity : 1;

    if (!ui)
        return -1;
    memset(ui, 0, sizeof(*ui));

    ui->queue = (int *)calloc((size_t)cap, sizeof(int));
    ui->qpos  = (int *)calloc((size_t)cap, sizeof(int));
    if (!ui->queue || !ui->qpos) {
        free(ui->queue);
        free(ui->qpos);
        memset(ui, 0, sizeof(*ui));
        return -1;
    }
    ui->queue_cap = cap;
    ui->volume    = DM_VOLUME_DEFAULT;
    ui->repeat    = DM_REPEAT_OFF;
    ui->shuffle   = 0;
    return 0;
}

void dm_ui_state_free(dm_ui_state_t *ui)
{
    if (!ui)
        return;
    free(ui->queue);
    free(ui->qpos);
    memset(ui, 0, sizeof(*ui));
}

void dm_ui_set_status(dm_ui_state_t *ui, int level, const char *fmt, ...)
{
    va_list ap;

    if (!ui || !fmt)
        return;
    va_start(ap, fmt);
    vsnprintf(ui->status, sizeof(ui->status), fmt, ap);
    va_end(ap);
    ui->status[sizeof(ui->status) - 1] = '\0';
    ui->status_level = level;
    ui->status_ttl   = 180; /* ~3 seconds at 60 fps */
}

static void rebuild_qpos(dm_ui_state_t *ui)
{
    int i;
    memset(ui->qpos, 0, (size_t)ui->queue_cap * sizeof(int));
    for (i = 0; i < ui->queue_len; i++)
        ui->qpos[ui->queue[i]] = i + 1;
}

int dm_ui_queue_add(dm_ui_state_t *ui, int track_index)
{
    if (!ui || track_index < 0 || track_index >= ui->queue_cap)
        return 0;
    if (ui->qpos[track_index] != 0)
        return 0; /* already queued */
    if (ui->queue_len >= ui->queue_cap)
        return 0;
    ui->queue[ui->queue_len++] = track_index;
    ui->qpos[track_index] = ui->queue_len;
    return 1;
}

static int queue_remove(dm_ui_state_t *ui, int track_index)
{
    int pos, i;

    if (track_index < 0 || track_index >= ui->queue_cap)
        return 0;
    pos = ui->qpos[track_index];
    if (pos == 0)
        return 0;
    for (i = pos - 1; i < ui->queue_len - 1; i++)
        ui->queue[i] = ui->queue[i + 1];
    ui->queue_len--;
    rebuild_qpos(ui);
    return 1;
}

void dm_ui_queue_clear(dm_ui_state_t *ui)
{
    if (!ui)
        return;
    ui->queue_len = 0;
    memset(ui->qpos, 0, (size_t)ui->queue_cap * sizeof(int));
}

/* ------------------------------------------------------------------ */
/* Layout helpers                                                     */
/* ------------------------------------------------------------------ */

static int footer_row0(void)  { return g_rows - g_my - DM_FOOTER_ROWS; }
static int list_row0(void)    { return g_my + 6; }

static int list_rows(void)
{
    int n = footer_row0() - 2 - list_row0() + 1;
    return n < 1 ? 1 : n;
}

static void ensure_visible(dm_ui_state_t *ui, int count)
{
    int page = list_rows();
    int max_top;

    if (ui->cursor < 0) ui->cursor = 0;
    if (ui->cursor >= count) ui->cursor = count > 0 ? count - 1 : 0;

    if (ui->cursor < ui->top)
        ui->top = ui->cursor;
    if (ui->cursor >= ui->top + page)
        ui->top = ui->cursor - page + 1;

    max_top = count - page;
    if (max_top < 0) max_top = 0;
    if (ui->top > max_top) ui->top = max_top;
    if (ui->top < 0) ui->top = 0;
}

static const char *repeat_name(int r)
{
    switch (r) {
    case DM_REPEAT_ALL: return "All";
    case DM_REPEAT_ONE: return "One";
    default:            return "Off";
    }
}

/* ------------------------------------------------------------------ */
/* Input handling                                                     */
/* ------------------------------------------------------------------ */

dm_ui_action_t dm_ui_update(dm_ui_state_t *ui, const dm_scan_t *scan, uint32_t pressed)
{
    int n = scan ? scan->count : 0;
    int page = list_rows();
    int i;

    ui->frame++;
    if (ui->status_ttl > 0)
        ui->status_ttl--;

    if (pressed & DM_BTN_START)
        return DM_ACTION_SAVE;
    if (pressed & DM_BTN_CIRCLE)
        return DM_ACTION_CANCEL;

    if (pressed & DM_BTN_LEFT) {
        ui->volume -= DM_VOLUME_STEP;
        if (ui->volume < DM_VOLUME_MIN) ui->volume = DM_VOLUME_MIN;
        dm_ui_set_status(ui, DM_STATUS_INFO, "Volume: %d%%", ui->volume);
    }
    if (pressed & DM_BTN_RIGHT) {
        ui->volume += DM_VOLUME_STEP;
        if (ui->volume > DM_VOLUME_MAX) ui->volume = DM_VOLUME_MAX;
        dm_ui_set_status(ui, DM_STATUS_INFO, "Volume: %d%%", ui->volume);
    }
    if (pressed & DM_BTN_SQUARE) {
        ui->repeat = (ui->repeat + 1) % 3;
        dm_ui_set_status(ui, DM_STATUS_INFO, "Repeat: %s", repeat_name(ui->repeat));
    }
    if (pressed & DM_BTN_L1) {
        ui->shuffle = !ui->shuffle;
        dm_ui_set_status(ui, DM_STATUS_INFO, "Shuffle: %s", ui->shuffle ? "On" : "Off");
    }

    if (n > 0) {
        if (pressed & DM_BTN_UP)
            ui->cursor = (ui->cursor <= 0) ? n - 1 : ui->cursor - 1;
        if (pressed & DM_BTN_DOWN)
            ui->cursor = (ui->cursor >= n - 1) ? 0 : ui->cursor + 1;
        if (pressed & DM_BTN_L2) {
            ui->cursor -= page;
            if (ui->cursor < 0) ui->cursor = 0;
        }
        if (pressed & DM_BTN_R2) {
            ui->cursor += page;
            if (ui->cursor > n - 1) ui->cursor = n - 1;
        }

        if (pressed & DM_BTN_CROSS) {
            if (queue_remove(ui, ui->cursor)) {
                dm_ui_set_status(ui, DM_STATUS_INFO, "Removed from queue (%d queued)", ui->queue_len);
            } else if (dm_ui_queue_add(ui, ui->cursor)) {
                dm_ui_set_status(ui, DM_STATUS_GOOD, "Enqueued #%d: %s", ui->queue_len,
                                 dm_track_name(&scan->tracks[ui->cursor]));
            } else {
                dm_ui_set_status(ui, DM_STATUS_WARN, "Queue is full");
            }
        }

        if (pressed & DM_BTN_TRIANGLE) {
            int added = 0;
            for (i = 0; i < n; i++)
                added += dm_ui_queue_add(ui, i);
            if (added > 0)
                dm_ui_set_status(ui, DM_STATUS_GOOD, "Enqueued %d tracks (%d in queue)", added, ui->queue_len);
            else
                dm_ui_set_status(ui, DM_STATUS_INFO, "All tracks are already enqueued");
        }

        if (pressed & DM_BTN_SELECT) {
            dm_ui_queue_clear(ui);
            dm_ui_set_status(ui, DM_STATUS_INFO, "Queue cleared");
        }

        ensure_visible(ui, n);
    }

    return DM_ACTION_NONE;
}

/* ------------------------------------------------------------------ */
/* Screens                                                            */
/* ------------------------------------------------------------------ */

void dm_ui_draw_scanning(int found, const char *current_dir)
{
    static const char spinner[4] = {'|', '/', '-', '\\'};
    int mid = g_rows / 2;
    int i;

    if (!g_gfx_ready)
        return;

    frame_begin();
    draw_header();

    put_center(mid - 3, COL_TEXT, "Scanning for music...");
    put_textf((g_cols - 1) / 2, mid - 1, COL_ACCENT, 1, "%c", spinner[(g_anim / 6) & 3]);
    put_textf(g_mx, mid + 1, COL_TEXT, g_cols - 2 * g_mx, "Tracks found so far: %d", found);
    put_textf(g_mx, mid + 2, COL_DIM, g_cols - 2 * g_mx, "Looking for .mp3 and .wav in:");
    for (i = 0; i < DM_SCAN_ROOT_COUNT; i++)
        put_textf(g_mx + 2, mid + 3 + i, COL_DIM, g_cols - 2 * g_mx - 2, "%s", dm_scan_root_path(i));
    if (current_dir && current_dir[0])
        put_textf(g_mx, mid + 4 + DM_SCAN_ROOT_COUNT, COL_DIM, g_cols - 2 * g_mx, "Now: %s", current_dir);

    frame_end();
}

void dm_ui_draw_message(const char *title, const char *line1, const char *line2, int level)
{
    uint32_t col = COL_TEXT;
    int mid = g_rows / 2;

    if (!g_gfx_ready)
        return;

    if (level == DM_STATUS_GOOD) col = COL_GOOD;
    else if (level == DM_STATUS_WARN) col = COL_WARN;
    else if (level == DM_STATUS_BAD) col = COL_BAD;

    frame_begin();
    draw_header();
    put_center(mid - 2, col, title ? title : "");
    if (line1) put_center(mid, COL_TEXT, line1);
    if (line2) put_center(mid + 1, COL_DIM, line2);
    frame_end();
}

static void draw_empty_state(const dm_scan_t *scan)
{
    int y = list_row0() + 1;
    int i;

    put_center(y, COL_WARN, "No tracks found");
    put_center(y + 2, COL_TEXT, "Copy .mp3 or .wav files into one of these folders:");

    for (i = 0; i < DM_SCAN_ROOT_COUNT; i++) {
        int present = scan->root_present[i];
        put_textf(g_mx + 2, y + 4 + i, present ? COL_TEXT : COL_DIM, g_cols - 2 * g_mx - 14,
                  "%s", dm_scan_root_path(i));
        put_text(g_cols - g_mx - 10, y + 4 + i, present ? COL_GOOD : COL_BAD, 10,
                 present ? "[found]" : "[missing]");
    }

    put_center(y + 5 + DM_SCAN_ROOT_COUNT, COL_DIM, "Then restart DMmusic to rescan.");
    put_center(y + 7 + DM_SCAN_ROOT_COUNT, COL_DIM,
               "START saves an empty queue. CIRCLE exits without saving.");
}

static void draw_track_list(const dm_ui_state_t *ui, const dm_scan_t *scan)
{
    int page = list_rows();
    int n = scan->count;
    int row, idx;
    int name_x = g_mx + 13;
    int name_w = g_cols - g_mx - 2 - name_x;
    int bar_x  = (g_cols - g_mx) * CELL_W - CELL_W / 2;
    int bar_y  = list_row0() * CELL_H;
    int bar_h  = page * CELL_H;

    for (row = 0; row < page; row++) {
        int cy = list_row0() + row;
        idx = ui->top + row;
        if (idx >= n)
            break;

        if (idx == ui->cursor) {
            fb_rect(g_mx * CELL_W - CELL_W / 2, cy * CELL_H,
                    g_width - 2 * (g_mx * CELL_W - CELL_W / 2), CELL_H, COL_SEL_BG);
            put_text(g_mx, cy, COL_ACCENT, 1, ">");
        }

        if (ui->qpos[idx] > 0)
            put_textf(g_mx + 2, cy, COL_QUEUED, 5, "[%3d]", ui->qpos[idx]);
        else
            put_text(g_mx + 2, cy, COL_DIM, 5, "[   ]");

        put_textf(g_mx + 8, cy, COL_DIM, 4, "%4d", idx + 1);
        put_text(name_x, cy, COL_TEXT, name_w,
                 dm_track_name(&scan->tracks[idx]));
    }

    /* scroll bar */
    fb_rect(bar_x, bar_y, g_scale * 2, bar_h, COL_LINE);
    if (n > page) {
        int thumb_h = (bar_h * page) / n;
        int thumb_y;
        if (thumb_h < CELL_H) thumb_h = CELL_H;
        thumb_y = bar_y + ((bar_h - thumb_h) * ui->top) / (n - page);
        fb_rect(bar_x, thumb_y, g_scale * 2, thumb_h, COL_ACCENT);
    }
}

void dm_ui_draw_main(const dm_ui_state_t *ui, const dm_scan_t *scan)
{
    int f0, maxw = g_cols - 2 * g_mx;
    uint32_t scol;

    if (!g_gfx_ready)
        return;

    frame_begin();
    draw_header();

    /* info line */
    put_textf(g_mx, g_my + 4, COL_TEXT, maxw,
              "Tracks: %d   Queued: %d   Volume: %d%%   Repeat: %s   Shuffle: %s",
              scan->count, ui->queue_len, ui->volume, repeat_name(ui->repeat),
              ui->shuffle ? "On" : "Off");

    if (scan->count > 0)
        draw_track_list(ui, scan);
    else
        draw_empty_state(scan);

    /* footer */
    f0 = footer_row0();
    hline((f0 - 1) * CELL_H + CELL_H / 2, COL_LINE);

    if (ui->status_ttl > 0) {
        scol = (ui->status_level == DM_STATUS_GOOD) ? COL_GOOD :
               (ui->status_level == DM_STATUS_WARN) ? COL_WARN :
               (ui->status_level == DM_STATUS_BAD)  ? COL_BAD  : COL_TEXT;
        put_text(g_mx, f0, scol, maxw, ui->status);
    } else if (scan->truncated) {
        put_textf(g_mx, f0, COL_WARN, maxw, "Library truncated at %d tracks", DM_SCAN_MAX_TRACKS);
    } else {
        put_text(g_mx, f0, COL_DIM, maxw, "Ready.");
    }

    if (scan->count > 0 && ui->cursor >= 0 && ui->cursor < scan->count)
        put_textf(g_mx, f0 + 1, COL_DIM, maxw, "Path: %s", scan->tracks[ui->cursor].path);

    put_text(g_mx, f0 + 2, COL_TEXT, maxw,
             "X: Select/Enqueue   TRIANGLE: Enqueue all   SQUARE: Repeat");
    put_text(g_mx, f0 + 3, COL_TEXT, maxw,
             "L1: Shuffle   SELECT: Clear queue   LEFT/RIGHT: Volume   L2/R2: Page");
    put_text(g_mx, f0 + 4, COL_ACCENT, maxw,
             "START: Save & return to XMB   CIRCLE: Exit without saving");

    frame_end();
}
