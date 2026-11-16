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

#ifndef SCOOBY_DINING_ROOMS_H
#define SCOOBY_DINING_ROOMS_H

#include "scooby/room_class.h"

namespace Scooby {

/**
 * Phantom's dining room, ROOM_P12_Dining_Room (0x40FD00, vtable 0x45E810).
 * The first visit plays Maria's movie (35) into her game (ROOM_Maria);
 * after, a click on her with the arrow goes back to it until it is won
 * (MariaPuzzle). Won, she gives her clue, the gang thanks her on the way
 * to the suspects, and the burger shows.
 */
class DiningRoomRoom : public RoomClass {
public:
	explicit DiningRoomRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x40FD60
	void enter(const Common::String &from) override;                               // 0x40FD80
	void tick() override;                                                          // 0x410110
	void idle() override;                                                          // 0x4100C0
	bool click(int sprite) override;
	bool isBusy() const override { return _thanks; }                               // game +0x24
	void event(const Common::String &object, bool success) override;              // 0x410610
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x40FEE0
	Common::String idleLine(RoomScript &script, const Common::String &name) override; // 0x410550
	void saveKeys(Common::INIFile &profile) override;                              // 0x4104D0
	void loadKeys(const Common::INIFile &profile) override;                        // 0x410510

	bool isMoviePlayed() const { return _moviePlayed; }
	void setMoviePlayed(bool played) { _moviePlayed = played; }
	bool isGiving() const { return _giving; }
	bool isThanking() const { return _thanks; }

private:
	bool _giving = false;      // +0xB8: her clue given, she gives when idle
	bool _thanks = false;      // +0xB9: given; the thanks when she is idle again
	bool _moviePlayed = false; // +0xBA, saved as MariaMoviePlayed
	int _clicked = -1;
};

/** The class for this room, or nullptr. */
RoomClass *createDiningRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_DINING_ROOMS_H
