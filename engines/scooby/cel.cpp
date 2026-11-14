/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/debug.h"
#include "common/endian.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/textconsole.h"
#include "graphics/surface.h"

#include "scooby/art_archive.h"
#include "scooby/cel.h"
#include "scooby/detection.h"

namespace Scooby {

// Cel header (24 bytes):
//   u32 size, u16 width, u16 height, u32 mask A offset, u32 mask B offset,
//   u8 solid, u8 rle, u16 solid colour, u32 runtime cache slot
static const uint32 kCelHeaderSize = 24;
static const byte kRunMarker = 0xF0;
// The tallest cels are 400 x 5800 (Showdown) and 400 x 5400 (Phantom). The
// cap only guards the allocation against a corrupt header.
static const uint32 kMaxCelPixels = 1 << 24;

// Room block: ten u32 layer slots at 0x80, each the offset of a frame record
// or 0. A frame record is u32 size, u16 n, u16 0, then n x (i16 x, i16 y,
// u32 cel offset).
static const uint32 kRoomLayerSlots = 0x80;
static const uint kRoomLayerCount = 10;
static const uint32 kRoomCelCount = 0xA8;
static const uint32 kRoomCelTable = 0xAC;
static const uint32 kFrameHeaderSize = 8;
static const uint32 kFrameEntrySize = 8;

// Object resource: u32 size, then four (u32 count, u32 offset) table pairs.
// Table 3 lists the object's cels.
static const uint32 kObjectCelCount = 4 + 8 * 3;
static const uint32 kObjectCelTable = kObjectCelCount + 4;

Cel::Cel() : _width(0), _height(0), _solid(false), _solidColor(0) {
}

bool Cel::load(const byte *block, uint32 blockSize, uint32 offset) {
	_width = _height = 0;
	_pixels.clear();
	_coverage.clear();

	if (offset > blockSize || blockSize - offset < kCelHeaderSize) {
		warning("Cel at 0x%x: header is outside its block", offset);
		return false;
	}

	const byte *header = block + offset;
	const uint16 width = READ_LE_UINT16(header + 4);
	const uint16 height = READ_LE_UINT16(header + 6);
	const uint32 maskA = READ_LE_UINT32(header + 8);
	const uint32 maskB = READ_LE_UINT32(header + 12);
	const bool rle = header[17] != 0;
	_solid = header[16] != 0;
	_solidColor = READ_LE_UINT16(header + 18);

	if (width == 0 || height == 0 || (uint32)width * height > kMaxCelPixels) {
		warning("Cel at 0x%x: bad size %u x %u", offset, width, height);
		return false;
	}

	const uint32 count = (uint32)width * height;
	_pixels.resize(count);
	_coverage.resize(count);
	for (uint32 i = 0; i < count; i++)
		_coverage[i] = kTransparent;

	const byte *src = header + kCelHeaderSize;
	const byte *end = block + blockSize;

	if (!rle) {
		// Raw cels are little-endian words, used in place by the original.
		if ((uint32)(end - src) / 2 < count) {
			warning("Cel at 0x%x: raw pixels run past its block", offset);
			return false;
		}
		for (uint32 i = 0; i < count; i++)
			_pixels[i] = READ_LE_UINT16(src + 2 * i);
	} else {
		// F0 count hi lo is a run; any other pair of bytes is one literal.
		// Words are big-endian, and runs cross row boundaries.
		uint32 n = 0;
		while (n < count) {
			if (end - src < 2) {
				warning("Cel at 0x%x: pixel stream runs past its block", offset);
				return false;
			}
			if (src[0] == kRunMarker) {
				if (end - src < 4 || src[1] > count - n) {
					warning("Cel at 0x%x: bad run at pixel %u", offset, n);
					return false;
				}
				const uint16 color = READ_BE_UINT16(src + 2);
				for (uint k = src[1]; k > 0; k--)
					_pixels[n++] = color;
				src += 4;
			} else {
				_pixels[n++] = READ_BE_UINT16(src);
				src += 2;
			}
		}
	}

	_width = width;
	_height = height;

	if ((maskA && !applyMask(block, blockSize, maskA, kOpaque)) ||
	    (maskB && !applyMask(block, blockSize, maskB, kTranslucent))) {
		warning("Cel at 0x%x: bad mask", offset);
		_width = _height = 0;
		_pixels.clear();
		_coverage.clear();
		return false;
	}

	return true;
}

bool Cel::applyMask(const byte *block, uint32 blockSize, uint32 offset, Coverage coverage) {
	// u32 size, u16 x, u16 y, u16 w, u16 h, then records up to the chunk end:
	// u16 repeat_rows, u16 n, u16 runs[n], with runs alternating skip and
	// covered. Each repeated row starts again at column x.
	if (offset > blockSize || blockSize - offset < 12)
		return false;

	const byte *chunk = block + offset;
	const uint32 size = READ_LE_UINT32(chunk);
	if (size < 12 || size > blockSize - offset)
		return false;

	const uint x = READ_LE_UINT16(chunk + 4);
	uint row = READ_LE_UINT16(chunk + 6);
	const byte *q = chunk + 12;
	const byte *end = chunk + size;

	while (q < end) {
		if (end - q < 4)
			return false;
		const uint repeat = READ_LE_UINT16(q);
		const uint n = READ_LE_UINT16(q + 2);
		if ((uint32)(end - q - 4) / 2 < n)
			return false;
		const byte *runs = q + 4;
		q += 4 + 2 * n;

		for (uint r = 0; r < repeat; r++, row++) {
			if (row >= _height)
				return false;
			uint col = x;
			for (uint k = 0; k + 1 < n; k += 2) {
				col += READ_LE_UINT16(runs + 2 * k);
				const uint covered = READ_LE_UINT16(runs + 2 * k + 2);
				if (covered) {
					if (col + covered > _width)
						return false;
					memset(&_coverage[row * _width + col], coverage, covered);
				}
				col += covered;
			}
		}
	}
	return true;
}

uint16 Cel::getColor(uint x, uint y) const {
	const uint i = y * _width + x;
	if (_coverage[i] == kTransparent)
		return 0;
	if (_coverage[i] == kOpaque && _solid)
		return _solidColor;
	return _pixels[i];
}

void Cel::drawOpaque(Graphics::Surface &dst, int x, int y) const {
	assert(dst.format.bytesPerPixel == 2);
	for (uint cy = 0; cy < _height; cy++) {
		const int sy = y + (int)cy;
		if (sy < 0 || sy >= dst.h)
			continue;
		uint16 *out = (uint16 *)dst.getBasePtr(0, sy);
		for (uint cx = 0; cx < _width; cx++) {
			const int sx = x + (int)cx;
			if (sx < 0 || sx >= dst.w || getCoverage(cx, cy) != kOpaque)
				continue;
			out[sx] = getColor(cx, cy);
		}
	}
}

Common::String Cel::getHash() const {
	Common::Array<byte> buf;
	buf.resize(_pixels.size() * 3);
	for (uint y = 0, i = 0; y < _height; y++) {
		for (uint x = 0; x < _width; x++, i += 3) {
			const uint16 color = getColor(x, y);
			buf[i] = getCoverage(x, y);
			buf[i + 1] = color & 0xFF;
			buf[i + 2] = color >> 8;
		}
	}
	Common::MemoryReadStream stream(buf.data(), buf.size());
	return Common::computeStreamMD5AsString(stream);
}

bool drawRoomLayers(const Common::Array<byte> &block, Graphics::Surface &dst) {
	const uint32 blockSize = block.size();
	if (blockSize < kRoomLayerSlots + 4 * kRoomLayerCount)
		return false;

	for (uint slot = 0; slot < kRoomLayerCount; slot++) {
		const uint32 frame = READ_LE_UINT32(&block[kRoomLayerSlots + 4 * slot]);
		if (!frame)
			continue;
		if (frame > blockSize || blockSize - frame < kFrameHeaderSize)
			return false;

		const uint n = READ_LE_UINT16(&block[frame + 4]);
		if ((blockSize - frame - kFrameHeaderSize) / kFrameEntrySize < n)
			return false;

		for (uint k = 0; k < n; k++) {
			const byte *entry = &block[frame + kFrameHeaderSize + kFrameEntrySize * k];
			const int dx = (int16)READ_LE_UINT16(entry);
			const int dy = (int16)READ_LE_UINT16(entry + 2);
			Cel cel;
			if (!cel.load(block.data(), blockSize, READ_LE_UINT32(entry + 4)))
				return false;
			debugC(2, kDebugGraphics, "layer %u: %u x %u cel at (%d, %d)", slot, cel.getWidth(), cel.getHeight(), dx, dy);
			cel.drawOpaque(dst, dx, dy);
		}
	}
	return true;
}

static void checkCelTable(const Common::String &name, const Common::Array<byte> &block,
                          uint32 countOffset, uint32 tableOffset, uint &cels, uint &bad) {
	const uint32 blockSize = block.size();
	if (blockSize < tableOffset + 4) {
		debugC(3, kDebugGraphics, "cel %s: block too small BAD", name.c_str());
		bad++;
		return;
	}

	const uint32 count = READ_LE_UINT32(&block[countOffset]);
	const uint32 table = READ_LE_UINT32(&block[tableOffset]);
	if (table > blockSize || (blockSize - table) / 4 < count) {
		debugC(3, kDebugGraphics, "cel %s: cel table outside the block BAD", name.c_str());
		bad++;
		return;
	}

	for (uint32 i = 0; i < count; i++) {
		Cel cel;
		cels++;
		if (!cel.load(block.data(), blockSize, READ_LE_UINT32(&block[table + 4 * i]))) {
			debugC(3, kDebugGraphics, "cel %s %u BAD", name.c_str(), i);
			bad++;
			continue;
		}
		debugC(3, kDebugGraphics, "cel %s %u %ux%u %s", name.c_str(), i, cel.getWidth(), cel.getHeight(),
		       cel.getHash().c_str());
	}
}

void checkAllCels(const ArtArchive &art) {
	uint cels = 0, bad = 0;
	Common::Array<byte> block;

	for (uint i = 0; i < art.getObjectCount(); i++) {
		const Common::String name = art.getObjectName(i);
		if (art.readBlock(name, block))
			checkCelTable(name, block, kObjectCelCount, kObjectCelTable, cels, bad);
		else
			bad++;
	}
	for (uint i = 0; i < art.getRoomCount(); i++) {
		const Common::String name = art.getRoomName(i);
		if (art.readBlock(name, block))
			checkCelTable(name, block, kRoomCelCount, kRoomCelTable, cels, bad);
		else
			bad++;
	}

	debugC(3, kDebugGraphics, "cel check: %u cels, %u bad", cels, bad);
}

} // End of namespace Scooby
