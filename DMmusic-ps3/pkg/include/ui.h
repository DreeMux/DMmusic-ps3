/*
 * DMmusic - Frontend (DMmusic.pkg) - video, pad input and user interface (header)
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

#ifndef DMMUSIC_UI_H
#define DMMUSIC_UI_H

#include <stdint.h>
#include "scanner.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DM_APP_NAME      "DMmusic"
#define DM_APP_VERSION   "1.0.0"
#define DM_TAGLINE       "Made by DreeMux — Made for PS3 gamers"

#define DM_VOLUME_MIN    0
#define DM_VOLUME_MAX    100
#define DM_VOLUME_STEP   5
#define DM_VOLUME_DEFAULT 80

/* Pad buttons (bit mask returned by dm_ui_poll_pad) */
enum {
    DM_BTN_UP       = 1u << 0,
    DM_BTN_DOWN     = 1u << 1,
    DM_BTN_LEFT     = 1u << 2,
    DM_BTN_RIGHT    = 1u << 3,
    DM_BTN_CROSS    = 1u << 4,
    DM_BTN_CIRCLE   = 1u << 5,
    DM_BTN_TRIANGLE = 1u << 6,
    DM_BTN_SQUARE   = 1u << 7,
    DM_BTN_START    = 1u << 8,
    DM_BTN_SELECT   = 1u << 9,
    DM_BTN_L1       = 1u << 10,
    DM_BTN_R1       = 1u << 11,
    DM_BTN_L2       = 1u << 12,
    DM_BTN_R2       = 1u << 13
};

typedef enum {
    DM_REPEAT_OFF = 0,
    DM_REPEAT_ALL = 1,
    DM_REPEAT_ONE = 2
} dm_repeat_t;

typedef enum {
    DM_ACTION_NONE = 0,
    DM_ACTION_SAVE,     /* START: save queue and return to XMB      */
    DM_ACTION_CANCEL    /* CIRCLE: exit without saving              */
} dm_ui_action_t;

typedef enum {
    DM_STATUS_INFO = 0,
    DM_STATUS_GOOD,
    DM_STATUS_WARN,
    DM_STATUS_BAD
} dm_status_level_t;

typedef struct dm_ui_state {
    int   cursor;          /* highlighted track index                    */
    int   top;             /* first visible track index                  */
    int   volume;          /* 0..100                                     */
    int   repeat;          /* dm_repeat_t                                */
    int   shuffle;         /* 0 / 1                                      */
    int  *queue;           /* ordered list of track indices              */
    int   queue_len;
    int   queue_cap;
    int  *qpos;            /* per-track 1-based queue position (0 = no)  */
    char  status[160];
    int   status_level;    /* dm_status_level_t                          */
    int   status_ttl;      /* frames left to show the status             */
    uint32_t frame;
} dm_ui_state_t;

/* ---- video / pad lifetime ------------------------------------------ */

/* Initialises RSX double-buffered video and pads. Returns 0 on success. */
int  dm_ui_init(void);

/* Releases pads and video buffers. Safe to call once after dm_ui_init. */
void dm_ui_shutdown(void);

/*
 * Polls all connected pads. Returns buttons newly pressed this frame
 * (D-Pad buttons auto-repeat while held).
 */
uint32_t dm_ui_poll_pad(void);

/* ---- UI state ------------------------------------------------------ */

int  dm_ui_state_init(dm_ui_state_t *ui, int track_capacity);
void dm_ui_state_free(dm_ui_state_t *ui);
void dm_ui_set_status(dm_ui_state_t *ui, int level, const char *fmt, ...);

/* Queue helpers. Return 1 if the queue changed, 0 otherwise. */
int  dm_ui_queue_add(dm_ui_state_t *ui, int track_index);
void dm_ui_queue_clear(dm_ui_state_t *ui);

/* Applies one frame of input to the UI state. */
dm_ui_action_t dm_ui_update(dm_ui_state_t *ui, const dm_scan_t *scan, uint32_t pressed);

/* ---- screens (each call renders and flips exactly one frame) ------- */

void dm_ui_draw_scanning(int found, const char *current_dir);
void dm_ui_draw_main(const dm_ui_state_t *ui, const dm_scan_t *scan);
void dm_ui_draw_message(const char *title, const char *line1, const char *line2, int level);

#ifdef __cplusplus
}
#endif

#endif /* DMMUSIC_UI_H */
