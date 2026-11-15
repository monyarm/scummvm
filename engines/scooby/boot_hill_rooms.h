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

#ifndef SCOOBY_BOOT_HILL_ROOMS_H
#define SCOOBY_BOOT_HILL_ROOMS_H

#include "scooby/room_class.h"

namespace Scooby {

/**
 * Showdown's graveyard, ROOM_P21_Boot_Hill1 to ROOM_P23_Boot_Hill3 (0x40ECB0,
 * vtable 0x45E74C), one class for the three rooms. The brick is in one of
 * their nine graves (GraveLocation): clicked, the gang digs it up, through
 * movie 3; a wrong grave brings the ghost. Boot Hill 2 keeps the gang until
 * Scooby has had the level's snacks.
 */
class BootHillRoom : public RoomClass {
public:
	BootHillRoom(ScoobyEngine &engine, const Common::String &name) : RoomClass(engine), _name(name) {}

	void reset() override;                                                          // 0x40ED10
	void prepare() override;                                                        // 0x40ED30 (+0x30)
	void enter(const Common::String &from) override;                                // 0x40EE10
	void tick() override;                                                           // 0x40F210
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x40F4F0
	void event(const Common::String &object, bool success) override;               // 0x40F250
	bool leaveBy(const Common::String &exitObject) override;                        // 0x40EFD0 (+0x24)
	void saveKeys(Common::INIFile &profile) override;                               // 0x40F660
	void loadKeys(const Common::INIFile &profile) override;                         // 0x40F6A0

	/** The grave GraveLocation names (0-8), as its object: three a room, P21 first. */
	static Common::String getGraveObject(int location);

	bool isDug() const { return _dug; }
	bool hasDigScenePlayed() const { return _digScenePlayed; }
	const Common::String &getLastScene() const { return _lastScene; }

private:
	bool _digDone = false;         // +0xB8: the dig scene is over
	bool _digScenePlayed = false;  // +0xB9: DigScenePlayed
	bool _dug = false;             // +0xBA: the brick dug up, for the next entry
	bool _digScene = false;        // the dig scene is playing
	Common::String _name;          // +0x08
	Common::String _lastScene;
};

/** The graveyard's room class the factory builds for @p room, or nullptr. */
RoomClass *createBootHillRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_BOOT_HILL_ROOMS_H
