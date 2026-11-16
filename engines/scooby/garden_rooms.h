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

#ifndef SCOOBY_GARDEN_ROOMS_H
#define SCOOBY_GARDEN_ROOMS_H

#include "scooby/room_class.h"

namespace Scooby {

/**
 * Phantom's gardener's house, ROOM_P05_Gardeners_House (0x411BF0, vtable
 * 0x45E914): its plaque, which gives a Big Head scene until a ball is
 * found and then the crest puzzle; the door to the garden, which the
 * puzzle opens; the openings coming back from the puzzle.
 */
class GardenersHouseRoom : public RoomClass {
public:
	explicit GardenersHouseRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override { _returnLine = 0; _bigHeadLine = 0; }                 // 0x411C50
	void arrive(const Common::String &from) override;                              // 0x411C70
	void enter(const Common::String &from) override;                               // 0x411D60
	void event(const Common::String &object, bool success) override;              // 0x411F10
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x412110

	int getDoor() const { return _door; }
	const Common::String &getScream() const { return _scream; }

private:
	int _returnLine = 0;  // +0xB8: which of 130B_, 130B1_, 130B2_ is next
	int _bigHeadLine = 0; // +0xBC: which of 130A_, 130A1_, 130A2_ is next
	int _door = -1;       // +0xC0
	Common::String _scream;
};

/**
 * Phantom's garden, ROOM_P06_Garden (0x411A20, vtable 0x45E8D4): the
 * tracks, there once the maze has been entered, and their three lines.
 */
class GardenRoom : public RoomClass {
public:
	explicit GardenRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                               // 0x411A80
	void event(const Common::String &object, bool success) override;              // 0x411AD0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x411B50

private:
	int _tracksLine = 0; // +0xB8
};

/**
 * Phantom's garden plaque, ROOM_Garden_Plaque (0x40F840, vtable 0x45E7D0):
 * four buttons, the lion, the bush, the deer and the dragon, to press in
 * the crest's order (the game's CrestSequence0-3). After four presses, back
 * to the gardener's house, with CrestPuzzle if all were right.
 */
class GardenPlaqueRoom : public RoomClass {
public:
	explicit GardenPlaqueRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // game +0x287 (0x42E880)
	void enter(const Common::String &from) override;                               // 0x40F8A0
	void tick() override;                                                          // 0x40F8E0
	bool click(int sprite) override;                                               // game +0x20 bit 4, read by the tick
	bool isBusy() const override { return _presses >= (int)kButtons; }             // game +0x24
	void event(const Common::String &object, bool success) override;              // 0x40FB20
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x40F940

	static const uint kButtons = 4;
	/** The button objects, in the class's order (+0xC0): lion, bush, deer, dragon. */
	static const char *getButtonObject(uint i);
	/** Each button's place in the crest (1-4). */
	static int getButtonValue(uint i);
	int getButton(uint i) const { return _buttons[i]; }
	bool isPressed(uint i) const { return _pressed[i]; }
	int getPresses() const { return _presses; }
	bool isAllRight() const { return _allRight; }
	/** Game +0x287: the plaque was tried since the last new game or visit. */
	static bool wasTried();

private:
	int _presses = 0;                       // +0xB8
	bool _allRight = true;                  // +0xBC
	int _buttons[kButtons] = { -1, -1, -1, -1 }; // +0xC0
	bool _pressed[kButtons] = {};           // +0xD0
	int _clicked = -1;
};

/** The class for one of these rooms, or nullptr. */
RoomClass *createGardenRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_GARDEN_ROOMS_H
