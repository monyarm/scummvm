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

#ifndef TERRAGLYPH_CEL_H
#define TERRAGLYPH_CEL_H

#include "common/array.h"
#include "common/str.h"

namespace Graphics {
struct Surface;
}

namespace Terraglyph {

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

	/**
	 * The original's point test for a cel (0x431400): a non-solid cel's
	 * non-zero pixel, or any pixel either mask covers.
	 */
	bool hitTest(uint x, uint y) const;

	/**
	 * Draw onto an RGB565 surface with (x, y) the cel's top-left corner:
	 * opaque pixels are copied, translucent ones blended as the original's
	 * 565 blitter does, transparent ones skipped.
	 */
	void draw(Graphics::Surface &dst, int x, int y) const;

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
 * Blend one ARGB4444 pixel over an RGB565 pixel, as the original's blitter
 * does (Scooby.exe 0x439170).
 */
uint16 blendArgb4444(uint16 dst, uint16 src);

/** Decode every cel in the art file and log its hash on the graphics channel. */
void checkAllCels(const ArtArchive &art);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_CEL_H
