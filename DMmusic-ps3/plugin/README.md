# DMmusic plugin (`dmmusic.sprx`)

**Made by DreeMux — Made for PS3 gamers**

Background MP3/WAV player for the XMB/VSH. It reads the queue written by
`DMmusic.pkg` (`/dev_hdd0/tmp/dmmusic_queue.txt`, format `DMMUSIC_QUEUE_V1`) and
reloads it automatically when the file changes (size + content fingerprint,
checked every 2 s).

## Hotkeys (pad 0)

| Combo | Action |
|-------|--------|
| SELECT + R1 | Next track |
| SELECT + L1 | Previous track |
| SELECT + DOWN | Pause / resume |
| SELECT + UP | Volume up (holds auto-repeat) |
| SELECT + LEFT | Volume down (holds auto-repeat) |

## Layout

| File | Role |
|------|------|
| `source/main.c` | `module_start` / `module_stop`, worker thread, queue parser + watcher |
| `source/audio_engine.c` | queue, shuffle/repeat, 44.1/48 kHz → 48 kHz resampler, cellAudio port |
| `source/decoder.c` | WAV (PCM16) parser + dr_mp3 streaming wrapper (4 KB reads, 64 KB private heap) |
| `source/pad_hook.c` | debounced hotkey state machine over `cellPadGetData` |
| `source/dm_sys.c` | raw LV2 syscalls (threads, file I/O, sleep) |
| `source/dm_libc.c` | `memcpy`/`memset`/... so no C runtime is needed in VSH |
| `test/host_test.c` | x86 unit test: `make -C plugin host-test` |
| `prx/` | PRX packaging layer (module info, imports, linker script, signing) — see `prx/README.md` |

`include/dr_mp3.h` is downloaded by CI (and git-ignored). For a local build:

```bash
curl -fsSL https://raw.githubusercontent.com/mackron/dr_libs/master/dr_mp3.h -o plugin/include/dr_mp3.h
```

## RAM budget

About 0.35 MB of static data (queue of 1000 × 224-byte paths ≈ 224 KB, decoder
state ≈ 80 KB, heap 64 KB) plus two small thread stacks (audio 64 KB because
dr_mp3 needs ~30 KB at peak; worker 32 KB). Well under 2 MB.

Licensed under the GNU General Public License v3.0 or later (see `../LICENSE`).
