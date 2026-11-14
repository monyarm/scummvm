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
	uint getSpriteCount() const { return _sprites.size(); }
	const Common::String &getLabel(int sprite) const { return _sprites[sprite].label; }
	/** The sprite's rollover cursor ANIM name, or empty for the default. */
	const Common::String &getRollover(int sprite) const { return _sprites[sprite].rollover; }
	/** An exit's destinationroom=, or empty. */
	const Common::String &getDestination(int sprite) const { return _sprites[sprite].destination; }

	/** The OBJ name a sprite shows now; empty for layers. */
	const Common::String &getObjectName(int sprite) const { return _sprites[sprite].object; }
	/** Character placements' sprites, in placement order. */
	const Common::Array<uint> &getCharacters() const { return _characters; }

	/**
	 * Switch a sprite to @p obj (if it differs and exists) and start @p anim
	 * on it (the state change at 0x405190, then vtable +0x24 and +0x10).
	 * If @p anim is missing, the sprite stops on step 0 and this returns
	 * false, as the original does after "Setting %s to %s failed".
	 */
	bool setSequence(uint sprite, const Common::String &obj, const Common::String &anim, int mode, int startStep);
	/**
	 * The step count of @p anim on @p obj, or -1 if either is missing (the
	 * walk length, 0x404EA0).
	 */
	int getSequenceSteps(const Common::String &obj, const Common::String &anim);
	int getDepth(uint sprite) const { return _sprites[sprite].depth; }
	/** Move a sprite to a new depth among its siblings (0x430EE0). */
	void setDepth(uint sprite, int depth);
	uint32 getStep(uint sprite) const { return _sprites[sprite].player.step; }
	uint32 getStepCount(uint sprite) const { return _sprites[sprite].player.steps; }
	/** A sequence in mode 0: stopped, or played to its end. */
	bool isStopped(uint sprite) const { return !_sprites[sprite].animated || _sprites[sprite].player.mode == 0; }
	/** Show @p step of the sprite's sequence, clamped to its last step. */
	void setStep(uint sprite, uint32 step);

	/** A placement the default path built as a clickableobject (0x408DC0). */
	bool isClickable(uint sprite) const { return _sprites[sprite].kind == kKindClickable; }
	/** The room's OBJ_GHOST sprite (game +0x1DC), or -1. */
	int findGhost() const;
	/** The ANIM name the sprite was last started on. */
	const Common::String &getAnim(uint sprite) const { return _sprites[sprite].anim; }
	/** Whether the art file names @p anim as an ANIM (GetId != 0). */
	bool hasAnim(const Common::String &anim) const;
	/**
	 * Start @p anim on the sprite's object (vtable +0x24, 0x430A00) at 3
	 * ticks per step. An unknown name has id 0: no sequence starts, and the
	 * sprite selects no frame, so it is neither drawn nor hit. Returns false
	 * then.
	 */
	bool startAnim(uint sprite, const Common::String &anim, int mode, int startStep);
	/** Whether the sprite selects a frame (0x4309C0); false after an unknown ANIM. */
	bool hasFrame(uint sprite) const { return _sprites[sprite].block && getFrame(_sprites[sprite]) != 0; }
	/** Show or hide a sprite (+0x14, 0x4313F0). */
	void setHidden(uint sprite, bool hidden) { _sprites[sprite].hidden = hidden; }

	/** A placement the default path built as an inventoryobject (0x4026B0). */
	bool isInventoryObject(uint sprite) const { return _sprites[sprite].kind == kKindInventory; }
	/** A placement the default path built as a requiresobject (0x402D90). */
	bool isRequiresObject(uint sprite) const { return _sprites[sprite].kind == kKindRequires; }
	bool isHidden(uint sprite) const { return _sprites[sprite].hidden; }
	/** The sequence player's mode (+0x40): 0 when stopped or played out. */
	int getMode(uint sprite) const { return _sprites[sprite].animated ? _sprites[sprite].player.mode : 0; }
	int getX(uint sprite) const { return _sprites[sprite].x; }
	int getY(uint sprite) const { return _sprites[sprite].y; }

	/**
	 * Add a sprite of the engine's own (the toolbar) under @p parent (-1 for
	 * the root), at (x, y) from its parent, playing @p anim from step 0 in
	 * mode 1 at 3 ticks per step (0x430790 as 0x407E50 calls it). @p hitType
	 * is 1 (cels) or 2 (step polygon). Returns its index.
	 */
	uint addEngineSprite(int parent, const Common::String &obj, const Common::String &anim, int depth,
	                     int x, int y, int hitType, const Common::String &label);
	/** Move a sprite to (x, y) from its parent; its children follow. */
	void setOffset(uint sprite, int x, int y);
	/** 0x430C00: the widest and the tallest cel over the current sequence's steps. */
	void getSize(uint sprite, int &width, int &height);
	/** 0x430AA0 on the current sequence: set the mode and the step. */
	void setPlayer(uint sprite, int mode, int startStep);

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
		kKindClickable,
		kKindEngine     // added by the engine: the toolbar
	};

	struct Sprite {
		int depth;
		BlockPtr block;               // block holding the frame records; null for the root
		uint32 frame;                 // fixed frame record, for layer sprites
		bool animated;
		Player player;
		int x, y;                     // on the room's surface
		int offsetX = 0, offsetY = 0; // from the parent (+0xC, +0x10)
		bool hidden;                  // +0x14: neither drawn nor hit
		int hitType;                  // +4: 1 = cel pixels and masks, 2 = step polygon
		uint parent;                  // index into _sprites; the root is its own parent
		Kind kind = kKindLayer;       // the constructor the default placement path picked
		Common::String anim;          // the ANIM last started
		Common::String object;        // the OBJ name the block belongs to
		Common::String label;
		Common::String rollover;
		Common::String destination;
		Common::Array<uint> children; // indexes into _sprites, by descending depth
	};

	uint addSprite(uint parent, const Sprite &sprite);
	void frontToBack(uint index, Common::Array<uint> &out) const;
	void insertChild(uint parent, uint index);
	void placeSubtree(uint index);
	void buildDrawList();
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
	Common::Array<uint> _characters;
	Common::String _order;
	Common::HashMap<Common::String, BlockPtr, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _objects;
};

} // End of namespace Scooby

#endif // SCOOBY_ROOM_H
