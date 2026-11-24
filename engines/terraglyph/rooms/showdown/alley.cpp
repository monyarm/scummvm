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

#include "terraglyph/rooms/showdown/alley.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kAlley = "ROOM_P19_Back_Alley";
static const char *const kStore = "ROOM_P15_General_Store_Interior";

static int findSprite(const Room &room, const char *object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void BackAlleyRoom::enter(const Common::String &from) {
	// 0x40E800, after the base entry: the snakes in the distance; the level's
	// snacks (+0x24 = level + 2) before the store. Fred walks in up and left
	// from the store, Shaggy up and right from the vault or the open safe.
	// Hotspots 3 and 1 are built as the default path builds a clickable
	// (0x40EAC0 calls 0x408DC0 with 1, -1), and found by name.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	_lastScene.clear();
	_hotspot3 = findSprite(room, "OBJ_P19_BACK_ALLEY_HOTSPOT3");
	_hotspot1 = findSprite(room, "OBJ_P19_BACK_ALLEY_HOTSPOT1");
	const bool found = _engine.getSound()->play("Back_Alley.Snakes_Distant");
	_lastSound = Common::String::format("Back_Alley.Snakes_Distant found %d", found ? 1 : 0);
	script.setSnacksNeeded(_engine.getDifficulty() + 2);
	if (from == kStore) {
		if (Character *fred = script.getCharacter("FRED"))
			fred->walkIn(room, *_engine.getSound(), "UL");
	} else if (from == "ROOM_P16_Bank_Vault" || from == "ROOM_Open_Safe") {
		if (Character *shaggy = script.getCharacter("SHAGGY"))
			shaggy->walkIn(room, *_engine.getSound(), "UR");
	}
}

bool BackAlleyRoom::leaveBy(const Common::String &exitObject) {
	// 0x40E8B0 (+0x24): into the store with fewer snacks given here (+0x88)
	// than level + 2: a line by level (350B_, 350B2_, 350B3_), held, and the
	// snack box flashes (0x40D4D0). Else the default exit handler.
	RoomScript &script = *_engine.getScript();
	if (script.readExit(exitObject).destination != kStore)
		return false;
	const int level = _engine.getDifficulty();
	if (script.getMemory(kAlley).snacksGiven >= (uint)(level + 2))
		return false;
	static const char *const kLines[] = { "Back_Alley.350B_", "Back_Alley.350B2_", "Back_Alley.350B3_" };
	if (level >= 0 && level <= 2 && script.startScene(kLines[level], true))
		_lastScene = kLines[level];
	_engine.getInventory()->flashSnackBox();
	debugC(1, kDebugScript, "back alley: not into the store without %d snacks (%u given)", level + 2, script.getMemory(kAlley).snacksGiven);
	return true;
}

void BackAlleyRoom::tick() {
	// 0x40EBC0, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	// Hotspot 3 playing its B, on step 0: one of two lines, not held. The
	// original draws rand() & 1 on every such tick.
	if (_hotspot3 >= 0 && room.getMode(_hotspot3) != 0 && room.getStep(_hotspot3) == 0) {
		const Common::String &anim = room.getAnim(_hotspot3);
		if (!anim.empty() && anim.lastChar() == 'B') {
			const char *scene = _engine.getRandomNumber(1) == 0 ? "Back_Alley.HOTSPOT3B_" : "Back_Alley.HOTSPOT3B2_";
			if (script.startScene(scene, false))
				_lastScene = scene;
		}
	}
	// Hotspot 1 on its step 8: Daphne and Scooby scared.
	if (_hotspot1 >= 0 && room.getStep(_hotspot1) == 8) {
		if (Character *daphne = script.getCharacter("DAPHNE"))
			daphne->setState(room, *_engine.getSound(), Character::kStateScared);
		if (Character *scooby = script.getCharacter("SCOOBY"))
			scooby->setState(room, *_engine.getSound(), Character::kStateScared);
	}
}

RoomClass *createAlleyRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kAlley)
		return new BackAlleyRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
