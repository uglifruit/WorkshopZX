# ZX — a ZX Spectrum 128K for the Workshop Computer

A **Music Thing Workshop Computer** card that runs a **cycle-accurate ZX Spectrum
128K** (Z80 CPU + ULA + banked memory + AY sound chip) inside the audio loop, and
turns the Computer's jacks and switch into a **patchable control surface** for the
running Spectrum program.

Its sibling card **OneBit** emulates the *sound* of the Spectrum — the 1-bit beeper
tricks. **ZX** emulates the whole *machine*, and then does something a real Spectrum
never could: it lets you patch modular CV, gates, and audio into the Spectrum's
keyboard and joystick ports.

## What it is

- A **timing-accurate Z80** running at the real 3.5 MHz (128K: 3.5469 MHz) emulated
  rate, executing from RP2040 SRAM.
- A **128K Spectrum**: 128 KB banked RAM + 32 KB ROM, the `0x7FFD` paging latch, the
  ULA (`0xFE`) beeper/border/EAR, and the **AY-3-8912** sound chip.
- **Snapshot loading** (`.sna`, `.z80`) so you can run real programs — baked into the
  firmware and/or uploaded live over USB.
- A **Web UI** (USB, no install — WebMIDI/SysEx to a local HTML page) that:
  1. **uploads snapshots** to the running card, and
  2. **maps the physical inputs** (the 6 jacks + momentary switch) to **Spectrum
     keypresses**, Kempston joystick directions, or raw input ports — with a
     clickable Spectrum keyboard graphic.
- **Audio out**: beeper on Pulse Out 1, AY (128K music) on Audio Out 2.
- **Tape loading** from Audio In (`LOAD ""`) as a **stretch goal** — snapshots are the
  primary path, so this is a bonus, not a dependency.

## How it runs (architecture)

`ProcessSample()` runs at **48 kHz**, which is far too slow to *be* the Z80 clock (one
Spectrum frame is 70,908 T-states at 3.5469 MHz). So the work is split across the
RP2040's two cores, following the repo's `second_core` pattern:

| Core | Job |
|------|-----|
| **Core 1** | Free-runs the **Z80 + ULA + AY**, pacing itself to emulated Spectrum time. Also handles **USB** (TinyUSB) for the Web UI. |
| **Core 0** | `ProcessSample()` at 48 kHz: latches the **beeper/AY** state to the outputs, samples the **inputs**, runs the **mapping table**, and presents the resulting key-matrix / joystick / port state to the Z80. |

The two cores meet through a few `volatile` shared latches (beeper bit, AY register
render, keyboard matrix rows, border) — single-writer per field, so no locks needed,
exactly as in `second_core`.

## Inputs as a control surface (the mapping engine)

Every physical input can be mapped, via the Web UI, to something the Spectrum program
reads:

**Sources:** Audio In 1/2, CV In 1/2, Pulse In 1/2, momentary Switch.

**Targets:**
- **A key in the 8×5 keyboard matrix** — "hold this bit low while the source is
  active" *is* a keypress. e.g. Pulse In 1 → SPACE, CV In 1 over a threshold → Q,
  Switch → ENTER.
- **A Kempston joystick bit** (port `0x1F`: `000FUDLR`) for the analog inputs.
- **A raw input port** value a program polls directly.

Pulse/switch sources map through **edge or level** logic; CV/audio sources through a
**comparator with threshold + hysteresis** (or a continuous value for port targets).
The mapping table is small and serialisable, so the same USB transport carries both
mappings and snapshots.

## Panel (defaults — most is remappable)

| Control | Function |
|---------|----------|
| **Knob Main** | Emulation speed / turbo (default = real-time ×1) |
| **Knob X** | Snapshot bank select (baked-in games) |
| **Knob Y** | Assignable control (mapped in the Web UI) |
| **Switch Up** | Run |
| **Switch Mid** | Pause |
| **Switch Down** | NMI / reset (momentary) — configurable |
| **Pulse In 1/2** | Mapped inputs (default: keys / Kempston fire) |
| **CV In 1/2** | Mapped inputs (comparator → key, or continuous port) |
| **Audio In 1/2** | Mapped inputs; **Audio In 1** doubles as the tape EAR source (stretch) |
| **Pulse Out 1** | **Beeper** (ULA `0xFE` bit 4) |
| **Pulse Out 2** | MIC / tape-out, or assignable gate |
| **CV Out 1** | **Border voltage** (ULA `0xFE` bits 0–2), or assignable |
| **Audio Out 1** | Beeper PCM-density monitor |
| **Audio Out 2** | **AY-3-8912** sound output (128K music) |
| **LEDs** | Border bar + CPU/AY/paging activity |

## Status & roadmap

**v0.3.0 — running on hardware.** A baked-in 128K `.z80` snapshot boots and makes
sound out of Pulse Out 1 — the full chain (snapshot load → RLE decompress → 128K
paging → real-time Z80 → ULA beeper) works on a real Workshop Computer.

Done:
- [x] **Instruction-stepped Z80 core** — superzazu/z80 (MIT, passes zexall). Runs the
      Spectrum at real-time from flash. (A cycle-stepped core was tried first but was
      ~8× too slow on the RP2040.)
- [x] **Memory + ULA** — 128 KB banked RAM, 32 KB ROM, `0x7FFD` paging, `0xFE` ULA
      (port decodes verified against the Sinclair Wiki)
- [x] **Two-core split** — Z80/ULA free-run on core 1, 48 kHz I/O latch on core 0.
      (Note: emulator setup must happen on core 1, *not* in the constructor — doing
      heavy init in the ComputerCard constructor wedges the chip.)
- [x] **Snapshot loader** — `.z80` v1/v2/v3, 48K & 128K, with RLE decompression
- [x] **Beeper → Pulse Out 1**, border → CV Out 1 + LED bar

Next:
- [ ] **Input mapping engine** — jacks/switch → keyboard matrix / Kempston (so the
      machine can be driven, and menu/BASIC/keypress sounds play)
- [ ] **AY-3-8912** — 3 square channels + noise + envelopes → Audio Out 2
- [ ] **Multiple baked snapshots** selectable by Knob X
- [ ] **Web UI** — snapshot upload + clickable-keyboard input mapper (USB SysEx)
- [ ] **Contended memory timing** — optional, for games that depend on it
- [ ] **Tape-EAR loader** — `LOAD ""` from Audio In *(stretch goal)*

## Snapshots & games

The Sinclair ROMs are embedded (Amstrad permit redistribution — see
`reference/ROMS.md`). **Game snapshots are not** committed to this repo — they're
copyrighted. To bake one in:

```sh
python tools/bin2h.py path/to/game.z80 snapshot_z80 snapshot_data.h
```

then rebuild. Without a baked snapshot the card boots to the 128K menu, and you can
upload a `.z80` live from the browser (see `interface.html`).

## Building

Raspberry Pi Pico SDK (2.2.0), like the other Workshop cards:

```sh
cmake -B build -G Ninja
cmake --build build
```

Produces `build/zx.uf2`. Hold BOOTSEL and drop it on the mounted RP2040.

## References

- Zilog **Z80 User Manual**; Sean Young, **_Z80 Undocumented Documented_** (flags, timing,
  `MEMPTR`/WZ, undocumented opcodes)
- **FUSE** / **zexall** — Z80 test suites used to verify the core
- Chris Smith, **_The ZX Spectrum ULA_** — `0xFE`, border, beeper, contention
- **World of Spectrum** — `.sna` / `.z80` formats, `0x7FFD` paging, AY, Kempston
- Existing RP2040 Spectrum emulators (MCUME / ZX-family) as prior art that this fits well
  within the RP2040's timing and memory budget
