/*
 * DMmusic - VSH background plugin (dmmusic.sprx) - system layer (LV2 syscalls / host shim)
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

#include "dm_sys.h"

#ifdef DM_HOST
/* ===================== host back end (unit tests only) ================== */
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

int dm_fs_open_ro(const char *path, int *fd)
{
    int f = open(path, O_RDONLY);
    if (f < 0) return -1;
    *fd = f;
    return 0;
}
int dm_fs_read(int fd, void *buf, uint32_t n, uint32_t *nread)
{
    ssize_t r = read(fd, buf, n);
    if (r < 0) return -1;
    *nread = (uint32_t)r;
    return 0;
}
int dm_fs_seek(int fd, int64_t off, int whence, uint64_t *pos)
{
    off_t r = lseek(fd, (off_t)off, whence);
    if (r < 0) return -1;
    if (pos) *pos = (uint64_t)r;
    return 0;
}
int dm_fs_close(int fd) { return close(fd); }

typedef struct { dm_thread_fn fn; uint64_t arg; } host_thr_t;
static void *host_tramp(void *p)
{
    host_thr_t t = *(host_thr_t *)p;
    free(p);
    t.fn(t.arg);
    return NULL;
}
int dm_thread_create(dm_thread_t *t, dm_thread_fn fn, uint64_t arg,
                     int prio, uint32_t stack, const char *name)
{
    pthread_t th;
    host_thr_t *h = (host_thr_t *)malloc(sizeof(*h));
    (void)prio; (void)stack; (void)name;
    if (!h) return -1;
    h->fn = fn; h->arg = arg;
    if (pthread_create(&th, NULL, host_tramp, h) != 0) return -1;
    *t = (dm_thread_t)th;
    return 0;
}
int dm_thread_join(dm_thread_t t) { return pthread_join((pthread_t)t, NULL); }
void dm_thread_exit(void) { }
void dm_sleep_ms(uint32_t ms) { usleep(ms * 1000u); }
void dm_log(const char *msg) { (void)msg; }
void dm_log_hex(const char *label, uint32_t val) { (void)label; (void)val; }

#else
/* ===================== PS3 PPU back end (LV2 syscalls) ================== */

#define SC_THREAD_EXIT     41
#define SC_THREAD_JOIN     44
#define SC_THREAD_CREATE   52
#define SC_TIMER_USLEEP    141
#define SC_FS_OPEN         801
#define SC_FS_READ         802
#define SC_FS_CLOSE        804
#define SC_FS_LSEEK        818

#define SC_THREAD_JOINABLE 1ull

static inline uint64_t dm_sc(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
                             uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
{
    register uint64_t r11 __asm__("11") = num;
    register uint64_t r3  __asm__("3")  = a1;
    register uint64_t r4  __asm__("4")  = a2;
    register uint64_t r5  __asm__("5")  = a3;
    register uint64_t r6  __asm__("6")  = a4;
    register uint64_t r7  __asm__("7")  = a5;
    register uint64_t r8  __asm__("8")  = a6;
    register uint64_t r9  __asm__("9")  = a7;

    __asm__ volatile("sc"
                     : "+r"(r3), "+r"(r4), "+r"(r5), "+r"(r6),
                       "+r"(r7), "+r"(r8), "+r"(r9), "+r"(r11)
                     :
                     : "r0", "r10", "r12", "lr", "ctr", "xer",
                       "cr0", "cr1", "cr5", "cr6", "cr7", "memory");
    return r3;
}

#define P(x) ((uint64_t)(uintptr_t)(x))

int dm_fs_open_ro(const char *path, int *fd)
{
    int32_t f = -1;
    int32_t r = (int32_t)dm_sc(SC_FS_OPEN, P(path), 0 /* O_RDONLY */, P(&f), 0, 0, 0, 0);
    if (r != 0) return -1;
    *fd = f;
    return 0;
}
int dm_fs_read(int fd, void *buf, uint32_t n, uint32_t *nread)
{
    uint64_t got = 0;
    int32_t r = (int32_t)dm_sc(SC_FS_READ, (uint64_t)(int64_t)fd, P(buf), n, P(&got), 0, 0, 0);
    if (r != 0) return -1;
    *nread = (uint32_t)got;
    return 0;
}
int dm_fs_seek(int fd, int64_t off, int whence, uint64_t *pos)
{
    uint64_t p = 0;
    int32_t r = (int32_t)dm_sc(SC_FS_LSEEK, (uint64_t)(int64_t)fd, (uint64_t)off,
                               (uint64_t)whence, P(&p), 0, 0, 0);
    if (r != 0) return -1;
    if (pos) *pos = p;
    return 0;
}
int dm_fs_close(int fd)
{
    return (int)(int32_t)dm_sc(SC_FS_CLOSE, (uint64_t)(int64_t)fd, 0, 0, 0, 0, 0, 0);
}

int dm_thread_create(dm_thread_t *t, dm_thread_fn fn, uint64_t arg,
                     int prio, uint32_t stack, const char *name)
{
    uint64_t id = 0;
    /* On the PPU64 ELF ABI a C function pointer is the address of its
     * function descriptor, which is what sys_ppu_thread_create expects. */
    int32_t r = (int32_t)dm_sc(SC_THREAD_CREATE, P(&id), P(fn), arg,
                               (uint64_t)(int64_t)prio, stack, SC_THREAD_JOINABLE, P(name));
    if (r != 0) return -1;
    *t = id;
    return 0;
}
int dm_thread_join(dm_thread_t t)
{
    uint64_t ret = 0;
    return (int)(int32_t)dm_sc(SC_THREAD_JOIN, t, P(&ret), 0, 0, 0, 0, 0);
}
void dm_thread_exit(void)
{
    dm_sc(SC_THREAD_EXIT, 0, 0, 0, 0, 0, 0, 0);
}
void dm_sleep_ms(uint32_t ms)
{
    dm_sc(SC_TIMER_USLEEP, (uint64_t)ms * 1000ull, 0, 0, 0, 0, 0, 0);
}

#define SC_FS_WRITE        803
#define SC_PRX_LOAD_MODULE 480
#define SC_PRX_START_MODULE 481

void dm_log(const char *msg)
{
    int32_t f = -1;
    uint32_t len = 0;
    uint64_t wr = 0;
    while (msg[len]) len++;
    /* O_WRONLY | O_CREAT | O_APPEND = 0x401 */
    if ((int32_t)dm_sc(SC_FS_OPEN, P("/dev_hdd0/tmp/dmmusic.log"), 0x401, P(&f), 0666, 0, 0, 0) == 0 && f >= 0) {
        dm_sc(SC_FS_WRITE, (uint64_t)(int64_t)f, P(msg), len, P(&wr), 0, 0, 0);
        dm_sc(SC_FS_CLOSE, (uint64_t)(int64_t)f, 0, 0, 0, 0, 0, 0);
    }
}

void dm_log_hex(const char *label, uint32_t val)
{
    char buf[64];
    const char *hex = "0123456789ABCDEF";
    uint32_t i = 0;
    int s;
    while (label[i] && i < 48) { buf[i] = label[i]; i++; }
    buf[i++] = '0'; buf[i++] = 'x';
    for (s = 28; s >= 0; s -= 4) buf[i++] = hex[(val >> s) & 0xFu];
    buf[i++] = '\n';
    buf[i] = '\0';
    dm_log(buf);
}

#endif /* DM_HOST */
