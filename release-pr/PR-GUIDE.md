# Submitting ZX to the Workshop_Computer repo (PR guide)

This folder (`release-pr/`) holds a **self-contained release-folder snapshot** of the
ZX + OneBit card, packaged to match the convention of Andy's existing cards
(`57_glitch`, `60_markov`): full source, ROMs, the compiled firmware in `UF2/`, the
web app, panel overlays, `info.yaml`, and `README.md`. It has been **verified to build
from scratch in isolation**.

## Steps

1. **Fork** `TomWhitwell/Workshop_Computer` on GitHub (if not already) and clone your
   fork.

2. **Pick the release number.** Numbers up to 99 are sparsely assigned; the next free
   gaps include **61, 62, 63, 65, 68, 70…**. Andy's cards are 57 and 60, so **61** is
   the natural next — but Tom may prefer to assign one. Rename the folder accordingly:
   ```
   release-pr/NN_ZX_Spectrum   ->   releases/61_ZX_Spectrum   (or whatever number)
   ```

3. **Copy the folder** into the fork's `releases/`:
   ```sh
   cp -r release-pr/NN_ZX_Spectrum <fork>/releases/61_ZX_Spectrum
   ```

4. **Add the table row** to `releases/README.md` (keep the numeric order). Row:

   ```
   | 61_ZX_Spectrum | A cycle-accurate ZX Spectrum 128K instrument — load games/demos/snapshots and .ay/.pt3 chip-music, patch CV/gates into the Spectrum keyboard/joystick/ports, mangle the AY live while music plays, beeper/border/AY out as CV+audio (with reverb); hold switch DOWN at boot for OneBit (1-bit beeper synth). | 1.2.0<br>Released | C++ (Pico SDK / ComputerCard) | Andy Jenkinson (uglifruit) |
   ```
   (replace `61` with the assigned number in both the folder name and this row)

5. **Commit + push** to your fork, then open a **Pull Request** to
   `TomWhitwell/Workshop_Computer` `main`.

## Notes for the PR description

- **Two modes in one firmware**: ZX Spectrum 128K (normal boot) + OneBit 1-bit beeper
  synth (hold switch down at boot).
- **Standalone repo**: https://github.com/uglifruit/WorkshopZX (also in `Repository:`).
- **ROMs**: the Sinclair ROMs are © Amstrad, included under Amstrad's redistribution
  permission (see `reference/ROMS.md`) — same basis other Spectrum-related tools use.
- **No copyrighted games/music** are included; only the freely-usable `bakedasm` demo.
- **Builds** with the standard Pico SDK flow (`cmake -B build -G Ninja && cmake --build
  build`); ROM/snapshot headers are generated at build time by `tools/bin2h.py`.
- **Web app** is `interface.html` (WebMIDI, Chrome/Edge) — no hosted editor URL yet, so
  no `Editor:` field; it's described in the summary. Host it later and add the field if
  you like.

## Housekeeping done for the release copy
- `Editor: interface.html` removed from `info.yaml` (that field expects a hosted URL).
- Compiled firmware placed in `UF2/zx.uf2`.
- Overlays in `panels/`; ROMs in `roms/`; vendored Z80 core in `vendor/sz80/`.
