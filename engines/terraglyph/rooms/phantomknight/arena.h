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

#ifndef TERRAGLYPH_ARENA_H
#define TERRAGLYPH_ARENA_H

#include "terraglyph/rooms/class.h"

namespace Terraglyph {

/**
 * Phantom's first arena, ROOM_P09_Arena1 (0x40D4E0, vtable 0x45E68C): the
 * rusty shears on the grindstone, through movie 40, come back sharp.
 */
class Arena1Room : public RoomClass {
public:
	explicit Arena1Room(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                             // 0x40D540
	void enter(const Common::String &from) override;                   // 0x40D700
	void roomLeft() override;                                          // 0x40D6A0
	void event(const Common::String &object, bool success) override;  // 0x40D560

	bool isSharpening() const { return _sharpening; }

private:
	int _stoneLine = 0;       // +0xB8
	bool _sharpening = false; // +0xBC
};

/**
 * Phantom's second arena, ROOM_P14_Arena2 (0x40D790, vtable 0x45E6CC),
 * Brad's. Until his game is won (BradPuzzle) he hides in the armour pile,
 * rises during the opening, and the pile leads to his game; won, he gives
 * his clue. The shield given him on Big Head, he steps aside and Velma
 * takes the mirror.
 */
class Arena2Room : public RoomClass {
public:
	explicit Arena2Room(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x40D7F0
	void enter(const Common::String &from) override;                               // 0x40D830
	void roomLeft() override;                                                      // 0x40E030
	void tick() override;                                                          // 0x40DA80
	void idle() override;                                                          // 0x40EBA0
	bool click(int sprite) override;
	bool isBusy() const override { return _thanks || _mirrorPending; }             // game +0x24
	void event(const Common::String &object, bool success) override;              // 0x40E5A0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x40E090
	Common::String idleLine(RoomScript &script, const Common::String &name) override; // 0x40EAC0
	bool holdScene() override;                                                     // 0x40E440

	int getPile() const { return _pile; }
	int getMirror() const { return _mirror; }
	bool isGiving() const { return _giving; }
	bool isShieldGiven() const { return _shieldGiven; }
	bool isBradMoved() const { return _bradMoved; }
	bool isMirrorPending() const { return _mirrorPending; }
	bool isIgnoringClick() const { return _ignoreClick; }
	bool isOpeningPending() const { return _openingPending; }

private:
	int findSprite(const Common::String &object) const;
	void rise();

	int _hotspot2 = -1;         // +0xB8
	int _pile = -1;             // +0xBC: OBJ_BRAD_ARMOR_PILE
	int _mirror = -1;           // +0xC0: OBJ_INV_MIRROR
	bool _giving = false;       // +0xC4: his clue given, he gives
	bool _thanks = false;       // +0xC5
	bool _shieldGiven = false;  // +0xC6: the shield given him on Big Head
	bool _bradMoved = false;    // +0xC7: he stood aside on entry
	bool _velmaTaking = false;  // +0xC8
	bool _mirrorPending = false; // +0xC9: Velma to take the mirror
	bool _ignoreClick = false;  // +0xCA: the pile's next click is the room's own
	bool _openingPending = false; // +0xCB: the opening unplayed on entry
	int _clicked = -1;
};

/** The class for this room, or nullptr. */
RoomClass *createArenaRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_ARENA_H
