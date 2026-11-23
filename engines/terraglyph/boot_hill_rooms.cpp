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

#include "terraglyph/boot_hill_rooms.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const char *const kHill1 = "ROOM_P21_Boot_Hill1";
static const char *const kHill2 = "ROOM_P22_Boot_Hill2";
static const char *const kHill3 = "ROOM_P23_Boot_Hill3";
static const char *const kBrick = "OBJ_INV_BRICK";
// The movie the dig plays before the room comes back (game vtable +0x08, push 3).
static const int kDigMovie = 3;

static bool isGrave(const Common::String &object) {
	for (int i = 0; i < 9; i++) {
		if (object == BootHillRoom::getGraveObject(i))
			return true;
	}
	return false;
}

static bool inSlots(const Inventory &inventory, const char *item) {
	for (uint i = 0; i < 4; i++) {
		if (inventory.getSlot(i) == item)
			return true;
	}
	return false;
}

Common::String BootHillRoom::getGraveObject(int location) {
	if (location < 0 || location > 8)
		return Common::String();
	const int hill = location / 3 + 1;
	return Common::String::format("OBJ_P%d_BOOT_HILL%d_GRAVE%d", 20 + hill, hill, location % 3 + 1);
}

void BootHillRoom::reset() {
	// 0x40ED10, after the base reset.
	_digDone = _digScenePlayed = _dug = false;
}

void BootHillRoom::prepare() {
	// 0x40ED30 (+0x30). Back from digging, its scene (210B_), unplayed, in
	// the room with the grave, until the dig scene counts as played. Else
	// 340E2_ in Boot Hill 2, or none; 0x40C6B0 sets only the name, so
	// whether it has played stays as it was.
	RoomScript &script = *_engine.getScript();
	RoomScript::Memory &memory = script.getMemory(_name);
	const int grave = _engine.getGameInt("GraveLocation");
	const bool here = (_name == kHill1 && grave >= 0 && grave <= 2) || (_name == kHill2 && grave >= 3 && grave <= 5) ||
	                  (_name == kHill3 && grave >= 6 && grave <= 8);
	if (_dug && !_digScenePlayed && here) {
		script.setOpening("Boot_Hill_1.210B_");
		return;
	}
	const bool played = memory.openingPlayed;
	script.setOpening(_name == kHill2 ? "Boot_Hill_2.340E2_" : "");
	memory.openingPlayed = played;
}

void BootHillRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x40F4F0: the graves aren't built once the brick is dug up or taken.
	if (isGrave(object) && (_dug || _engine.getInventory()->hasItem(kBrick)))
		claim.build = Room::Claim::kSkip;
}

void BootHillRoom::enter(const Common::String &from) {
	// 0x40EE10, after the base entry.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	_lastScene.clear();
	_digScene = false;
	_engine.getSound()->play("Boot_Hill.Ambient");
	// The snacks the room needs: the level's plus 2.
	script.setSnacksNeeded(_engine.getDifficulty() + 2);
	// Back from digging: the brick.
	if (_dug) {
		if (!inventory.hasItem(kBrick))
			inventory.addItem(kBrick, "ANIM_TOOLBARBRICK", "ANIM_CURSORBRICK");
		_dug = false;
	}
	if (inSlots(inventory, kBrick))
		_digScenePlayed = true;
	// The gang walking in: Velma (up) into Boot Hill 2 from 1, Daphne
	// (down-right) and Shaggy (up-right) into 1 from 2 and 3, Fred
	// (down-left) into 3 from 1 and from the lit tunnel.
	const char *who = nullptr, *direction = nullptr;
	if (from == kHill1) {
		if (_name == kHill2) {
			who = "VELMA";
			direction = "U";
		} else if (_name == kHill3) {
			who = "FRED";
			direction = "DL";
		}
	} else if (from == kHill2 && _name == kHill1) {
		who = "DAPHNE";
		direction = "DR";
	} else if (from == kHill3 && _name == kHill1) {
		who = "SHAGGY";
		direction = "UR";
	} else if (from == "ROOM_P18_Tunnel_Lit") {
		who = "FRED";
		direction = "DL";
	}
	if (who) {
		if (Character *c = script.getCharacter(who))
			c->walkIn(room, *_engine.getSound(), direction);
	}
}

void BootHillRoom::event(const Common::String &object, bool success) {
	// 0x40F250: a grave clicked. The brick's grave, with the brick never
	// taken, no scene and the opening done: a dig scene (rand() % 3),
	// holding input. Any other grave: the ghost (0x40A990).
	if (!isGrave(object))
		return;
	RoomScript &script = *_engine.getScript();
	if (object != getGraveObject(_engine.getGameInt("GraveLocation"))) {
		debugC(1, kDebugScript, "%s: %s isn't the grave; the ghost", _name.c_str(), object.c_str());
		script.showGhost();
		return;
	}
	if (_engine.getInventory()->hasItem(kBrick) || script.isScenePlaying() || !script.isOpeningDone())
		return;
	static const char *const kDigs[] = { "Boot_Hill_1.190B_", "Boot_Hill_1.190B2_", "Boot_Hill_1.190B3_" };
	const char *scene = kDigs[_engine.getRandomNumber(2)];
	if (script.startScene(scene, true)) {
		_lastScene = scene;
		_digScene = true;
		_digDone = false;
		debugC(1, kDebugScript, "%s: digging, %s", _name.c_str(), scene);
	}
}

void BootHillRoom::tick() {
	// 0x40F210, after the base tick: the dig scene over, the brick is dug up
	// and the room comes back through movie 3.
	RoomScript &script = *_engine.getScript();
	if (_digScene && !script.isScenePlaying()) {
		_digScene = false;
		_digDone = true;
	}
	if (_digDone) {
		_digDone = false;
		_dug = true;
		debugC(1, kDebugScript, "%s: dug up at tick %u; movie %d", _name.c_str(), script.getTick(), kDigMovie);
		_engine.requestRoom(_name, kDigMovie);
	}
}

bool BootHillRoom::leaveBy(const Common::String &exitObject) {
	// 0x40EFD0 (+0x24): Boot Hill 2's way back to 1 or 3 waits for the
	// level's snacks (+0x88 against level + 2): a line by level (190A_,
	// 190A2_, 190A3_), and the snack box flashes (0x40D4D0). Else the
	// default exit handler.
	if (_name != kHill2)
		return false;
	RoomScript &script = *_engine.getScript();
	const Common::String destination = script.readExit(exitObject).destination;
	if (destination != kHill1 && destination != kHill3)
		return false;
	const int level = _engine.getDifficulty();
	if (script.getMemory(_name).snacksGiven >= (uint)(level + 2))
		return false;
	static const char *const kLines[] = { "Boot_Hill_1.190A_", "Boot_Hill_1.190A2_", "Boot_Hill_1.190A3_" };
	if (level >= 0 && level <= 2 && script.startScene(kLines[level], true))
		_lastScene = kLines[level];
	_engine.getInventory()->flashSnackBox();
	debugC(1, kDebugScript, "%s: not without %d snacks (%u given)", _name.c_str(), level + 2, script.getMemory(_name).snacksGiven);
	return true;
}

void BootHillRoom::saveKeys(Common::INIFile &profile) {
	// 0x40F660: the base keys, then DigScenePlayed, in the room's section.
	profile.setKey("DigScenePlayed", _name, _digScenePlayed ? "Yes" : "No");
}

void BootHillRoom::loadKeys(const Common::INIFile &profile) {
	// 0x40F6A0 -> 0x40C560: a missing key reads as false.
	Common::String v;
	_digScenePlayed = profile.getKey("DigScenePlayed", _name, v) && v == "Yes";
}

RoomClass *createBootHillRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0): the three rooms share the class.
	if (room == kHill1 || room == kHill2 || room == kHill3)
		return new BootHillRoom(engine, room);
	return nullptr;
}

} // End of namespace Terraglyph
