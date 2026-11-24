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

#ifndef TERRAGLYPH_STORE_ROOMS_H
#define TERRAGLYPH_STORE_ROOMS_H

#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"

namespace Terraglyph {

/**
 * Showdown's front of the General Store, ROOM_P10_Front_Of_General_Store
 * (0x40F6E0, vtable 0x45E78C): the clock over the door. The rusty hammer
 * breaks it, and after movie 2 its broken face is in the toolbar.
 */
class StoreFrontRoom : public RoomClass {
public:
	explicit StoreFrontRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x40F740
	void prepare() override;                                                        // 0x40F890 (+0x30)
	void enter(const Common::String &from) override;                                // 0x40FB70
	void tick() override;                                                           // 0x40F760
	void event(const Common::String &object, bool success) override;                // 0x40F8D0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x40FA40

	int getClock() const { return _clock; }
	int getNoFace() const { return _noFace; }
	bool isClockTaken() const { return _clockTaken; }
	bool isScoobyWaiting() const { return _scoobyWaiting; }
	const Common::String &getLastScene() const { return _lastScene; }

private:
	int _clock = -1;            // +0xB8: OBJ_INV_CLOCK, a requires object
	int _noFace = -1;           // +0xBC: the clock without its face
	bool _clockTaken = false;   // +0xC0: the hammer used; movie 2 when free
	int _clockLine = 0;         // +0xC4: the clock's next line
	bool _scoobyWaiting = false; // +0xC8: hotspot 1 clicked
	Common::String _lastScene;
};

/**
 * Showdown's General Store, ROOM_P15_General_Store_Interior (0x4125D0,
 * vtable 0x45E8D0): a rattlesnake guards the lantern. Clicked, it strikes;
 * the blanket from the jail goes over it, Fred tosses it, and then Fred can
 * take the lantern.
 */
class GeneralStoreRoom : public RoomClass {
public:
	explicit GeneralStoreRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x412630
	void enter(const Common::String &from) override;                                // 0x412DF0
	void roomLeft() override;                                                       // 0x4129B0
	void tick() override;                                                           // 0x412650
	void event(const Common::String &object, bool success) override;                // 0x412A00
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x412BD0
	// Game +0x24, set on every tick while the blanket goes over the snake or
	// Fred takes the lantern.
	bool isBusy() const override { return _blanketDone || _lanternClicked; }

	int getSnake() const { return _snake; }
	int getLantern() const { return _lantern; }
	bool isCovered() const { return _covered; }
	bool isStrikePending() const { return _strike; }
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	void playSound(const char *name);

	int _snake = -1;             // +0xB8
	int _lantern = -1;           // +0xBC
	int _hotspot1 = 0;           // +0xC4
	int _snakeLine = 0;          // +0xC8: the snake's next line
	bool _strike = false;        // +0xCC: the snake strikes on its step 0
	bool _blanketDone = false;   // +0xCD: Click_Blanket_On_Snake_'s done flag
	bool _tossing = false;       // +0xCE: Fred tosses the blanket
	bool _lanternClicked = false; // +0xCF
	bool _grabbing = false;      // +0xD0: Fred takes the lantern
	bool _covered = false;       // the snake under the blanket
	bool _blanketScene = false;  // the scene started, its done flag not yet set
	Common::String _lastScene;
	Common::String _lastSound;
};

/** The General Store's room class the factory builds for @p room, or nullptr. */
RoomClass *createStoreRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_STORE_ROOMS_H
