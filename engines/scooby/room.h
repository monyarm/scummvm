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
class Cel;

/**
 * A Gen 1 room as the original sets it up on entry: its layer sprites, then
 * one animated sprite per placement, in the traced sprite tree
 * (docs/formats/tgifile-art.md, "Rendering a room" and "Sequences").
 */
class Room {
public:
	/** Game ticks per second (Scooby.exe 0x435BC0: elapsed ms * 36 / 1000). */
	static const uint kTicksPerSecond = 36;

	Room(const ArtArchive &art, const Common::INIFile &objectIni);
	~Room();

	/**
	 * Build @p name's sprite tree and start every placement in mode 1 at
	 * 3 ticks per step from step 0. Returns false, after a warning, if the
	 * room's data is malformed.
	 */
	bool load(const Common::String &name);

	/** Run one game tick. Returns true if any sprite changed step. */
	bool tick();

	/** Paint the current steps into @p out, (re)created at the room's size. */
	void draw(Graphics::Surface &out);

	const Common::String &getName() const { return _name; }
	/** Front-to-back draw order, as sprite labels. */
	const Common::String &getOrder() const { return _order; }

private:
	struct Block {
		Common::Array<byte> data;
		Common::HashMap<uint32, Common::SharedPtr<Cel> > cels; // decoded once per room
	};
	typedef Common::SharedPtr<Block> BlockPtr;

	// Sequence player state (0x430890 / 0x430AA0).
	struct Player {
		int mode;       // 0 stopped, 1 loop forward, 2 loop back, 3 once forward, 4 once back
		int ticksPerStep;
		int countdown;
		uint32 step;
		uint32 steps;   // step count of the sequence
		uint32 record;  // sequence record offset in the object block
	};

	struct Sprite {
		int depth;
		BlockPtr block;               // block holding the frame records; null for the root
		uint32 frame;                 // fixed frame record, for layer sprites
		bool animated;
		Player player;
		int x, y;
		Common::String label;
		Common::Array<uint> children; // indexes into _sprites, by descending depth
	};

	uint addSprite(uint parent, const Sprite &sprite);
	void frontToBack(uint index, Common::Array<uint> &out) const;
	uint32 getFrame(const Sprite &sprite) const;
	void paint(const Sprite &sprite, Graphics::Surface &out);
	const Cel *getCel(Block &block, uint32 offset);
	BlockPtr getObjectBlock(const Common::String &name);
	bool startSequence(Sprite &sprite, uint32 sequence, int mode, int ticksPerStep, int startStep);
	void getPlacementParent(const Common::String &obj, int y, int &slot, int &depth) const;
	int getIniInt(const Common::String &section, const char *key) const;

	const ArtArchive &_art;
	const Common::INIFile &_ini;
	Common::String _name;
	uint16 _width, _height;
	Common::Array<Sprite> _sprites;
	Common::Array<uint> _drawList; // front to back
	Common::String _order;
	Common::HashMap<Common::String, BlockPtr, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _objects;
};

} // End of namespace Scooby

#endif // SCOOBY_ROOM_H
