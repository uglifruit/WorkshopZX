// reverb.h — a small fixed-point Freeverb-style reverb.
//
// Runs on core 0 in ProcessSample (which otherwise just latches outputs, so it
// has spare CPU). Processes the mono (beeper + AY) mix and produces a wet signal
// for Audio Out 1; Knob X sets wet/dry. Uses ~16KB of int16 delay lines (static),
// well within the RAM budget.
//
// Structure: 4 parallel comb filters (with damping) summed, then 2 series
// allpass filters — the classic Schroeder/Freeverb topology, mono.

#pragma once
#include <cstdint>
#include "pico.h"   // __not_in_flash_func

namespace zx {

class Reverb {
public:
	void Reset();
	// Process one sample. `in` is a signed ~12-bit mono input; returns the wet
	// (reverberated) sample, also ~12-bit range. Caller mixes wet/dry.
	// RAM-resident (called from the 48kHz core-0 callback; keeps it off the flash
	// bus that core 1 saturates).
	int16_t __not_in_flash_func(Process)(int16_t in);

private:
	// Comb filter tunings (samples). Freeverb's 44.1kHz values scaled ~*48/44.1.
	static constexpr int kNumCombs = 4;
	static constexpr int kNumAllpass = 2;
	static constexpr int kCombLen[kNumCombs]    = { 1687, 1751, 1615, 1539 };
	static constexpr int kAllpassLen[kNumAllpass] = { 245, 605 };

	int16_t comb_[kNumCombs][1751];      // sized to the largest comb
	int32_t combStore_[kNumCombs];       // damping lowpass state
	int     combIdx_[kNumCombs];

	int16_t allpass_[kNumAllpass][605];  // sized to the largest allpass
	int     apIdx_[kNumAllpass];
};

} // namespace zx
