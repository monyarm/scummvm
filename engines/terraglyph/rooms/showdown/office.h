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

#ifndef TERRAGLYPH_OFFICE_ROOMS_H
#define TERRAGLYPH_OFFICE_ROOMS_H

#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"

namespace Terraglyph {

/**
 * Showdown's Sheriff's office, ROOM_P02_Sheriffs_Office (0x4163A0, vtable
 * 0x45E9D0): the cell door sticks until Shaggy oils it with the oil can,
 * and the way into the jail waits for the level's snacks. Clicked, Shaggy
 * may also have a sandwich.
 */
class SheriffsOfficeRoom : public RoomClass {
public:
	explicit SheriffsOfficeRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void prepare() override;                                                        // 0x416680 (+0x30)
	void enter(const Common::String &from) override;                                // 0x416400
	void tick() override;                                                           // 0x4166C0
	void event(const Common::String &object, bool success) override;                // 0x416AA0
	bool leaveBy(const Common::String &exitObject) override;                        // 0x416470 (+0x24)
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x416C30
	// Game +0x24, set on every tick while Shaggy goes to the door or has his
	// sandwich.
	bool isBusy() const override { return _doorClicked || _sandwich; }

	int getDoor() const { return _door; }
	int getHotspot4() const { return _hotspot4; }
	bool isDoorClicked() const { return _doorClicked; }
	bool isSandwichWaiting() const { return _sandwichWaiting; }
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	void playSound(const char *name);

	int _doorLine = 0;            // +0xB8: the stuck door's next line
	bool _doorClicked = false;    // +0xBC
	bool _atDoor = false;         // +0xBD: Shaggy at the door
	bool _sandwich = false;       // +0xBE
	bool _eating = false;         // +0xBF
	bool _sandwichWaiting = false; // +0xC0: hotspot 4's C clicked, until it stops
	int _door = -1;               // +0xC4
	int _hotspot1 = -1;           // +0xC8
	int _hotspot4 = -1;           // +0xCC
	Common::String _lastScene;
	Common::String _lastSound;
};

/** The Sheriff's office's room class the factory builds for @p room, or nullptr. */
RoomClass *createOfficeRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_OFFICE_ROOMS_H
