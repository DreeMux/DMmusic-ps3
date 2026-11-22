# DMmusic

**Made by DreeMux — Made for PS3 gamers**

DMmusic is a background music player for the PlayStation 3, built for
**CFW Evilnat 4.93 with Cobra 8.5**. It lets you pick MP3/WAV tracks from your
console and keep them playing while you game.

It is split into two decoupled parts:

| Part | Output | Role |
|------|--------|------|
| **Frontend** (`pkg/`) | `DMmusic.pkg` | A normal homebrew app (PSL1GHT / RSX). You launch it from the XMB, browse your music, build a queue and save it. |
| **Backend** (`plugin/`) | `dmmusic.sprx` | A VSH plugin loaded by Cobra that reads the saved queue and plays it in the background. *(Phase 2 — not in this commit yet.)* |

> **Status:** Phase 1 (this repository state) contains the repository layout,
> CI, documentation, license and the complete **frontend app**. The VSH plugin
> follows in phase 2.

## Features (frontend, `DMmusic.pkg`)

- Recursively scans `/dev_hdd0/MUSIC`, `/dev_usb000/MUSIC` and `/dev_usb001/MUSIC`
  for `.mp3` and `.wav` files (case-insensitive). Missing folders are fine.
- Shows a clear **"No tracks found"** screen (with folder status) instead of
  hanging or exiting when there is no music.
- Flicker-free double-buffered UI with the branding header
  *"Made by DreeMux — Made for PS3 gamers"*.
- Track list with scrolling, queue positions, volume, repeat and shuffle.
- Saves the queue and playback settings to `/dev_hdd0/tmp/dmmusic_queue.txt`
  and returns cleanly to the XMB.
- Remembers your last queue/settings and restores them on the next launch.

### Controls

| Button | Action |
|--------|--------|
| D-Pad Up/Down | Move cursor |
| L2 / R2 | Page up / page down |
| **X** | Select / enqueue the highlighted track (press again to remove) |
| **TRIANGLE** | Enqueue all tracks |
| **SELECT** | Clear the queue |
| D-Pad Left/Right | Volume −/+ 5 % |
| **SQUARE** | Repeat: Off → All → One |
| **L1** | Toggle shuffle |
| **START** | Save the queue and return to the XMB |
| **CIRCLE** | Exit without saving |
| PS button → Quit | Always handled cleanly (never freezes the console) |

### Queue file format

`/dev_hdd0/tmp/dmmusic_queue.txt` — UTF-8, one `key=value` per line, `\n` line endings:

```text
DMMUSIC_QUEUE_V1
volume=80
repeat=0
shuffle=0
count=2
track=/dev_hdd0/MUSIC/Album/01 Song.mp3
track=/dev_usb000/MUSIC/02 Another.wav
```

`repeat`: `0` off, `1` all, `2` one. `count` always equals the number of
`track=` lines, which are in play order. The file is written to a temporary
file first and then renamed, so the plugin never reads a half-written queue.

### Anti-black-screen design

Homebrew packages often boot to a black screen on real hardware. DMmusic
avoids that by:

1. Initialising RSX/GCM video with **two XRGB framebuffers** before anything
   slow happens, and putting a frame on screen immediately.
2. Showing a live **"Scanning for music…"** screen (with spinner and counter)
   *during* the folder scan, so large libraries never look frozen.
3. Calling `sysUtilCheckCallback()` **every frame** (including during the scan
   and save screens) and handling `SYSUTIL_EXIT_GAME` so the PS button always
   works.
4. Flipping with `gcmSetFlip()` + `gcmSetWaitFlip()` and waiting with
   `gcmGetFlipStatus()` every frame, even when no music exists.
5. Shipping a valid `PARAM.SFO` (`CATEGORY=HG`, `TITLE_ID=DMMUS0001`,
   `APP_VER=01.00`, `BOOTABLE=1`).

Text is drawn with an embedded 8x8 bitmap font, so the app needs **no extra
libraries** beyond PSL1GHT itself.

## Architecture: why a `.pkg` + a VSH `.sprx`?

Playing audio while another game runs needs code that lives inside the XMB
(VSH) process, which is why the player itself is a `.sprx` plugin. The
configuration UI, however, is a regular app. Keeping them **decoupled** gives:

- **No firmware/bootloader modification.** The plugin is loaded by Cobra from
  `boot_plugins.txt`. Nothing touches `coldboot.raf`, `coldboot_stereo.raf`
  or other flash/VSH resources, which are the classic way to soft-brick a
  console with a bad mod.
- **Easy recovery.** If anything goes wrong, removing one line from
  `boot_plugins.txt` (or deleting the `.sprx`) disables the plugin.
- **Safe UI.** A crash in the picker app just returns you to the XMB; it can
  never take the plugin or the system with it.
- **Tiny contract.** The two halves only share the queue text file above.

## Building

### With GitHub Actions

Every push and pull request runs `.github/workflows/build.yml`, which builds
inside the official `ps3dev/ps3dev` Docker image, checks that the outputs exist
and are non-empty, and uploads them as artifacts (`DMmusic-pkg`, and
`dmmusic-sprx` once the plugin exists).

### Locally with Docker

```bash
git clone https://github.com/<you>/DMmusic.git
cd DMmusic
docker run --rm -v "$PWD":/src -w /src ps3dev/ps3dev make -C pkg clean pkg
# -> pkg/DMmusic.pkg
```

### Locally with a native toolchain

Install [ps3dev](https://github.com/ps3dev/ps3dev) (ps3toolchain + PSL1GHT),
make sure `PS3DEV` and `PSL1GHT` are exported, then:

```bash
make -C pkg pkg      # builds pkg/DMmusic.pkg
make -C pkg clean
```

## Installing on Evilnat 4.93 + Cobra 8.5

1. Copy `DMmusic.pkg` to a USB stick (or `/dev_hdd0/packages/`) and install it
   from **Game → Package Manager → Install Package Files** (or via webMAN MOD
   / FTP).
2. Put your music in `/dev_hdd0/MUSIC` or `/dev_usb000/MUSIC` (subfolders are
   fine).
3. Launch **DMmusic** from the XMB, pick tracks, press **START**.
4. *(Phase 2)* Copy `dmmusic.sprx` to `/dev_hdd0/plugins/dmmusic.sprx` and add
   this line to `/dev_hdd0/boot_plugins.txt` (one path per line):

   ```text
   /dev_hdd0/plugins/dmmusic.sprx
   ```

   Reboot the console. With Cobra 8.5 the plugin is loaded into VSH at boot.

## Hot-reload testing with webMAN MOD (phase 2)

During development you can load and unload the plugin without rebooting. In a
browser (or `curl`) on the same network, replace `PS3-IP` with your console's
address — or use `localhost` from the PS3's own browser:

```text
http://PS3-IP/vshplugin.ps3?/dev_hdd0/plugins/dmmusic.sprx
http://PS3-IP/unloadprx.ps3?prx=/dev_hdd0/plugins/dmmusic.sprx&slot=6
http://PS3-IP/loadprx.ps3?prx=/dev_hdd0/plugins/dmmusic.sprx&slot=6
```

`vshplugin.ps3` toggles the plugin (load if unloaded, unload if loaded);
`loadprx.ps3` / `unloadprx.ps3` target an explicit VSH slot. Command names
and slot rules can differ slightly between webMAN MOD versions, so check the
webMAN MOD documentation for your build.

## Repository layout

```text
DMmusic/
├── .github/workflows/build.yml   CI: build + verify + upload artifacts
├── LICENSE                       GNU GPL v3.0 (full text)
├── README.md
├── pkg/                          Frontend app -> DMmusic.pkg
│   ├── Makefile
│   ├── sfo.xml                   PARAM.SFO template (HG, TITLE_ID, APP_VER)
│   ├── ICON0.PNG
│   ├── include/{scanner.h, ui.h}
│   └── source/{main.c, scanner.c, ui.c}
└── plugin/                       VSH plugin -> dmmusic.sprx (phase 2)
    └── README.md
```

## License

DMmusic is free software under the **GNU General Public License v3.0 or
later**. See [`LICENSE`](LICENSE). The embedded 8x8 font data is the
public-domain *font8x8_basic* by Daniel Hepper.

---

**Made by DreeMux — Made for PS3 gamers**
