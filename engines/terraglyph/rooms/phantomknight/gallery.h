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

#ifndef TERRAGLYPH_GALLERY_H
#define TERRAGLYPH_GALLERY_H

#include "terraglyph/rooms/phantomknight/doors.h" // JesterControl
#include "terraglyph/rooms/class.h"

namespace Terraglyph {

class Character;

/**
 * Phantom's royal gallery, ROOM_P07_Royal_Gallery (0x416240, vtable
 * 0x45EADC): its mirror is bare until the found mirror is hung on it, when
 * Shaggy walks over and puts it up; the way to the magic mirror room opens
 * with it, and only once the level's snacks plus two are given. The red
 * ball, shown to the Jester, is found; the Jester himself stands here as he
 * does in the room of doors (the shared JesterControl). The gallery exits
 * to the magic mirror room and the dining room.
 */
class RoyalGalleryRoom : public RoomClass {
public:
	explicit RoyalGalleryRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x4162D0
	void enter(const Common::String &from) override;                               // 0x416830
	void tick() override;                                                          // 0x416610
	bool click(int sprite) override;
	void event(const Common::String &object, bool success) override;               // 0x416500
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x4168D0
	bool leaveBy(const Common::String &exitObject) override;                        // 0x4162F0
	bool isBusy() const override { return _mirrorUsed || _jester.isHolding(); }     // game +0x24

	/** The mirror is hung: OBJ_INV_MIRROR ever taken and no longer in a slot. */
	static bool isMirrorPlaced(TerraglyphEngine &engine);

private:
	int findSprite(const Common::String &object) const;

	JesterControl _jester;      // +0xC8: the Jester, as the room of doors keeps him
	int _cycle = 0;             // +0xBC: the next 210A_ line
	bool _mirrorUsed = false;   // +0xC0: the mirror used, Shaggy to hang it
	bool _placing = false;      // +0xC1: Shaggy hanging it
	bool _flashPending = false; // +0xC2: flash the snack box after 210B_
	int _mirror = -1;           // +0xB8: the mirror sprite
	int _exit = -1;             // +0xC4: the OBJ_P07_TO_P08 exit sprite
};

/**
 * Phantom's magic mirror room, ROOM_P08_Magic_Mirror_Room (0x414EC0, vtable
 * 0x45E9DC): the mirror hums (Magic_Mirror_Room.Mirror); three hotspots
 * give their lines, two of them cycling; the cameras and the way to the
 * wizard's skull are built as the room is entered.
 */
class MagicMirrorRoom : public RoomClass {
public:
	explicit MagicMirrorRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x414F20
	void enter(const Common::String &from) override;                               // 0x415250
	void event(const Common::String &object, bool success) override;               // 0x414F40
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x415060

private:
	int _cam = 0;    // +0xB8: HOTSPOT4's 310X2_ / 310XA2_ / 310XA3_
	int _toggle = 0; // +0xBC: HOTSPOT2's 310X1_ / 310X2_
};

/** The class for this room, or nullptr. */
RoomClass *createGalleryRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_GALLERY_H
