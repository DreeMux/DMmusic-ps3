/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - system layer header
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

#ifndef DM_SYS_H
#define DM_SYS_H

#include <stddef.h>
#include <stdint.h>

/*
 * Everything that touches the OS lives behind this header, so the rest of the
 * plugin is plain portable C.  Two back ends (see dm_sys.c):
 *   - default (PS3 PPU): raw LV2 syscalls through the "sc" instruction, so no
 *     libc / PSL1GHT runtime is needed inside the VSH process;
 *   - DM_HOST (host unit test only): POSIX equivalents.
 *
 * cellAudio* and cellPadGetData are provided by the VSH process itself and
 * are declared at the bottom; the PRX import table resolves them.
 */

#define DM_SEEK_SET 0
#define DM_SEEK_CUR 1
#define DM_SEEK_END 2

typedef uint64_t dm_thread_t;
typedef void (*dm_thread_fn)(uint64_t arg);

int  dm_fs_open_ro(const char *path, int *fd);          /* 0 on success */
int  dm_fs_read(int fd, void *buf, uint32_t n, uint32_t *nread);
int  dm_fs_seek(int fd, int64_t off, int whence, uint64_t *pos);
int  dm_fs_close(int fd);

int  dm_thread_create(dm_thread_t *t, dm_thread_fn fn, uint64_t arg,
                      int prio, uint32_t stack, const char *name);
int  dm_thread_join(dm_thread_t t);
void dm_thread_exit(void);
void dm_sleep_ms(uint32_t ms);

/* ---- cellAudio / cellPad as exposed by the VSH process ------------------ */

typedef struct {
    uint64_t nChannel;
    uint64_t nBlock;
    uint64_t attr;
    float    level;
} dm_audio_port_param_t;

typedef struct {
    uint32_t readIndexAddr;   /* 32-bit LV2 address of the read block index */
    uint32_t status;
    uint64_t nChannel;
    uint64_t nBlock;
    uint32_t portSize;
    uint32_t portAddr;        /* 32-bit LV2 address of the float PCM buffer */
} dm_audio_port_config_t;

typedef struct {
    int32_t  len;
    uint16_t button[64];      /* [2] = digital 1, [3] = digital 2 */
} dm_pad_data_t;

#define DM_AUDIO_ERR_ALREADY_INIT 0x80310701u

int32_t cellAudioInit(void);
int32_t cellAudioQuit(void);
int32_t cellAudioPortOpen(dm_audio_port_param_t *param, uint32_t *portNum);
int32_t cellAudioPortClose(uint32_t portNum);
int32_t cellAudioPortStart(uint32_t portNum);
int32_t cellAudioPortStop(uint32_t portNum);
int32_t cellAudioGetPortConfig(uint32_t portNum, dm_audio_port_config_t *cfg);
int32_t cellPadGetData(uint32_t port, dm_pad_data_t *data);

#endif /* DM_SYS_H */
