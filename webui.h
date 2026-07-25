// webui.h — USB-MIDI/SysEx Web UI transport + protocol for ZX.
//
// The card enumerates as a USB-MIDI device; a browser (WebMIDI) talks to it over
// SysEx. This lets you upload .z80 snapshots and edit the input mapping live,
// with no app install and no reflashing.
//
// SysEx payloads are 7-bit (each byte 0-127). Binary data (a snapshot, the raw
// mapping table) is 7-bit encoded: groups of 7 source bytes become 8 septets,
// where the 8th carries the high bits. Decoding reverses this.
//
// Runs on core 1 (alongside the emulator). USB is polled between emulation
// batches; when the machine is PAUSED (switch Up) core 1 gives USB its full
// attention, which is the intended time to upload/remap.

#pragma once
#include <cstdint>
#include "spectrum.h"
#include "mapping.h"

namespace zx {

// SysEx message IDs (first payload byte). fw = firmware, ui = browser.
enum : uint8_t {
	MSG_HELLO         = 0x01, // ui->fw: announce; fw replies MSG_INFO
	MSG_INFO          = 0x02, // fw->ui: firmware version + capabilities
	MSG_SNAP_BEGIN    = 0x10, // ui->fw: start upload; carries total length (enc)
	MSG_SNAP_CHUNK    = 0x11, // ui->fw: a chunk of 7-bit-encoded snapshot bytes
	MSG_SNAP_END      = 0x12, // ui->fw: upload complete -> load+run
	MSG_SNAP_ACK      = 0x13, // fw->ui: chunk received / status
	MSG_MAP_GET       = 0x20, // ui->fw: please send the current mapping
	MSG_MAP_REPORT    = 0x21, // fw->ui: the current mapping table
	MSG_MAP_SET       = 0x22, // ui->fw: set the mapping table
	MSG_KEY           = 0x40, // ui->fw: keyboard passthrough. payload: keyIndex,down
	MSG_KEY_RESET     = 0x41, // ui->fw: release all passthrough keys
	MSG_STATUS        = 0x30, // fw->ui: running/paused, current activity
};

// Manufacturer ID 0x7D = "prototyping / private use" (same as the repo example).
constexpr uint8_t ZX_MANUFACTURER_ID = 0x7D;

// 7-bit encode/decode. Return bytes written. dst must be large enough:
// encoded length = ceil(srcLen/7)*8; decoded length <= (srcLen/8)*7.
uint32_t Encode7bit(const uint8_t *src, uint32_t srcLen, uint8_t *dst, uint32_t dstMax);
uint32_t Decode7bit(const uint8_t *src, uint32_t srcLen, uint8_t *dst, uint32_t dstMax);

// The Web UI handler. Owns the USB init, the SysEx parser, and the staging
// buffer for an in-progress snapshot upload. Lives on core 1.
class WebUI {
public:
	void Init(Spectrum *spec, Mapper *mapper);  // call once on core 1
	void Task();                                // call frequently on core 1

	// True when a complete snapshot has arrived and is waiting to be loaded.
	// The emulation loop checks this, loads gStagingSnapshot, and clears it.
	bool SnapshotReady() const { return snapReady_; }
	const uint8_t *SnapshotData() const { return staging_; }
	uint32_t SnapshotLen() const { return snapLen_; }
	void ClearSnapshot() { snapReady_ = false; }

private:
	void SendSysEx(const uint8_t *data, uint32_t size);
	void OnSysEx(const uint8_t *data, uint32_t size); // decoded payload (no F0/id/F7)
	void ParseStream(const uint8_t *buf, uint32_t n);

	Spectrum *spec_ = nullptr;
	Mapper   *mapper_ = nullptr;

	// SysEx reassembly
	static constexpr uint32_t kRxBuf = 128;
	static constexpr uint32_t kMsgBuf = 512;   // one decoded SysEx message
	uint8_t  msg_[kMsgBuf];
	uint32_t msgLen_ = 0;
	bool     inSysex_ = false;

	// Snapshot staging. A .z80 is COMPRESSED, so even a 128K snapshot file is
	// well under this; 64KB is a safe cap that fits our RAM budget as a static
	// array (no risky 140KB malloc that would exceed free RAM and wedge core 1).
	static constexpr uint32_t kStagingMax = 64 * 1024;
	uint8_t   staging_[kStagingMax];   // static, sized at link time
	uint32_t  snapLen_ = 0;            // bytes received so far / final length
	uint32_t  snapExpected_ = 0;
	bool      snapReady_ = false;
};

} // namespace zx
