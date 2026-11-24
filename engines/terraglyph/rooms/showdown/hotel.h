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

#ifndef TERRAGLYPH_HOTEL_ROOMS_H
#define TERRAGLYPH_HOTEL_ROOMS_H

#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"

namespace Terraglyph {

/**
 * Showdown's hotel hall, ROOM_P17_Hotel_Hall (0x410BB0, vtable 0x45E810):
 * the locked door Fred opens (HotelHallDoor), the chandelier's lines, a
 * hotspot that makes Daphne scream, and a second opening on the second
 * visit (SecondEntrance).
 */
class HotelHallRoom : public RoomClass {
public:
	explicit HotelHallRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x410C10
	void prepare() override;                                                        // 0x410CC0 (+0x30)
	void enter(const Common::String &from) override;                                // 0x410C40
	void tick() override;                                                           // 0x410D00
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x410DC0
	void event(const Common::String &object, bool success) override;               // 0x410F80
	void saveKeys(Common::INIFile &profile) override;                               // 0x411090
	void loadKeys(const Common::INIFile &profile) override;                         // 0x4110D0

	bool hasSecondEntrance() const { return _secondEntrance; }
	int getDoorSprite() const { return _door; }
	int getHotspot1() const { return _hotspot1; }
	const Common::String &getLastScene() const { return _lastScene; }

private:
	bool _secondEntrance = false; // +0xB8: SecondEntrance
	int _door = -1;               // +0xC0: the door, a requires object
	int _chandLine = 0;           // +0xC4: the chandelier's next line
	int _hotspot1 = -1;           // +0xC8: the hotspot that frightens Daphne
	Common::String _lastScene;
};

/**
 * Showdown's hotel room, ROOM_P20_Hotel_Room (0x411110, vtable 0x45E850):
 * the nickel Daphne picks up, the window (an exit back into the room through
 * movie 15 + GraveLocation, the graveyard from above), which the first visit
 * takes by itself (LookedOutAuto), and two hotspots.
 */
class HotelRoomRoom : public RoomClass {
public:
	explicit HotelRoomRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x411170
	void arrive(const Common::String &from) override;                               // 0x411190 (+0x2C)
	void enter(const Common::String &from) override;                                // 0x411250
	void roomLeft() override;                                                       // 0x411910
	void tick() override;                                                           // 0x4115D0
	bool isBusy() const override { return _nickelClicked; }                         // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x411280
	void event(const Common::String &object, bool success) override;               // 0x4113B0
	bool leaveBy(const Common::String &exitObject) override;                        // 0x411440 (+0x24)
	void saveKeys(Common::INIFile &profile) override;                               // 0x411960
	void loadKeys(const Common::INIFile &profile) override;                         // 0x4119A0

	bool hasLookedOutAuto() const { return _lookedOutAuto; }
	bool isNickelClicked() const { return _nickelClicked; }
	int getNickelSprite() const { return _nickel; }
	int getWindowCount() const { return _windowCount; }
	const Common::String &getLastScene() const { return _lastScene; }
	const RoomScript::Exit &getLastExit() const { return _lastExit; }

private:
	/** 0x411440: the room's exit handler, then the default one. */
	void leave(RoomScript::Exit exit);

	int _hotspot2 = -1;           // +0xB8
	int _hotspot4 = -1;           // +0xBC
	int _nickel = -1;             // +0xC0: the nickel, a requires object
	bool _nickelClicked = false;  // +0xC4
	bool _grabbing = false;       // +0xC5: Daphne is picking it up
	bool _lookedOutAuto = false;  // +0xC6: LookedOutAuto
	bool _autoLook = false;       // +0xC7: the window, once the opening is over
	int _windowCount = 0;         // +0xC8
	Common::String _lastScene;
	RoomScript::Exit _lastExit;
};

/** The hotel's room class the factory builds for @p room, or nullptr. */
RoomClass *createHotelRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_HOTEL_ROOMS_H
