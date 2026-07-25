// webui.cpp — USB-MIDI/SysEx Web UI transport + protocol implementation.

#include "webui.h"
#include "tusb.h"
#include <cstdlib>
#include <cstring>

namespace zx {

// --- 7-bit codec ------------------------------------------------------------
// Encode groups of 7 bytes into 8 septets: first septet holds the 7 high bits
// (bit i = high bit of source byte i), followed by the 7 low-7-bit values.
uint32_t Encode7bit(const uint8_t *src, uint32_t srcLen, uint8_t *dst, uint32_t dstMax)
{
	uint32_t di = 0;
	for (uint32_t i = 0; i < srcLen; i += 7)
	{
		uint32_t n = (srcLen - i < 7) ? (srcLen - i) : 7;
		if (di + 1 + n > dstMax) break;
		uint8_t high = 0;
		for (uint32_t j = 0; j < n; j++)
			if (src[i + j] & 0x80) high |= (1 << j);
		dst[di++] = high;
		for (uint32_t j = 0; j < n; j++)
			dst[di++] = src[i + j] & 0x7F;
	}
	return di;
}

uint32_t Decode7bit(const uint8_t *src, uint32_t srcLen, uint8_t *dst, uint32_t dstMax)
{
	uint32_t di = 0;
	uint32_t i = 0;
	while (i < srcLen)
	{
		uint8_t high = src[i++];
		for (uint32_t j = 0; j < 7 && i < srcLen && di < dstMax; j++)
		{
			uint8_t v = src[i++] & 0x7F;
			if (high & (1 << j)) v |= 0x80;
			dst[di++] = v;
		}
	}
	return di;
}

// --- USB send ---------------------------------------------------------------
static void MidiWriteBlocking(const uint8_t *data, uint32_t size)
{
	uint32_t sent = 0;
	while (sent < size)
	{
		uint32_t n = tud_midi_stream_write(0, data + sent, size - sent);
		sent += n;
		if (!n) tud_task();
	}
}

void WebUI::SendSysEx(const uint8_t *data, uint32_t size)
{
	uint8_t header[] = { 0xF0, ZX_MANUFACTURER_ID };
	uint8_t footer[] = { 0xF7 };
	MidiWriteBlocking(header, 2);
	MidiWriteBlocking(data, size);
	MidiWriteBlocking(footer, 1);
}

// --- Init / task ------------------------------------------------------------
void WebUI::Init(Spectrum *spec, Mapper *mapper)
{
	spec_ = spec;
	mapper_ = mapper;
	// staging_ is a static array now — no malloc (a 140KB malloc exceeded free
	// RAM and wedged core 1).
	msgLen_ = 0;
	inSysex_ = false;
	snapReady_ = false;
	tusb_init();
}

void WebUI::Task()
{
	tud_task();
	uint8_t rx[kRxBuf];
	while (tud_midi_available())
	{
		uint32_t n = tud_midi_stream_read(rx, sizeof(rx));
		if (n > 0) ParseStream(rx, n);
	}
}

// SysEx framing FSM (same shape as the repo's web_interface example).
void WebUI::ParseStream(const uint8_t *buf, uint32_t n)
{
	for (uint32_t i = 0; i < n; i++)
	{
		uint8_t b = buf[i];
		if (!inSysex_)
		{
			if (b == 0xF0) { inSysex_ = true; msgLen_ = 0; }
		}
		else
		{
			if (b == 0xF7)
			{
				// msg_[0] = manufacturer id, msg_[1..] = payload
				if (msgLen_ >= 1 && msg_[0] == ZX_MANUFACTURER_ID)
					OnSysEx(msg_ + 1, msgLen_ - 1);
				inSysex_ = false;
				msgLen_ = 0;
			}
			else if (msgLen_ < kMsgBuf)
			{
				msg_[msgLen_++] = b;
			}
		}
	}
}

// --- Protocol handlers ------------------------------------------------------
void WebUI::OnSysEx(const uint8_t *data, uint32_t size)
{
	if (size < 1) return;
	uint8_t id = data[0];

	switch (id)
	{
	case MSG_HELLO:
	{
		uint8_t info[] = { MSG_INFO, 0, 3, 0 }; // version 0.3.0
		SendSysEx(info, sizeof(info));
		break;
	}

	case MSG_SNAP_BEGIN:
	{
		// payload: MSG_SNAP_BEGIN + 4 septets = expected length (28-bit)
		snapLen_ = 0;
		snapExpected_ = 0;
		if (size >= 5)
			snapExpected_ = uint32_t(data[1]) | (uint32_t(data[2]) << 7)
			              | (uint32_t(data[3]) << 14) | (uint32_t(data[4]) << 21);
		uint8_t ack[] = { MSG_SNAP_ACK, 0 };
		SendSysEx(ack, sizeof(ack));
		break;
	}

	case MSG_SNAP_CHUNK:
	{
		// payload after id is 7-bit-encoded snapshot bytes.
		uint8_t decoded[kMsgBuf];
		uint32_t m = Decode7bit(data + 1, size - 1, decoded, sizeof(decoded));
		if (snapLen_ + m <= kStagingMax)
		{
			memcpy(staging_ + snapLen_, decoded, m);
			snapLen_ += m;
		}
		uint8_t ack[] = { MSG_SNAP_ACK, uint8_t((snapLen_ >> 7) & 0x7F) };
		SendSysEx(ack, sizeof(ack));
		break;
	}

	case MSG_SNAP_END:
	{
		snapReady_ = true;   // emulation loop will pick this up and load it
		uint8_t st[] = { MSG_STATUS, 1 }; // 1 = loaded
		SendSysEx(st, sizeof(st));
		break;
	}

	case MSG_MAP_GET:
	{
		// Report the mapping table: for each of SRC_COUNT sources, 3 bytes:
		// kind, arg, threshold-high-nibble packed (kept simple/7-bit safe).
		uint8_t rep[2 + SRC_COUNT * 3];
		uint32_t p = 0;
		rep[p++] = MSG_MAP_REPORT;
		rep[p++] = SRC_COUNT;
		for (int s = 0; s < SRC_COUNT; s++)
		{
			const Mapping &mp = mapper_->GetMapping((Source)s);
			rep[p++] = mp.kind & 0x7F;
			rep[p++] = mp.arg & 0x7F;
			rep[p++] = uint8_t((mp.threshold >> 4) & 0x7F); // coarse threshold
		}
		SendSysEx(rep, p);
		break;
	}

	case MSG_MAP_SET:
	{
		// payload: id, count, then count*(kind,arg,threshold7) triples.
		if (size < 2) break;
		uint8_t count = data[1];
		uint32_t p = 2;
		for (int s = 0; s < count && s < SRC_COUNT && p + 2 < size; s++)
		{
			Mapping mp;
			mp.kind      = (TargetKind)data[p++];
			mp.arg       = data[p++];
			mp.threshold = int16_t(data[p++]) << 4;
			mp.invert    = false;
			mapper_->SetMapping((Source)s, mp);
		}
		uint8_t st[] = { MSG_STATUS, 2 }; // 2 = mapping updated
		SendSysEx(st, sizeof(st));
		break;
	}

	case MSG_KEY:
		// payload: id, keyIndex, down(0/1) — keyboard passthrough.
		if (size >= 3) mapper_->PassthroughKey(data[1], data[2] != 0);
		break;

	case MSG_KEY_RESET:
		mapper_->PassthroughReset();
		break;

	default: break;
	}
}

} // namespace zx
