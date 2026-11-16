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

#ifndef SCOOBY_ALLEY_ROOMS_H
#define SCOOBY_ALLEY_ROOMS_H

#include "scooby/room_class.h"
#include "scooby/room_script.h"

namespace Scooby {

/**
 * Showdown's back alley, ROOM_P19_Back_Alley (0x40E7A0, vtable 0x45E70C):
 * between the bank vault and the General Store. Rattlesnakes are heard in
 * the distance, and the gang won't go into the store without the level's
 * snacks.
 */
class BackAlleyRoom : public RoomClass {
public:
	explicit BackAlleyRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;         // 0x40E800
	void tick() override;                                    // 0x40EBC0
	bool leaveBy(const Common::String &exitObject) override; // 0x40E8B0 (+0x24)

	int getHotspot1() const { return _hotspot1; }
	int getHotspot3() const { return _hotspot3; }
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	int _hotspot3 = -1; // +0xB8
	int _hotspot1 = -1; // +0xBC
	Common::String _lastScene;
	Common::String _lastSound;
};

/** The back alley's room class the factory builds for @p room, or nullptr. */
RoomClass *createAlleyRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_ALLEY_ROOMS_H
