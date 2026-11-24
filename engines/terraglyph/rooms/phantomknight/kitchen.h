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

#ifndef TERRAGLYPH_KITCHEN_H
#define TERRAGLYPH_KITCHEN_H

#include "terraglyph/rooms/class.h"

namespace Terraglyph {

/**
 * Phantom's kitchen, ROOM_P13_Kitchen (0x412C90, vtable 0x45E954), where
 * Ewan cooks. A click on him introduces him on Big Head, then leads to his
 * quiz (ROOM_Ewan) until it is won (EwanPuzzle). Won, he gives his clue;
 * after, he stirs until the screwdriver is had, then sleeps until the
 * mirror is used. With the screwdriver, the empty bottle at the grease
 * fills it. The storage room opens once his quiz is won.
 */
class KitchenRoom : public RoomClass {
public:
	explicit KitchenRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x412CF0
	void enter(const Common::String &from) override;                               // 0x412EA0
	void roomLeft() override;                                                      // 0x4138D0
	void tick() override;                                                          // 0x4130D0
	bool click(int sprite) override;
	bool isBusy() const override { return _thanks || _grease; }                    // game +0x24
	void event(const Common::String &object, bool success) override;              // 0x4136C0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x413740
	void exitMovie(const Common::String &destination, int &movie) override;        // 0x412D20
	void saveKeys(Common::INIFile &profile) override;                              // 0x413920
	void loadKeys(const Common::INIFile &profile) override;                        // 0x413960

	bool isIntroDone() const { return _introDone; }
	void setIntroDone(bool done) { _introDone = done; }
	bool isGiving() const { return _giving; }
	bool isSleeping() const { return _sleeping; }
	bool isGreasing() const { return _grease; }
	const Common::String &getLine() const { return _line; }

private:
	int findSprite(const Common::String &object) const;

	int _grease1 = -1;         // +0xB8: OBJ_P13_KITCHEN_HOTSPOT1, the grease
	bool _giving = false;      // +0xBC: his clue given, he gives when idle
	bool _thanks = false;      // +0xBD: given; the thanks when he is idle again
	int _lineIndex = 0;        // +0xC0: his next line or scene, 0-2
	bool _grease = false;      // +0xC4: the bottle at the grease
	bool _fetching = false;    // +0xC5: Fred fetching the grease
	bool _sleeping = false;    // +0xC6
	bool _introDone = false;   // +0xC7, saved as EwanIntro
	Common::String _line;      // +0xC8: his line playing, with the speech lock
	int _clicked = -1;
};

/** The class for this room, or nullptr. */
RoomClass *createKitchenRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_KITCHEN_H
