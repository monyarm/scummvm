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

#ifndef TERRAGLYPH_STRONGBOX_H
#define TERRAGLYPH_STRONGBOX_H

#include "terraglyph/rooms/class.h"
#include "terraglyph/rooms/script.h"

namespace Terraglyph {

/**
 * Showdown's strong boxes, ROOM_SB1 to ROOM_SB5 (one class, 0x42A340,
 * vtable 0x45F1A8): a strong box's lid opens with Strongbox.opens, the
 * narrator's Bank_Vault.340D_ plays, and the gang is back in the vault.
 * Nothing in the shipped game sends the gang to these rooms: the right
 * strong box plays a movie instead (WP-040).
 */
class StrongBoxRoom : public RoomClass {
public:
	explicit StrongBoxRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x42A3A0
	void enter(const Common::String &from) override;                                // 0x42A5B0
	void tick() override;                                                           // 0x42A3C0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42A600
	// Game +0x24 set on entry, and the cursor faded (0x435150, 0x12), until
	// the room changes.
	bool isBusy() const override { return true; }

	int getLid() const { return _lid; }
	int getCountdown() const { return _countdown; }
	bool isOpening() const { return _opening; }
	bool isSceneDone() const { return _sceneDone; }
	const Common::String &getLastSound() const { return _lastSound; }
	const Common::String &getLastScene() const { return _lastScene; }
	const RoomScript::Exit &getLastExit() const { return _lastExit; }

private:
	bool _sceneDone = false;    // +0xB8: 340D_'s done flag
	bool _opening = false;      // +0xB9: the lid playing
	int _lid = -1;              // +0xBC
	int _countdown = 36;        // +0xC0
	bool _scenePlaying = false; // 340D_ started, its done flag not yet set
	Common::String _lastSound;
	Common::String _lastScene;
	RoomScript::Exit _lastExit;
};

/** The strong boxes' room class the factory builds for @p room, or nullptr. */
RoomClass *createStrongBoxRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_STRONGBOX_H
