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
	 * Build @p name's sprite tree. Each placement starts as its kind's
	 * constructor starts it (mode, hidden flag, hit test). Returns false,
	 * after a warning, if the room's data is malformed.
	 */
	bool load(const Common::String &name);

	/** Run one game tick. Returns true if any sprite (or the cursor) changed step. */
	bool tick();

	/** Paint the current steps, then the cursor, into @p out, (re)created at the room's size. */
	void draw(Graphics::Surface &out);

	const Common::String &getName() const { return _name; }
	/** Front-to-back draw order, as sprite labels. */
	const Common::String &getOrder() const { return _order; }

	/**
	 * The original's hit walk (0x4311F0): the first sprite, front to back,
	 * whose test passes at (x, y). Returns a sprite index, or -1.
	 */
	int hitTest(int x, int y);
	const Common::String &getLabel(int sprite) const { return _sprites[sprite].label; }
	/** The sprite's rollover cursor ANIM name, or empty for the default. */
	const Common::String &getRollover(int sprite) const { return _sprites[sprite].rollover; }
	/** An exit's destinationroom=, or empty. */
	const Common::String &getDestination(int sprite) const { return _sprites[sprite].destination; }

	/** Show the OBJ_CURSOR sprite playing @p anim at (x, y); an empty name hides it. */
	bool setCursor(const Common::String &anim);
	void setCursorPosition(int x, int y);

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

	// Placement kinds, by the constructor the default placement path picks.
	enum Kind {
		kKindLayer,
		kKindSimple,
		kKindCharacter,
		kKindGhost,
		kKindExit,
		kKindRequires,
		kKindInventory,
		kKindClickable
	};

	struct Sprite {
		int depth;
		BlockPtr block;               // block holding the frame records; null for the root
		uint32 frame;                 // fixed frame record, for layer sprites
		bool animated;
		Player player;
		int x, y;
		bool hidden;                  // +0x14: neither drawn nor hit
		int hitType;                  // +4: 1 = cel pixels and masks, 2 = step polygon
		Common::String label;
		Common::String rollover;
		Common::String destination;
		Common::Array<uint> children; // indexes into _sprites, by descending depth
	};

	uint addSprite(uint parent, const Sprite &sprite);
	void frontToBack(uint index, Common::Array<uint> &out) const;
	uint32 getFrame(const Sprite &sprite) const;
	uint32 getPolygon(const Sprite &sprite) const;
	void paint(const Sprite &sprite, Graphics::Surface &out);
	bool hitCels(const Sprite &sprite, int x, int y);
	bool hitPolygon(const Sprite &sprite, int x, int y) const;
	bool tickPlayer(Sprite &sprite);
	const Cel *getCel(Block &block, uint32 offset);
	BlockPtr getObjectBlock(const Common::String &name);
	bool startSequence(Sprite &sprite, uint32 sequence, int mode, int ticksPerStep, int startStep);
	Kind getPlacementKind(const Common::String &obj, int y, int &slot, int &depth) const;
	int getIniInt(const Common::String &section, const char *key) const;
	Common::String getIniString(const Common::String &section, const char *key) const;

	Sprite _cursor;
	bool _cursorShown;
	Common::String _cursorAnim;

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
