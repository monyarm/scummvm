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

#ifndef SCOOBY_CEL_H
#define SCOOBY_CEL_H

#include "common/array.h"
#include "common/str.h"

namespace Graphics {
struct Surface;
}

namespace Scooby {

class ArtArchive;

/**
 * One Gen 1 image (a "cel"), decoded from a room or object block.
 *
 * Pixels are 16-bit words. Where mask A covers a pixel it is opaque RGB565;
 * where mask B covers it, it is translucent ARGB4444; anywhere else it is
 * transparent. A solid cel paints mask A with one colour instead of its
 * pixels.
 */
class Cel {
public:
	enum Coverage {
		kTransparent = 0,
		kOpaque = 1,     // mask A
		kTranslucent = 2 // mask B
	};

	Cel();

	/**
	 * Decode the cel at @p offset in a block. Mask offsets are relative to
	 * the same block. Returns false, after a warning, if any part of the
	 * cel falls outside the block or the image.
	 */
	bool load(const byte *block, uint32 blockSize, uint32 offset);

	uint16 getWidth() const { return _width; }
	uint16 getHeight() const { return _height; }
	Coverage getCoverage(uint x, uint y) const { return (Coverage)_coverage[y * _width + x]; }

	/** The pixel's word: the solid colour for opaque pixels of a solid cel, 0 where transparent. */
	uint16 getColor(uint x, uint y) const;

	/** Copy the opaque pixels to an RGB565 surface, with (x, y) the cel's top-left corner. */
	void drawOpaque(Graphics::Surface &dst, int x, int y) const;

	/** MD5 of (coverage, colour) for every pixel, for checking against tools/probe_art.py. */
	Common::String getHash() const;

private:
	bool applyMask(const byte *block, uint32 blockSize, uint32 offset, Coverage coverage);

	uint16 _width;
	uint16 _height;
	bool _solid;
	uint16 _solidColor;
	Common::Array<uint16> _pixels;
	Common::Array<byte> _coverage;
};

/**
 * Draw the opaque pass of a room block's layers: each used layer slot's
 * frame record, slot 0 (furthest back) first. Returns false if the block's
 * layer data is malformed.
 */
bool drawRoomLayers(const Common::Array<byte> &block, Graphics::Surface &dst);

/** Decode every cel in the art file and log its hash on the graphics channel. */
void checkAllCels(const ArtArchive &art);

} // End of namespace Scooby

#endif // SCOOBY_CEL_H
