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

#ifndef SCOOBY_STATION_ROOMS_H
#define SCOOBY_STATION_ROOMS_H

#include "scooby/room_class.h"
#include "scooby/room_script.h"

namespace Scooby {

/**
 * Showdown's railroad station, ROOM_P09_Railroad_Station (0x4207C0, vtable
 * 0x45EE28): Gummy sleeps here with his donkey once Artie has been met.
 * Clicked, they lead to his whistle puzzle (ROOM_Gummy). With the puzzle
 * solved he wants his dentures; given them, he gives his clue, and the way
 * to the chase opens.
 */
class RailroadStationRoom : public RoomClass {
public:
	explicit RailroadStationRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x420820
	void arrive(const Common::String &from) override;                               // 0x4212C0 (+0x2C)
	void enter(const Common::String &from) override;                                // 0x4210F0
	void tick() override;                                                           // 0x420840
	bool click(int sprite) override;                                                // 0x420840, Gummy and the donkey clicked
	void event(const Common::String &object, bool success) override;                // 0x421070
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x4213C0
	bool isBusy() const override { return _thanks; }                                // game +0x24

	int getHotspot3() const { return _hotspot3; }
	bool isGiving() const { return _gives; }
	const Common::String &getLastScene() const { return _lastScene; }
	const RoomScript::Exit &getLastExit() const { return _lastExit; }

private:
	/** 0x421680: Gummy's hint, 220C_, 220C2_, 220C3_ in turn; with the dentures had, back to the first after the second. */
	const char *nextHint();
	void leave(const char *destination, const char *scene, int movie);

	int _hotspot2 = -1;         // +0xB8
	int _hotspot3 = -1;         // +0xBC
	bool _gives = false;        // +0xC0: Gummy gives his clue when idle
	bool _thanks = false;       // +0xC1: then the thanks and the suspects
	int _hint = 0;              // +0xC8: Gummy's next hint
	int _gummyClicked = 0;      // 0 none, 1 with the arrow, 2 with the dentures
	bool _donkeyClicked = false;
	Common::String _lastScene;
	RoomScript::Exit _lastExit;
};

/** The railroad station's room class the factory builds for @p room, or nullptr. */
RoomClass *createStationRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_STATION_ROOMS_H
