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

#ifndef SCOOBY_POST_ROOMS_H
#define SCOOBY_POST_ROOMS_H

#include "scooby/room_class.h"
#include "scooby/room_script.h"

namespace Scooby {

/**
 * Showdown's post office, ROOM_P08_Post_Office (0x413930, vtable
 * 0x45E950): a hole in the floor leads down to the tunnel. The gang won't
 * go down in the dark; with the lantern dropped down it, Fred tosses it in
 * and the way is lit. Shaggy and Scooby come up from the dark tunnel on
 * their own.
 */
class PostOfficeRoom : public RoomClass {
public:
	explicit PostOfficeRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x413A00
	void arrive(const Common::String &from) override;                               // 0x413EE0 (+0x2C)
	void enter(const Common::String &from) override;                                // 0x413A30
	void tick() override;                                                           // 0x414270
	bool click(int sprite) override;                                                // 0x414270, the lantern on the hole
	void event(const Common::String &object, bool success) override;                // 0x4140F0
	bool leaveBy(const Common::String &exitObject) override;                        // 0x413BC0 (+0x24)
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x413F40
	void saveKeys(Common::INIFile &profile) override;                               // 0x4141D0
	void loadKeys(const Common::INIFile &profile) override;                         // 0x414210
	// Game +0x24, set on every tick from the lantern's scene to the room's
	// change.
	bool isBusy() const override { return _tossPending; }

	int getHole() const { return _hole; }
	int getHoleLit() const { return _holeLit; }
	int getHotspot2() const { return _hotspot2; }
	int getHotspot3() const { return _hotspot3; }
	const Common::String &getOldRoomName() const { return _oldRoomName; }
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	int _holeLine = 0;          // +0xB8: the dark hole's next line
	int _hotspot3Line = 0;      // +0xBC
	Common::String _oldRoomName = "ROOM_P03_Street1"; // +0xC0, saved as OldRoomName
	int _hotspot3 = -1;         // +0xC8
	int _hotspot2 = -1;         // +0xCC
	bool _tossPending = false;  // +0xD0: 160C_'s done flag
	bool _tossing = false;      // +0xD1
	int _holeLit = -1;          // +0xD4
	int _hole = -1;             // +0xD8
	bool _lanternOnHole = false; // the hole clicked with the lantern
	bool _lanternScene = false; // 160C_ started, its done flag not yet set
	Common::String _lastScene;
	Common::String _lastSound;
};

/** The post office's room class the factory builds for @p room, or nullptr. */
RoomClass *createPostRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_POST_ROOMS_H
