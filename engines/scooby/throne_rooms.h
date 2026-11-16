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

#ifndef SCOOBY_THRONE_ROOMS_H
#define SCOOBY_THRONE_ROOMS_H

#include "scooby/room_class.h"

namespace Scooby {

/**
 * Phantom's throne room, ROOM_P10_Throne_Room (0x417A90, vtable 0x45EC1C).
 * The first visit plays Jane's movie (18) instead. Jane takes the duster;
 * one hotspot scares the gang; the throne answers the ruby once the three
 * balls are found, with movie 9, and is hidden after.
 */
class ThroneRoomRoom : public RoomClass {
public:
	explicit ThroneRoomRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x417AF0
	void enter(const Common::String &from) override;                               // 0x417B20
	void tick() override;                                                          // 0x417E80
	bool click(int sprite) override;
	void event(const Common::String &object, bool success) override;              // 0x417FB0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x417C10
	void saveKeys(Common::INIFile &profile) override;                              // 0x418030
	void loadKeys(const Common::INIFile &profile) override;                        // 0x418090

	bool isJaneMoviePlayed() const { return _janeMoviePlayed; }
	void setJaneMoviePlayed(bool played) { _janeMoviePlayed = played; }
	int getEntrances() const { return _entrances; }
	int getThrone() const { return _throne; }
	int getHotspot3() const { return _hotspot3; }
	/** All three balls found (game +0x25B, +0x25C, +0x25D). */
	static bool allBallsFound(ScoobyEngine &engine);
	/** The ruby used: ever taken (0x407060) and no longer in a slot (0x4070C0). */
	static bool isRubyUsed(ScoobyEngine &engine);

private:
	int findSprite(const Common::String &object) const;

	int _throne = -1;               // +0xB8: OBJ_P10_THRONE_ROOM_THRONE
	bool _janeMoviePlayed = false;  // +0xBC, saved as JaneMoviePlayed
	int _entrances = 0;             // +0xC0, saved as Entrances; nothing changes it
	int _hotspot3 = -1;             // +0xC4: OBJ_P10_THRONE_ROOM_HOTSPOT3
	int _clicked = -1;
};

/** The class for this room, or nullptr. */
RoomClass *createThroneRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_THRONE_ROOMS_H
