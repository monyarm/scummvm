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

#ifndef TERRAGLYPH_STORAGE_ROOMS_H
#define TERRAGLYPH_STORAGE_ROOMS_H

#include "terraglyph/rooms/room_class.h"

namespace Terraglyph {

/**
 * Phantom's kitchen storage room, ROOM_P17_Kitchen_Storage (0x4139A0,
 * vtable 0x45E994). Fred takes the screwdriver and Daphne the shield when
 * clicked. The door won't open with the arrow; with the screwdriver it is
 * worked open over three clicks, each through a movie back to this room
 * (DoorClicked 1 to 3).
 */
class KitchenStorageRoom : public RoomClass {
public:
	explicit KitchenStorageRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x413AF0
	void enter(const Common::String &from) override;                               // 0x413A00
	void roomLeft() override;                                                      // 0x414DB0
	void tick() override;                                                          // 0x413B20
	bool isBusy() const override { return _screwdriver || _shield; }               // game +0x24
	void event(const Common::String &object, bool success) override;              // 0x413EA0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x414B30
	void saveKeys(Common::INIFile &profile) override;                              // 0x414E40
	void loadKeys(const Common::INIFile &profile) override;                        // 0x414E80

	int getDoorClicked() const { return _doorClicked; }
	void setDoorClicked(int n) { _doorClicked = n; }
	bool isTakingScrewdriver() const { return _screwdriver; }
	bool isTakingShield() const { return _shield; }
	int getDoor() const { return _door; }

private:
	int findSprite(const Common::String &object) const;
	void exitHere(int movie, bool fredTurns);

	int _door = -1;             // +0xB8: OBJ_P17_KITCHEN_STORAGE_DOOR
	int _doorClicked = 0;       // +0xBC, saved as DoorClicked
	int _doorLine = 0;          // +0xC0: the arrow on the door, 0-2
	int _hotspot2Line = 0;      // +0xC4
	int _hotspot1Line = 0;      // +0xC8
	bool _screwdriver = false;  // +0xCC: Fred to take the screwdriver
	bool _fredTaking = false;   // +0xCD
	bool _shield = false;       // +0xCE: Daphne to take the shield
	bool _daphneTaking = false; // +0xCF
	int _hotspot3 = -1;         // +0xD0
	int _screwdriverSprite = -1; // +0xD4
	int _shieldSprite = -1;     // +0xD8
};

/** The class for this room, or nullptr. */
RoomClass *createStorageRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_STORAGE_ROOMS_H
