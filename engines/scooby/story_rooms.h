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

#ifndef SCOOBY_STORY_ROOMS_H
#define SCOOBY_STORY_ROOMS_H

#include "scooby/room_class.h"

namespace Scooby {

/**
 * Showdown's Town Center (0x42CFD0, vtable 0x45F300): the game's first
 * room. The gang won't leave until Scooby has had a snack here, and the
 * hanging sign comes down for Scooby to take.
 */
class TownCenterRoom : public RoomClass {
public:
	explicit TownCenterRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x42D030
	void enter(const Common::String &from) override;                               // 0x42D270
	void tick() override;                                                          // 0x42D3F0
	bool isBusy() const override { return _takeSign; }                             // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42D800
	void event(const Common::String &object, bool success) override;              // 0x42D7B0
	bool leaveBy(const Common::String &exitObject) override;                       // 0x42D060
	void idle() override;                                                          // 0x42D340
	void saveKeys(Common::INIFile &profile) override;                              // 0x42D970
	void loadKeys(const Common::INIFile &profile) override;                        // 0x42D9B0

	bool isSignClicked() const { return _signClicked; }

private:
	bool isMoaning() const;
	int findSprite(const Common::String &object) const;

	int _exitScene = 0;     // +0xB8: which of the three exit scenes is next
	int _moanDelay = 0;     // +0xBC
	bool _signClicked = false; // +0xC0, saved as SignClicked
	int _hangingSign = -1;  // +0xC4: OBJ_P01_TC_HOTSPOT4
	int _sign = -1;         // +0xC8: OBJ_INV_SIGN, as a requires object
	int _hotspot2 = -1;     // +0xCC: OBJ_P01_TC_HOTSPOT2
	bool _takeSign = false; // +0xD0: the sign was clicked; input is held
	bool _taking = false;   // +0xD1: Scooby is taking it
	int _takes = -1;        // +0xD4: OBJ_SCOOBY_P01_TAKES_SIGN
};

/**
 * Showdown's Street 2 (0x42C3E0, vtable 0x45F27C): Gummy and his donkey,
 * once Artie is met; the clock; Gummy's window.
 */
class Street2Room : public RoomClass {
public:
	explicit Street2Room(ScoobyEngine &engine) : RoomClass(engine) {}

	void prepare() override;                                                       // 0x42CB40
	void enter(const Common::String &from) override;                               // 0x42C440
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42C7A0
	void event(const Common::String &object, bool success) override;              // 0x42CAE0
	void leaving(const Common::String &destination, Common::String &scene) override; // 0x42C600
};

/**
 * Showdown's Street 3 (0x42CB80, vtable 0x45F2C0): moans, a hotspot that
 * cuts the music for its own, and another with its scene.
 */
class Street3Room : public RoomClass {
public:
	explicit Street3Room(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x42CBE0
	void enter(const Common::String &from) override;                               // 0x42CE50
	void tick() override;                                                          // 0x42CD10
	void event(const Common::String &object, bool success) override;              // 0x42CC10
	void idle() override;                                                          // 0x42CCC0

private:
	int _moanDelay = 0;      // +0xB8
	bool _musicBack = true;  // +0xBC: the music group comes back when hotspot 2 is still
	int _hotspot2 = -1;      // +0xC0
	int _hotspot3 = -1;      // +0xC4
	int _hotspotScene = 0;   // +0xC8: which of the three hotspot scenes is next
};

/** The story room class the factory builds for @p room, or nullptr. */
RoomClass *createStoryRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_STORY_ROOMS_H
