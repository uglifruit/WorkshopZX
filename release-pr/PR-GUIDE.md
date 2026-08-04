# Submitting ZX to the Workshop_Computer repo (PR guide)

This folder (`release-pr/`) holds a **self-contained release-folder snapshot** of the
ZX + OneBit card, packaged to match the convention of Andy's existing cards
(`57_glitch`, `60_markov`): full source, ROMs, the compiled firmware in `UF2/`, the
web app, panel overlays, `info.yaml`, and `README.md`. It has been **verified to build
from scratch in isolation**.

## Release number: 61 (assigned)

The card is **already published upstream** as `61_ZX_Spectrum` —
https://computer.musicthing.co.uk/programs/61-zx-spectrum/index.html — so this is
no longer a first submission. That page currently serves **v1.0.0**; the point of
the next PR is to bring the published card up to the current version.

## Steps

1. **Fork** `TomWhitwell/Workshop_Computer` on GitHub (if not already) and clone your
   fork. If the fork already exists, sync it with upstream `main` first — the
   published `releases/61_ZX_Spectrum/` is what you're replacing.

2. **Replace the folder** in the fork's `releases/`:
   ```sh
   rm -rf <fork>/releases/61_ZX_Spectrum
   cp -r release-pr/61_ZX_Spectrum <fork>/releases/61_ZX_Spectrum
   ```
   A straight copy over the top would leave behind any file the card no longer
   ships — notably the old space-containing `panels/` overlay names, which is what
   broke the rendered README images the first time.

3. **Update the table row** in `releases/README.md` (the row already exists; only the
   version/description cells change):

   ```
   | 61_ZX_Spectrum | A cycle-accurate ZX Spectrum 128K instrument — load games/demos/snapshots and .ay/.pt3 chip-music, patch CV/gates into the Spectrum keyboard/joystick/ports, mangle the AY live while music plays, beeper/border/AY out as CV+audio (with reverb); hold switch DOWN at boot for OneBit (1-bit beeper synth). | 1.2.1<br>Released | C++ (Pico SDK / ComputerCard) | Andy Jenkinson (uglifruit) |
   ```

4. **Commit + push** to your fork, then open a **Pull Request** to
   `TomWhitwell/Workshop_Computer` `main`. Say in the description that it updates an
   existing card from 1.0.0 to 1.2.1 rather than adding a new one.

## Notes for the PR description

- **Two modes in one firmware**: ZX Spectrum 128K (normal boot) + OneBit 1-bit beeper
  synth (hold switch down at boot).
- **Standalone repo**: https://github.com/uglifruit/WorkshopZX (also in `Repository:`).
- **ROMs**: the Sinclair ROMs are © Amstrad, included under Amstrad's redistribution
  permission (see `reference/ROMS.md`) — same basis other Spectrum-related tools use.
- **No copyrighted games/music** are included; only the freely-usable `bakedasm` demo.
- **Builds** with the standard Pico SDK flow (`cmake -B build -G Ninja && cmake --build
  build`); ROM/snapshot headers are generated at build time by `tools/bin2h.py`.
- **Web app** is `interface.html` (WebMIDI, Chrome/Edge), hosted on GitHub Pages and
  referenced by the `Editor:` field in `info.yaml`.

## Housekeeping done for the release copy
- `info.yaml` carries the hosted GitHub Pages URL in `Editor:`, plus `contact:` and
  `discussion:` (see `documentation/info.yaml.md` upstream for the field list).
- Compiled firmware placed in `UF2/zx.uf2`.
- Overlays in `panels/`; ROMs in `roms/`; vendored Z80 core in `vendor/sz80/`.

## Keeping this folder in sync
The folder is a **snapshot**, not a symlink — it drifts as the main tree moves, and
that drift is invisible until someone flashes a stale `UF2/`. When cutting a release:

1. Copy the changed sources across (`ay.cpp`, `interface.html`, `machine.cpp`,
   `main.cpp`, `mapping.*`, `spectrum.*`, `webui.*`, `README.md`, `info.yaml`,
   `.gitignore`) and any renamed assets — `panels/` filenames must stay space-free
   or the rendered README images break.
2. Bump `Version:` in `info.yaml` **and** the `MSG_INFO` version bytes in
   `webui.cpp` together; the Web UI banner reads the latter, so they diverge
   silently if you only do one.
3. Rebuild in isolation (copy the folder out of the repo, `cmake -B build -G Ninja
   && cmake --build build`) and copy the resulting `zx.uf2` into `UF2/`.
   Two builds of identical sources differ in 4 bytes — a `__DATE__` string — so
   compare section sizes rather than hashes to confirm they match.
