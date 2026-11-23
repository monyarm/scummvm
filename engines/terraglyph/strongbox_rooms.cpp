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

#include "common/debug.h"

#include "terraglyph/detection.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"
#include "terraglyph/strongbox_rooms.h"

namespace Terraglyph {

static const char *const kScene = "Bank_Vault.340D_";

void StrongBoxRoom::reset() {
	// 0x42A3A0, after the base reset.
	_sceneDone = false;
	_opening = false;
}

void StrongBoxRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42A600: OBJ_SB1_LID to OBJ_SB5_LID a sprite (graphics_AnimSprite_ctor)
	// under the first layer (the engine's +0x58, 0x475F4C) at depth 0, then
	// stopped on step 0 (graphics_AnimSprite_Start 0, 3, -1).
	if (object.size() == 11 && object.hasPrefix("OBJ_SB") && object.hasSuffix("_LID") && object[6] >= '1' && object[6] <= '5') {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
	}
}

void StrongBoxRoom::enter(const Common::String &from) {
	// 0x42A5B0, after the base entry: the toolbar off (0x407CC0, in the
	// toolbar's hidden list), the cursor faded, input held, a second to
	// wait.
	Room &room = *_engine.getRoom();
	_lid = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s).hasSuffix("_LID") && room.getObjectName(s).hasPrefix("OBJ_SB"))
			_lid = s;
	}
	if (_lid >= 0)
		room.setPlayer(_lid, 0, -1);
	_countdown = 36;
	_scenePlaying = false;
	_lastSound.clear();
	_lastScene.clear();
	_lastExit = RoomScript::Exit();
}

void StrongBoxRoom::tick() {
	// 0x42A3C0, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();

	// 340D_'s done flag, set by the base tick as it ends.
	if (_scenePlaying && script.getScene() != kScene) {
		_scenePlaying = false;
		_sceneDone = true;
	}

	// A second on: Strongbox.opens, and the lid opens once at 5 ticks a
	// step (graphics_AnimSprite_Start 3, 5, -1).
	if (_countdown-- == 0) {
		const bool found = _engine.getSound()->play("Strongbox.opens");
		_lastSound = Common::String::format("Strongbox.opens found %d", found ? 1 : 0);
		if (_lid >= 0) {
			room.setPlayer(_lid, 3, -1, 5);
		}
		_opening = true;
	}
	// The lid open: 340D_, held, with its done flag.
	if (_opening && (_lid < 0 || room.getMode(_lid) == 0)) {
		_opening = false;
		if (script.startScene(kScene, true)) {
			_scenePlaying = true;
			_lastScene = kScene;
		}
	}
	// Over: the vault, through the room's exit handler (+0x24, the base's).
	if (_sceneDone) {
		RoomScript::Exit exit;
		exit.destination = "ROOM_P16_Bank_Vault";
		exit.movie = -1;
		_lastExit = exit;
		script.takeExit(exit);
		_sceneDone = false;
	}
}

RoomClass *createStrongBoxRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0): ROOM_SB1 to ROOM_SB5 share the class.
	static const char *const kRooms[] = { "ROOM_SB1", "ROOM_SB2", "ROOM_SB3", "ROOM_SB4", "ROOM_SB5" };
	for (uint i = 0; i < ARRAYSIZE(kRooms); i++) {
		if (room == kRooms[i])
			return new StrongBoxRoom(engine);
	}
	return nullptr;
}

} // End of namespace Terraglyph
