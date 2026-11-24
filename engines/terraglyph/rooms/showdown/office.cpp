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

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/rooms/showdown/office.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kOffice = "ROOM_P02_Sheriffs_Office";
static const char *const kJail = "ROOM_P07_Jail";
static const char *const kDoor = "OBJ_P02_DOOR";
static const char *const kOilCan = "OBJ_INV_OILCAN";
static const char *const kBlanket = "OBJ_INV_BLANKET";
static const char *const kDoorOpen = "ANIM_P02_DOOROPEN";

static int findSprite(const Room &room, const char *object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

static bool isIdle(const Character *c) {
	return c && c->getState() == Character::kStateIdle;
}

// 0x407060 and 0x4070C0: taken, and no longer in one of the four slots.
static bool used(const Inventory &inventory, const char *item) {
	if (!inventory.hasItem(item))
		return false;
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (inventory.getSlot(i) == item)
			return false;
	}
	return true;
}

// No scene (+0x14) and the opening done (+0x10, or no +0x78).
static bool isFree(const RoomScript &script) {
	return !script.isScenePlaying() && script.isOpeningDone();
}

// The oil can used, and the blanket not yet taken from the cell.
static bool doorOiled(const Inventory &inventory) {
	return used(inventory, kOilCan) && !inventory.hasItem(kBlanket);
}

void SheriffsOfficeRoom::playSound(const char *name) {
	const bool found = _engine.getSound()->play(name);
	_lastSound = Common::String::format("%s found %d", name, found ? 1 : 0);
}

void SheriffsOfficeRoom::prepare() {
	// 0x416680 (+0x30): while MoaningActive, 140A_; otherwise none. Unplayed
	// either way.
	_engine.getScript()->setOpening(_engine.getGameFlag("MoaningActive") ? "Sheriffs_Office.140A_" : "");
}

void SheriffsOfficeRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x416C30. The cell door a requires object (0x408C80 with -1: layer 0)
	// at depth 1: with the oil can used, open, or with the bed gone once the
	// blanket has been taken. The way into the jail not built once the
	// blanket has been taken. Hotspots 1 and 4 are built as the default path
	// builds a clickable (0x408DC0 with 1, -1), so they are left to it.
	Inventory &inventory = *_engine.getInventory();
	if (object == kDoor) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.depth = 1;
		if (used(inventory, kOilCan))
			claim.anim = inventory.hasItem(kBlanket) ? "ANIM_P02_DOORBED_GONE" : kDoorOpen;
	} else if (object == "OBJ_P02_TO_P07") {
		if (inventory.hasItem(kBlanket))
			claim.build = Room::Claim::kSkip;
	}
}

void SheriffsOfficeRoom::enter(const Common::String &from) {
	// 0x416400, after the base entry: nothing under way; the level's snacks
	// (+0x24 = level + 2) before the way into the jail.
	Room &room = *_engine.getRoom();
	_doorLine = 0;
	_doorClicked = false;
	_atDoor = false;
	_sandwich = false;
	_eating = false;
	_lastScene.clear();
	_lastSound.clear();
	_door = findSprite(room, kDoor);
	_hotspot1 = findSprite(room, "OBJ_P02_SHERIFF_HOTSPOT1");
	_hotspot4 = findSprite(room, "OBJ_P02_SHERIFF_HOTSPOT4");
	_engine.getScript()->setSnacksNeeded(_engine.getDifficulty() + 2);
}

void SheriffsOfficeRoom::event(const Common::String &object, bool success) {
	// 0x416AA0.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	const char *scene = nullptr;
	if (object == kDoor) {
		// The oil can on it: used up, the arrow back. Either way Shaggy goes
		// to the door; input held.
		if (success) {
			Inventory &inventory = *_engine.getInventory();
			inventory.removeItem(kOilCan);
			inventory.setCursor("ANIM_CURSORARROW");
		}
		_doorClicked = true;
		return;
	}
	if (object == "OBJ_P02_SHERIFF_HOTSPOT1" && _hotspot1 >= 0) {
		// Moved on to its B or its E: a line.
		const Common::String state = script.getClickableState(_hotspot1);
		if (state == "ANIM_P02_SHERIFF_HOTSPOT1B")
			scene = "Sheriffs_Office.140X_";
		else if (state == "ANIM_P02_SHERIFF_HOTSPOT1E")
			scene = "Sheriffs_Office.140X3_";
	} else if (object == "OBJ_P02_SHERIFF_HOTSPOT4" && _hotspot4 >= 0) {
		// By the sequence it plays (+0x54): A and B a line; C, the sandwich
		// when it stops.
		const Common::String &anim = room.getAnim(_hotspot4);
		if (anim == "ANIM_P02_SHERIFF_HOTSPOT4A")
			scene = "Sheriffs_Office.HOTSPOT4A_";
		else if (anim == "ANIM_P02_SHERIFF_HOTSPOT4B")
			scene = "Sheriffs_Office.HOTSPOT4A2_";
		else if (anim == "ANIM_P02_SHERIFF_HOTSPOT4C")
			_sandwichWaiting = true;
	}
	if (scene && script.startScene(scene, true))
		_lastScene = scene;
}

bool SheriffsOfficeRoom::leaveBy(const Common::String &exitObject) {
	// 0x416470 (+0x24): the way into the jail waits for the level's snacks
	// (+0x88 against level + 2): a line by level (150A_, 150A2_, 150A3_), and
	// the snack box flashes (0x40D4D0). Else the default exit handler.
	RoomScript &script = *_engine.getScript();
	if (script.readExit(exitObject).destination != kJail)
		return false;
	const int level = _engine.getDifficulty();
	if (script.getMemory(kOffice).snacksGiven >= (uint)(level + 2))
		return false;
	static const char *const kLines[] = { "Sheriffs_Office.150A_", "Sheriffs_Office.150A2_", "Sheriffs_Office.150A3_" };
	if (level >= 0 && level <= 2 && script.startScene(kLines[level], true))
		_lastScene = kLines[level];
	_engine.getInventory()->flashSnackBox();
	debugC(1, kDebugScript, "Sheriff's office: not into the jail without %d snacks (%u given)", level + 2, script.getMemory(kOffice).snacksGiven);
	return true;
}

void SheriffsOfficeRoom::tick() {
	// 0x4166C0, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	Character *shaggy = script.getCharacter("SHAGGY");

	// Hotspot 4's C over: the sandwich.
	if (_sandwichWaiting && _hotspot4 >= 0 && room.getMode(_hotspot4) == 0) {
		_sandwichWaiting = false;
		_sandwich = true;
	}

	// The sandwich: Shaggy idle and free, he eats it; idle again, input free.
	if (_sandwich) {
		if (!_eating && isIdle(shaggy) && isFree(script)) {
			shaggy->special(room, sound, "OBJ_SHAGGY_P02_SANDWICH");
			playSound("Global.Idle.SHAGGY_S1_E");
			_eating = true;
		}
		if (_eating && isIdle(shaggy)) {
			_eating = false;
			_sandwich = false;
		}
	}

	if (!_doorClicked)
		return;
	// The door: Shaggy idle and free, he oils it (the door hidden while he
	// does), or shakes the bars without the oil can, or once the blanket is
	// taken.
	if (!_atDoor && isIdle(shaggy) && isFree(script)) {
		if (doorOiled(inventory)) {
			shaggy->special(room, sound, "OBJ_SHAGGY_P02_OIL_DOOR");
			playSound("Shaggy.Oils.Door");
			if (_door >= 0)
				room.setHidden(_door, true);
		} else {
			shaggy->special(room, sound, "OBJ_SHAGGY_P02_NO_OIL_DOOR");
			playSound("Shaggy.Shakes.Bars");
		}
		_atDoor = true;
	}
	if (!_atDoor || !isIdle(shaggy))
		return;
	// Idle again: the door open, input free; or a line in turn, not held.
	if (doorOiled(inventory)) {
		if (_door >= 0) {
			room.startAnim(_door, kDoorOpen, 1, -1);
			room.setHidden(_door, false);
		}
		debugC(1, kDebugScript, "Sheriff's office: the door open at tick %u", script.getTick());
	} else {
		static const char *const kLines[] = { "Sheriffs_Office.150A1_", "Sheriffs_Office.150A12_", "Sheriffs_Office.150A13_" };
		const int old = _doorLine++;
		if (old < 3 && script.startScene(kLines[old], false))
			_lastScene = kLines[old];
		if (old == 2)
			_doorLine = 0;
	}
	_atDoor = false;
	_doorClicked = false;
}

RoomClass *createOfficeRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kOffice)
		return new SheriffsOfficeRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
