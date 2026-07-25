// mapping.h — the input-mapping engine.
//
// Turns the Workshop Computer's physical inputs (6 jacks + momentary switch) into
// things the running Spectrum program reads: keyboard-matrix bits, Kempston
// joystick bits, or (later) raw input ports. This is the "patchable control
// surface" idea — the whole point of ZX being a modular instrument rather than a
// Spectrum-in-a-box.
//
// Runs on core 0 (inside ProcessSample), writing gSpectrum.kbd.rows[] and
// xc.kempston, which core 1's ULA/port reads consult. The mapping table is small
// and will later be editable over USB from the Web UI.

#pragma once
#include <cstdint>
#include "spectrum.h"

namespace zx {

// Named Spectrum keys — indices into kKeyTable (spectrum.cpp). Order MUST match
// that table (row 0..7, each row's 5 bits low..high).
enum Keycode : uint8_t {
	KEY_CAPS=0, KEY_Z, KEY_X, KEY_C, KEY_V,
	KEY_A, KEY_S, KEY_D, KEY_F, KEY_G,
	KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T,
	KEY_1, KEY_2, KEY_3, KEY_4, KEY_5,
	KEY_0, KEY_9, KEY_8, KEY_7, KEY_6,
	KEY_P, KEY_O, KEY_I, KEY_U, KEY_Y,
	KEY_ENTER, KEY_L, KEY_K, KEY_J, KEY_H,
	KEY_SPACE, KEY_SYMSHIFT, KEY_M, KEY_N, KEY_B,
	KEY_COUNT,
	KEY_NONE = 0xFF,
};

// The six physical input jacks + the switch = the mapping sources.
enum Source : uint8_t {
	SRC_PULSE1, SRC_PULSE2,   // gates (digital)
	SRC_CV1, SRC_CV2,         // CV (analog, comparator)
	SRC_AUDIO1, SRC_AUDIO2,   // audio-rate (analog, comparator)
	SRC_SWITCH,               // momentary switch (Up/Down = active)
	SRC_COUNT,
};

// What a source drives.
enum TargetKind : uint8_t {
	TGT_NONE,
	TGT_KEY,        // press a Spectrum key (arg = Keycode)
	TGT_KEMPSTON,   // set a Kempston joystick bit (arg = bit mask 000FUDLR)
};

// One mapping: source -> target, with a threshold for analog sources.
struct Mapping {
	TargetKind kind;
	uint8_t    arg;        // Keycode, or Kempston bitmask
	int16_t    threshold;  // analog: activate when value >= threshold (12-bit signed)
	bool       invert;     // invert the active sense
};

// Kempston joystick bits (port 0x1F: 000FUDLR).
enum : uint8_t {
	KEMP_RIGHT=0x01, KEMP_LEFT=0x02, KEMP_DOWN=0x04, KEMP_UP=0x08, KEMP_FIRE=0x10,
};

// The mapping engine: holds the table, evaluates it each sample.
class Mapper {
public:
	void LoadDefaults();                 // sensible starting patch
	void SetMapping(Source s, const Mapping &m) { table_[s] = m; }
	const Mapping &GetMapping(Source s) const { return table_[s]; }

	// Called each 48kHz sample on core 0. `srcActive[]` are the evaluated
	// on/off states of the seven sources (computed by the caller from the jacks,
	// which need ComputerCard access). Writes kbd.rows[] and xc.kempston.
	void Apply(const bool srcActive[SRC_COUNT], Spectrum &spec);

	// Keyboard passthrough (laptop keys via the Web UI). These are OR-combined
	// with the jack mappings in Apply(), so both work at once. keyIndex is a
	// kKeyTable index (0..KEY_COUNT-1). Called from core 1 (USB); the bitset is
	// volatile and read by core 0 in Apply().
	void PassthroughKey(uint8_t keyIndex, bool down);
	void PassthroughReset();

private:
	Mapping table_[SRC_COUNT];
	// One bit per matrix key held via passthrough (40 keys -> 2x uint32_t).
	volatile uint32_t passthrough_[2] = {0, 0};
};

} // namespace zx
