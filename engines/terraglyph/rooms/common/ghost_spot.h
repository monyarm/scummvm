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

#ifndef TERRAGLYPH_GHOST_SPOT_H
#define TERRAGLYPH_GHOST_SPOT_H

#include "terraglyph/rooms/class.h"

namespace Terraglyph {

/**
 * The ghost spot, ROOM_Ghost_Spot (Showdown 0x4222E0, vtable 0x45EEAC;
 * Phantom 0x420350, vtable 0x45EFD4, the same code): where the game goes
 * once the ghost has scared the gang. The background pans 6 pixels every
 * other tick for 396 ticks, with input held, then the game goes on to its
 * town (game vtable +0x64). A skip key goes there at once.
 */
class GhostSpotRoom : public RoomClass {
public:
	explicit GhostSpotRoom(TerraglyphEngine &engine) : RoomClass(engine), _countdown(0), _skip(false) {}

	void enter(const Common::String &from) override;                                // 0x422420
	void tick() override;                                                           // 0x422350
	bool skip() override;                                                           // 0x422350's input slot 5
	void reset() override { _countdown = 0; _skip = false; }
	bool isBusy() const override { return true; }
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x422490

	int getCountdown() const { return _countdown; }
	/** The global that lets the background move every other tick (0x475F10). */
	static int getToggle();
	static void setToggle(int toggle);
	/**
	 * Game vtable +0x64, where the ghost spot goes on to. Showdown's
	 * (0x4303B0) is always ROOM_P01_Town_Center. Phantom's (0x430DD0) is
	 * ROOM_P27_Below_Balcony if @p current, the room loaded (game +0x04's
	 * name), is one of five castle rooms, else ROOM_P04_Town.
	 */
	static const char *getReturnRoom(const Common::String &gameId, const Common::String &current);

	static const int kStart = -1280;  // 0xC4A00000
	static const int kPan = 6;        // 0x45EEA8
	static const int kWrap = 1280;    // 0x45EB14
	static const int kTicks = 396;    // 0x18C: 11 seconds

private:
	int _countdown; // +0xB8
	bool _skip;     // input slot 5 seen, for the next tick
};

} // End of namespace Terraglyph

#endif // TERRAGLYPH_GHOST_SPOT_H
