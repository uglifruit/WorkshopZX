// reverb.cpp — fixed-point Freeverb-style reverb (see reverb.h).

#include "reverb.h"
#include <cstring>

namespace zx {

// out-of-line constexpr array definitions (C++14)
constexpr int Reverb::kCombLen[];
constexpr int Reverb::kAllpassLen[];

// Fixed-point coefficients (Q15). Freeverb defaults: room ~0.84, damp ~0.2.
static constexpr int32_t kFeedback = 27525; // ~0.84 * 32768 (comb feedback)
static constexpr int32_t kDamp1    = 6554;  // ~0.20 * 32768
static constexpr int32_t kDamp2    = 32768 - kDamp1;
static constexpr int32_t kApFeed   = 16384; // 0.5 allpass feedback

void Reverb::Reset()
{
	for (int c = 0; c < kNumCombs; c++)
	{
		memset(comb_[c], 0, sizeof(comb_[c]));
		combStore_[c] = 0;
		combIdx_[c] = 0;
	}
	for (int a = 0; a < kNumAllpass; a++)
	{
		memset(allpass_[a], 0, sizeof(allpass_[a]));
		apIdx_[a] = 0;
	}
}

int16_t Reverb::Process(int16_t in)
{
	int32_t input = in;
	int32_t out = 0;

	// Parallel comb filters with damping lowpass in the feedback path.
	for (int c = 0; c < kNumCombs; c++)
	{
		int len = kCombLen[c];
		int16_t y = comb_[c][combIdx_[c]];
		out += y;
		// Damping lowpass: store = y*damp2 + store*damp1
		combStore_[c] = (int32_t(y) * kDamp2 + combStore_[c] * kDamp1) >> 15;
		int32_t v = input + ((combStore_[c] * kFeedback) >> 15);
		if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
		comb_[c][combIdx_[c]] = int16_t(v);
		if (++combIdx_[c] >= len) combIdx_[c] = 0;
	}
	// Average the combs.
	out >>= 2;

	// Series allpass filters.
	for (int a = 0; a < kNumAllpass; a++)
	{
		int len = kAllpassLen[a];
		int16_t buf = allpass_[a][apIdx_[a]];
		int32_t y = int32_t(buf) - out;                 // allpass output
		int32_t v = out + ((int32_t(buf) * kApFeed) >> 15);
		if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
		allpass_[a][apIdx_[a]] = int16_t(v);
		if (++apIdx_[a] >= len) apIdx_[a] = 0;
		out = y;
	}

	// Clamp to ~12-bit output range (AudioOut expects -2048..2047).
	if (out > 2047) out = 2047; else if (out < -2048) out = -2048;
	return int16_t(out);
}

} // namespace zx
