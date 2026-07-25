// ay.cpp — AY-3-8912 sound chip.
//
// BOOT-SLICE STUB: registers are stored so the ROM's AY writes don't crash, but
// no sound is generated yet (Render returns silence). Full 3-channel square +
// noise + envelope generation is a later milestone (-> Audio Out 2).

#include "spectrum.h"

namespace zx {

void AY::Reset()
{
	for (int i = 0; i < 16; i++) reg[i] = 0;
	selected = 0;
}

void AY::Write(uint8_t r, uint8_t v)
{
	reg[r & 15] = v;
	// TODO: latch tone/noise periods, envelope, mixer, volumes into generators.
}

int16_t AY::Render(uint32_t /*tstates*/)
{
	// TODO: advance tone/noise/envelope generators by `tstates` and mix.
	return 0; // silence for now
}

} // namespace zx
