# ZX — a ZX Spectrum for the Music Thing Workshop Computer

A program card for the **Music Thing Modular Workshop System Computer** that runs a
**cycle-accurate ZX Spectrum 128K** (Z80 CPU + ULA + banked memory + AY sound chip)
in real time, and turns the module's jacks, knobs and switch into a **patchable
control surface** for the running Spectrum — a two-way bridge between the modular
and the machine.

Play games and demos, load `.z80`/`.sna` snapshots or `.ay` chip-music files over a
browser, drive the Spectrum keyboard from CV/gates or your laptop, read patched CV
back as data inside a Spectrum program, and take the beeper, border and AY out as
CV and audio. It even carries **OneBit** (its 1-bit beeper-synth sibling) as an
alternate boot mode.

Runs on a real Workshop Computer. Built on the RP2040.

---

## Features

**The machine**
- **Cycle-accurate Z80** at the true 3.5469 MHz (128K) emulated rate.
- **128K Spectrum**: 128 KB banked RAM, 32 KB ROM, `0x7FFD` paging, the ULA
  (`0xFE` beeper/border), and the **AY-3-8912** sound chip (3 tones + noise + 8
  envelope shapes).
- **48K snapshots** run too (faked on the 128K model with the 48 BASIC ROM paged in).

**Loading**
- **`.z80`** (v1/v2/v3, 48K & 128K, RLE-decompressed) and **`.sna`** (48K/128K)
  snapshots.
- **`.ay`** chip-music files (ZXAY/EMUL) — the tune's own Z80 player runs on the
  emulated CPU, driven by a self-contained IM2 harness.
- Snapshots decode **in the browser** and stream to the card, so any size fits with
  no big buffer on the RP2040.
- A **baked-in default** program (built from `snapshots/bakedasm.z80`) boots at
  power-on; if none is present, the card boots the 128K menu.

**Audio out**
- **Beeper** → Pulse Out 1.
- **AY** (128K game/`.ay` music) → Audio Out 2.
- **Reverb** (Freeverb-style) of the beeper+AY mix → Audio Out 1, wet/dry on **Knob X**.

**CV / control out**
- **Border** → CV Out 1 (as a voltage).
- **Memory probe** → CV Out 2: the value of any RAM address (0–255 → 0–5 V), chosen
  in the Web UI (default 16384 = screen memory, so it flickers with the display).

**Inputs as a control surface** — every jack + the switch can be mapped (in the Web
UI) to something the Spectrum reads:
- a **key** in the keyboard matrix (held while the input is active),
- a **Kempston joystick** direction (Up/Down/Left/Right/Fire),
- or a **Z80-readable port** — the input's live 0–255 value at a fixed port (read
  with `IN A,(port)`), so a Spectrum program can read patched CV as data.

**Web UI** (USB-MIDI / WebMIDI SysEx — Chrome/Edge, no install):
- upload `.z80` / `.sna` / `.ay`,
- remap inputs on a clickable QWERTY keyboard (+ Kempston + Port targets),
- **laptop keyboard passthrough** — type into the Spectrum,
- set the CV Out 2 memory-probe address.

**Knobs & switch**
- **Knob Main** — emulation speed, with a centre **deadzone at 100%** (speed shifts
  emulated pitch, so the detent holds true speed). Works for snapshots *and* `.ay`.
- **Knob X** — reverb wet/dry.
- **Knob Y** — readable by the Z80 at **port 0x5F** (`IN 95` in BASIC).
- **Switch** — Up = pause, Middle = run, Down = a mappable momentary keypress.

**OneBit alternate boot mode** — hold the momentary switch **Down** at power-on to
boot **OneBit** (Andy's 1-bit beeper-synth card) instead of the Spectrum. Adjacent
cards, one firmware.

---

## Panel

| Control | Function |
|---------|----------|
| **Knob Main** | Emulation speed (centre deadzone = exactly 100% / real time) |
| **Knob X** | Reverb wet/dry (on Audio Out 1) |
| **Knob Y** | Readable by the Z80 at port `0x5F` |
| **Switch Up** | Pause |
| **Switch Middle** | Run |
| **Switch Down** | Mappable momentary keypress · *(held at power-on → OneBit)* |
| **Pulse In 1/2** | Mapped inputs (key / Kempston / port) |
| **CV In 1/2** | Mapped inputs (comparator → key/Kempston, or value → port) |
| **Audio In 1/2** | Mapped inputs (as above) |
| **Pulse Out 1** | **Beeper** |
| **Pulse Out 2** | MIC / tape line |
| **CV Out 1** | **Border** voltage |
| **CV Out 2** | **Memory probe** — a chosen RAM byte as 0–5 V |
| **Audio Out 1** | **Reverb** (beeper + AY), wet/dry on Knob X |
| **Audio Out 2** | **AY-3-8912** sound |
| **LEDs** | Left: mode (128K / 48K / AY). Right: heartbeat / correct-speed / beeper |

**Mapped input ports** (read with `IN A,(port)` when a source is mapped to *Port*):
Knob Y `0x5F`, CV In 1 `0x6F`, CV In 2 `0x7F`, Audio In 1 `0x8F`, Audio In 2 `0x9F`,
Pulse In 1 `0xAF`, Pulse In 2 `0xBF`, Switch `0xCF`.

---

## How it runs

`ProcessSample()` runs at 48 kHz — far too slow to *be* the 3.5 MHz Z80 clock — so
work is split across the RP2040's two cores (the repo's `second_core` pattern):

| Core | Job |
|------|-----|
| **Core 1** | Free-runs the Z80 + ULA + AY, paced to emulated Spectrum time; also services USB. |
| **Core 0** | 48 kHz I/O: latches beeper/AY/border to the outputs, samples the inputs, runs the mapping engine + reverb, presents keyboard/joystick/port state to the Z80. |

They meet through a small lock-free `CrossCore` struct (single-writer per field).
An instruction-stepped Z80 core keeps it real-time from flash. Emulator setup runs
**on core 1**, never in the ComputerCard constructor (which would wedge the chip).

---

## Building

Raspberry Pi Pico SDK (2.2.0):

```sh
cmake -B build -G Ninja
cmake --build build
```

Produces `build/zx.uf2`. Hold BOOTSEL on the Computer's RP2040 and drop it on the
mounted drive. The ROM headers and the baked-snapshot header are generated from
`roms/*.rom` and `snapshots/bakedasm.z80` at build time (Python + `tools/bin2h.py`).

**Bake your own default snapshot:**
```sh
python tools/bin2h.py path/to/game.z80 snapshot_z80 snapshot_data.h
```
Copyrighted game/`.ay` files are **not** committed; only the freely-usable
`bakedasm` demo is.

---

## Credits

This card stands on a lot of other people's work. Thank you, all of you.

**Hardware & framework**
- **Music Thing Modular Workshop System Computer** and the **ComputerCard** library —
  Tom Whitwell / Music Thing Modular, ComputerCard by **Chris Johnson**. The whole
  card is built on this. (MIT-licensed, header-only.)
- **Raspberry Pi Pico SDK** / RP2040 — Raspberry Pi Ltd.

**Emulation**
- **Z80 CPU core** — [`superzazu/z80`](https://github.com/superzazu/z80) by
  **Nicolas Allemand** (MIT). Instruction-stepped, passes zexdoc/zexall. Vendored in
  `vendor/sz80/` with its licence; the only modification is a `port16` field so the
  I/O callback receives the full 16-bit port (marked `MOD(ZX)` in-file).
- **Sinclair ZX Spectrum ROMs** — © **Amstrad plc**, redistributed with Amstrad's
  kind permission (see `reference/ROMS.md`). Unmodified images.
- Format & hardware references: **World of Spectrum** and the **Sinclair Wiki**
  (`.z80`/`.sna`/`.ay` formats, `0x7FFD` paging, ULA port decode, AY, Kempston);
  Sean Young, *Z80 Undocumented Documented*; Chris Smith, *The ZX Spectrum ULA*;
  the **FUSE** emulator and **zexall** test suite as verification references;
  **Project AY** for the `.ay` format.

**Sibling card**
- **OneBit** (bundled as the alternate boot mode) ports classic ZX-Spectrum 1-bit
  "beeper" engines. Credits (all via **Beepola** by Chris Cowley): **Joffa Smith**
  (PlipPlop / Special FX), **Shiru** (Tritone / Qchan / Phaser / Huby), **Jason C
  Brooke** (Savage), **Mark Alexander** (Music Box), **Saa Puica** (Music Studio),
  and above all **utz** for the 1-bit routine tutorials and community.

**This card**
- ZX for the Workshop Computer — **Andy Jenkinson** (**uglifruit**), 2026.

The reverb is a fixed-point take on **Jezar at Dreampoint**'s *Freeverb* topology.

## Licence

The card's own source is Andy's. Vendored components keep their own licences
(`vendor/sz80/LICENSE`, `ComputerCard.h`). The Sinclair ROMs are Amstrad's, used by
permission. No copyrighted game or music files are included.
