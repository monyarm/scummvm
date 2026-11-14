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

#ifndef SCOOBY_ROOM_H
#define SCOOBY_ROOM_H

#include "common/array.h"
#include "common/formats/ini-file.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/ptr.h"
#include "common/str.h"

namespace Graphics {
struct Surface;
}

namespace Scooby {

class ArtArchive;

/**
 * Builds a Gen 1 room as the original sets it up on entry: its layer
 * sprites, then one sprite per placement at step 0 of its sequence, in the
 * traced sprite tree (docs/formats/tgifile-art.md, "Rendering a room").
 */
class RoomRenderer {
public:
	RoomRenderer(const ArtArchive &art, const Common::INIFile &objectIni);

	/**
	 * Render @p room into @p out, which is (re)created at the room's size in
	 * RGB565. @p order receives the front-to-back draw order. Returns false,
	 * after a warning, if the room's data is malformed.
	 */
	bool render(const Common::String &room, Graphics::Surface &out, Common::String &order);

private:
	typedef Common::SharedPtr<Common::Array<byte> > BlockPtr;

	struct Sprite {
		int depth;
		BlockPtr block;               // block holding the frame record; null for the root
		uint32 frame;                 // frame record offset in the block
		int x, y;
		Common::String label;
		Common::Array<uint> children; // indexes into _sprites, by descending depth
	};

	uint addSprite(uint parent, const Sprite &sprite);
	void frontToBack(uint index, Common::Array<uint> &out) const;
	bool paint(const Sprite &sprite, Graphics::Surface &out) const;
	BlockPtr getObjectBlock(const Common::String &name);
	bool findStepFrame(const Common::Array<byte> &object, uint32 sequence, uint32 &frame) const;
	void getPlacementParent(const Common::String &obj, int y, int &slot, int &depth) const;
	int getIniInt(const Common::String &section, const char *key) const;

	const ArtArchive &_art;
	const Common::INIFile &_ini;
	Common::Array<Sprite> _sprites;
	Common::HashMap<Common::String, BlockPtr, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _objects;
};

} // End of namespace Scooby

#endif // SCOOBY_ROOM_H
