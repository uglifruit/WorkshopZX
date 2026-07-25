// ZX — a ZX Spectrum 128K for the Workshop Computer.
//
// BOOT SLICE (v0.2.0): a vertical slice that boots the real 128K ROM to its menu.
//   * Core 1 free-runs the Z80 + 128K memory/paging + ULA, paced to 3.5MHz.
//   * Core 0's 48kHz ProcessSample() latches the beeper to Pulse Out 1 and the
//     border to CV Out 1 / the LED bar, and advances a time reference the CPU
//     core uses to pace itself.
// AY sound, snapshots, the input-mapping engine and the Web UI bolt on next; this
// slice exists to prove CPU + memory + paging + timing + the two-core split.
//
// See spectrum.h for the machine model and CrossCore (the only shared state).

#include "ComputerCard.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/vreg.h"   // vreg_set_voltage for the overclock

#include "spectrum.h"
#include "machine.h"
#include "mapping.h"
#include "webui.h"
#include "snapshot_default.h" // pulls in snapshot_data.h if present, else empty
                              // (snapshot_z80[], snapshot_z80_len, ZX_HAVE_SNAPSHOT)

using namespace zx;

// The emulated machine + CPU runner. Single instances, reached from core 1 via
// ComputerCard::ThisPtr() (the repo's second_core idiom).
static Spectrum gSpectrum;
static Machine  gMachine;
static Mapper   gMapper;
static WebUI    gWebUI;

class ZXCard : public ComputerCard
{
public:
	ZXCard()
	{
		gMapper.LoadDefaults();   // light table setup — safe in the constructor
		// Only launch the second core here. Do NOT do emulator setup
		// (Attach/Reset — which wires ROM pointers and clears 128KB of RAM) in
		// this constructor: it runs during ComputerCard's own construction,
		// before Run()/hardware init, and that heavy work wedged the chip.
		// Instead, core 1 constructs the emulator itself (proven by the STAGE 2+
		// diagnostics, which all set up the machine on core 1 and work).
		multicore_launch_core1(core1);
	}

	// Core-1 entry point. Does NOT use ThisPtr() (null before Run()); touches
	// only the file-scope gSpectrum/gMachine globals.
	static void core1()
	{
		gMachine.Attach(&gSpectrum);   // set up the emulator ON core 1
		gMachine.Reset();
		// Boot the baked-in snapshot if one is present; otherwise leave the
		// machine on the 128K menu (a game can be uploaded live over the Web UI).
		if (ZX_HAVE_SNAPSHOT)
			LoadSnapshot(gMachine, snapshot_z80, snapshot_z80_len);
		gWebUI.Init(&gSpectrum, &gMapper);   // USB-MIDI/SysEx Web UI on this core
		EmulationCore();
	}

	// -------- Core 1: free-running Z80 + ULA, paced to real Spectrum time -----
	static void EmulationCore()
	{
		// Run in chunks and pace to wall-clock so 70908 T-states take ~20ms
		// (one 50Hz frame). We derive elapsed emulated time from how many 48kHz
		// audio samples core 0 has counted, which keeps the Spectrum locked to
		// audio time (and hence to the outputs) rather than to core1's own speed.
		constexpr uint32_t tStatesPerSample = kCpuHz / 48000; // ~73 T-states
		uint32_t lastSample = 0;

		while (true)
		{
			// Service USB every iteration. Cheap when idle; this keeps the Web UI
			// responsive while running, and gets full attention while paused.
			gWebUI.Task();

			// Snapshot uploads are STAGED, not applied immediately: an uploaded
			// image is loaded fresh (registers/flags/PC) only when we return to
			// run mode. Remapping-only sessions never disturb the running machine.
			static bool wasPaused = false;
			bool nowPaused = gSpectrum.xc.paused;
			if (wasPaused && !nowPaused && gWebUI.SnapshotReady())
			{
				// Leaving pause with a freshly uploaded snapshot. The browser has
				// already decoded it: RAM banks were written directly as pages
				// arrived, so we just apply the register/paging state. (No Reset()
				// — that would wipe the RAM the browser just filled.)
				gWebUI.ApplyDecoded(&gMachine);
				gWebUI.ClearSnapshot();
				lastSample = gSpectrum.xc.sampleCount;
				wasPaused = false;
				continue;
			}
			wasPaused = nowPaused;

			uint32_t now = gSpectrum.xc.sampleCount;
			uint32_t elapsed = now - lastSample; // samples since we last ran
			lastSample = now;

			// Reset+reload on request (currently unbound; kept for future use,
			// e.g. a Web-UI reset button).
			if (gSpectrum.xc.resetRequest)
			{
				gMachine.Reset();
				if (ZX_HAVE_SNAPSHOT)
					LoadSnapshot(gMachine, snapshot_z80, snapshot_z80_len);
				gSpectrum.xc.resetRequest = false;
				lastSample = gSpectrum.xc.sampleCount;
				continue;
			}
			// Paused (switch Up): don't run the CPU; spin servicing USB hard so
			// uploads/remaps are fast. This is the intended time to use the Web UI.
			if (gSpectrum.xc.paused)
			{
				for (int k = 0; k < 50; k++) gWebUI.Task();
				lastSample = gSpectrum.xc.sampleCount; // don't accumulate backlog
				continue;
			}

			if (elapsed == 0)
			{
				tight_loop_contents(); // wait for core 0 to advance time
				continue;
			}
			// PERF METER: if elapsed regularly hits the cap, core 1 can't keep up
			// with real-time (running the Spectrum in slow motion). Record the
			// worst-case backlog so core 0 can show it on the LEDs.
			if (elapsed >= gSpectrum.xc.maxElapsed) gSpectrum.xc.maxElapsed = elapsed;
			if (elapsed > 96) elapsed = 96;

			gMachine.Run(elapsed * tStatesPerSample);

			// Publish a heartbeat + paging state for core 0 to show on LEDs.
			// (LED hardware is driven only from core 0 to avoid contention.)
			gSpectrum.xc.emuAlive++;
		}
	}

	// -------- Core 0: 48kHz I/O ----------------------------------------------
	// Forced into RAM (like OneBit): core 1 saturates the flash XIP bus running
	// the Z80, which would otherwise starve a flash-resident audio callback.
	virtual void __not_in_flash_func(ProcessSample)()
	{
		// Switch (while running):
		//   Up     -> PAUSE the emulator (freeze)
		//   Middle -> RUN (normal rest position)
		//   Down   -> a mappable KEYPRESS source (held while down)
		Switch sw = SwitchVal();
		gSpectrum.xc.paused = (sw == Switch::Up);

		// --- Evaluate the mapping sources from the jacks + switch --------------
		// A jack only triggers if something is actually PLUGGED IN (normalisation
		// probe). This stops floating/unpatched CV/Audio inputs pressing keys.
		// For patched analog inputs, the source is active while the value is above
		// the per-mapping threshold (comparator). Pulses are digital gates.
		bool srcActive[SRC_COUNT];
		srcActive[SRC_PULSE1] = Connected(Input::Pulse1) && PulseIn1();
		srcActive[SRC_PULSE2] = Connected(Input::Pulse2) && PulseIn2();
		srcActive[SRC_CV1]    = Connected(Input::CV1)    && CVIn1()    >= gMapper.GetMapping(SRC_CV1).threshold;
		srcActive[SRC_CV2]    = Connected(Input::CV2)    && CVIn2()    >= gMapper.GetMapping(SRC_CV2).threshold;
		srcActive[SRC_AUDIO1] = Connected(Input::Audio1) && AudioIn1() >= gMapper.GetMapping(SRC_AUDIO1).threshold;
		srcActive[SRC_AUDIO2] = Connected(Input::Audio2) && AudioIn2() >= gMapper.GetMapping(SRC_AUDIO2).threshold;
		srcActive[SRC_SWITCH] = (sw == Switch::Down);
		gMapper.Apply(srcActive, gSpectrum);

		// Latch emulator outputs (written by core 1) to the hardware.
		PulseOut1(gSpectrum.xc.beeper);           // beeper
		PulseOut2(gSpectrum.xc.mic);              // MIC/tape
		AudioOut2(gSpectrum.xc.aySample);         // AY (silent in boot slice)

		// Border colour -> CV Out 1 (stepped voltage) + left LED column as a bar.
		uint8_t b = gSpectrum.xc.border;
		CVOut1(int16_t((b * 2047) / 7));
		LedBrightness(0, (b & 1) ? 4095 : 0);   // border bit 0
		LedBrightness(2, (b & 2) ? 4095 : 0);   // border bit 1
		LedBrightness(4, (b & 4) ? 4095 : 0);   // border bit 2

		// Right LED column = status:
		//   LED 1: CPU heartbeat (blinks while the Z80 runs)
		//   LED 3: paging latch locked
		//   LED 5: recent beeper activity (latched blink after any toggle)
		static uint32_t lastAlive = 0, blink = 0;
		uint32_t alive = gSpectrum.xc.emuAlive;
		if (alive != lastAlive) { blink++; lastAlive = alive; }
		LedOn(1, (blink >> 11) & 1);

		static bool lastBeep = false;
		static uint32_t beepSeen = 0;
		if (gSpectrum.xc.beeper != lastBeep) { beepSeen = 24000; lastBeep = gSpectrum.xc.beeper; }
		if (beepSeen) beepSeen--;
		LedOn(5, beepSeen != 0);
		LedOn(3, gSpectrum.mem.pagingLatch & 0x20);   // paging locked

		// Left column: border colour bar (restored now perf is confirmed OK).
		LedBrightness(0, (b & 1) ? 4095 : 0);
		LedBrightness(2, (b & 2) ? 4095 : 0);
		LedBrightness(4, (b & 4) ? 4095 : 0);

		// TODO(boot-slice+1): sample inputs and run the mapping engine here,
		// writing gSpectrum.kbd.rows[] / xc.kempston / xc.earIn.

		// Advance the shared time reference the emulation core paces against.
		gSpectrum.xc.sampleCount++;
	}
};

int main()
{
	// Overclock for emulation headroom. 200MHz needs a small core-voltage bump;
	// 200MHz is not a multiple of 48MHz (mild audio-input noise tradeoff), but the
	// emulator needs the raw MHz more than pristine ADC noise here. 192MHz (=4x48)
	// is the noise-clean alternative if this proves marginal.
	vreg_set_voltage(VREG_VOLTAGE_1_15);
	set_sys_clock_khz(200000, true);

	ZXCard zx;
	zx.EnableNormalisationProbe();
	zx.Run();
}
