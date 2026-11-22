# prx/ — PRX packaging layer

**Made by DreeMux — Made for PS3 gamers**

Everything that depends on the binary layout of a VSH PRX lives here and
nowhere else:

* module info + export table for `module_start` / `module_stop`
* import table for `cellAudio*` / `cellPadGetData` (needs the real NIDs)
* linker script and the sign/convert step (`sprxlinker`, `make_fself`, ...)

The plugin sources in `../source` only assume that a PRX built from them
exports `module_start(uint64_t, void *)` and `module_stop(uint64_t, void *)` and
that the symbols declared at the bottom of `../include/dm_sys.h` are resolved
by import stubs.

`../Makefile` includes `prx/link.mk` when it exists; that file must define a
rule producing `dmmusic.sprx` from `$(OBJS)`. It is not included yet because it
has to be written from the files the CI "PRX toolchain probe" step prints, and
from NIDs checked against a reliable source, not from memory.
