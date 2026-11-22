/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - hotkey listener API
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

#ifndef DM_PAD_HOOK_H
#define DM_PAD_HOOK_H

#include <stdint.h>
#include "dm_sys.h"

/*
 * Non-blocking hotkey listener.  Call dm_pad_poll() at a fixed rate
 * (DM_PAD_POLL_MS); it reads pad 0 once and returns the DM_CMD_* mask of
 * actions that fired on this tick.
 *
 *   SELECT + R1     next track          (edge triggered)
 *   SELECT + L1     previous track      (edge triggered)
 *   SELECT + DOWN   pause / resume      (edge triggered)
 *   SELECT + UP     volume up           (edge, then auto-repeat while held)
 *   SELECT + LEFT   volume down         (edge, then auto-repeat while held)
 *
 * The pad is only read, never initialised or closed: the VSH process owns it.
 */

#define DM_PAD_POLL_MS 16u

/* Raw digital bits (button[2] = digital 1, button[3] = digital 2). */
#define DM_PAD_SELECT 0x0001u   /* digital 1 */
#define DM_PAD_UP     0x0010u   /* digital 1 */
#define DM_PAD_DOWN   0x0040u   /* digital 1 */
#define DM_PAD_LEFT   0x0080u   /* digital 1 */
#define DM_PAD_L1     0x0004u   /* digital 2 */
#define DM_PAD_R1     0x0008u   /* digital 2 */

void     dm_pad_reset(void);

/* Pure state machine, no I/O: feed it the two digital words.  Exposed so it
 * can be unit-tested on the host. */
uint32_t dm_pad_step(uint16_t digital1, uint16_t digital2);

/* Reads pad 0 and runs dm_pad_step(); returns 0 when no pad data is available. */
uint32_t dm_pad_poll(void);

#endif /* DM_PAD_HOOK_H */
