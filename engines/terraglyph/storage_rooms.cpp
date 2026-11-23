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

// Phantom of the Knight's kitchen storage room. The addresses are Phantom's.

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"
#include "terraglyph/storage_rooms.h"

namespace Terraglyph {

static const char *const kStorage = "ROOM_P17_Kitchen_Storage";
static const char *const kDoorObject = "OBJ_P17_KITCHEN_STORAGE_DOOR";
static const char *const kScrewdriver = "OBJ_INV_SCREWDRIVER";
static const char *const kShield = "OBJ_INV_SHIELD";

int KitchenStorageRoom::findSprite(const Common::String &object) const {
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void KitchenStorageRoom::reset() {
	// 0x413AF0: the door untouched, the lines from the first.
	_doorClicked = 0;
	_doorLine = 0;
	_hotspot1Line = 0;
	_hotspot2Line = 0;
}

void KitchenStorageRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x414B30. The door, while not yet open (DoorClicked under 3), a
	// requires object (0x402D90) under the first layer at depth 0, hit by
	// its step polygon; its rollover the screwdriver once begun (enter()).
	// Open, not built. The screwdriver and the shield, never had, requires
	// objects (0x408C80 with -1) the arrow answers; had, not built. The
	// third hotspot a clickable as the default path builds it.
	Inventory &inventory = *_engine.getInventory();
	if (object == kDoorObject) {
		if (_doorClicked < 3) {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
			claim.depth = 0;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	} else if (object == kScrewdriver || object == kShield) {
		if (!inventory.hasItem(object)) {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
			claim.depth = 0;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	}
}

void KitchenStorageRoom::enter(const Common::String &from) {
	// 0x413A00, after the base entry. The door open (3), from elsewhere:
	// Kitchen_Storage.200A_, blocking. Back here through the first door
	// movie (1): Kitchen_Storage.200D_, blocking. Nothing pending. The door
	// begun: Fred stands 144 left and 13 up, at it.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	_door = findSprite(kDoorObject);
	_hotspot3 = findSprite("OBJ_P17_KITCHEN_STORAGE_HOTSPOT3");
	_screwdriverSprite = findSprite(kScrewdriver);
	_shieldSprite = findSprite(kShield);
	if (_door >= 0 && _doorClicked != 0)
		room.setRollover(_door, "ANIM_CURSORSCREWDRIVER");
	if (_doorClicked == 3 && from != kStorage)
		script.startScene("Kitchen_Storage.200A_", true);
	if (from == kStorage && _doorClicked == 1)
		script.startScene("Kitchen_Storage.200D_", true);
	_screwdriver = _fredTaking = _shield = _daphneTaking = false;
	if (_doorClicked > 0) {
		Character *fred = script.getCharacter("FRED");
		if (fred && fred->getSprite() >= 0) {
			const int s = fred->getSprite();
			room.setOffset(s, room.getOffsetX(s) - 144, room.getOffsetY(s) - 13);
		}
	}
	debugC(1, kDebugScript, "kitchen storage: from %s, DoorClicked %d", from.c_str(), _doorClicked);
}

void KitchenStorageRoom::roomLeft() {
	// 0x414DB0, after the base leave: left while either was being taken, it
	// goes in the toolbar all the same.
	Inventory &inventory = *_engine.getInventory();
	if (_screwdriver && !inventory.hasItem(kScrewdriver))
		inventory.addItem(kScrewdriver, "ANIM_TOOLBARSCREWDRIVER", "ANIM_CURSORSCREWDRIVER");
	if (_shield && !inventory.hasItem(kShield))
		inventory.addItem(kShield, "ANIM_TOOLBARSHIELD", "ANIM_CURSORSHIELD");
}

void KitchenStorageRoom::exitHere(int movie, bool fredTurns) {
	// The room's exit handler (+0x24) back to this room through @p movie.
	RoomScript::Exit exit;
	exit.destination = kStorage;
	exit.movie = movie;
	if (fredTurns)
		exit.directions[RoomScript::kFred] = "TURN_UL";
	_engine.getScript()->takeExit(exit);
}

void KitchenStorageRoom::event(const Common::String &object, bool success) {
	// 0x413EA0.
	RoomScript &script = *_engine.getScript();
	if (object == kShield) {
		_shield = true;
		return;
	}
	if (object == kScrewdriver) {
		_screwdriver = true;
		return;
	}
	if (object == "OBJ_P17_KITCHEN_STORAGE_HOTSPOT1") {
		static const char *const kLines[] = { "Kitchen_Storage.200XB_", "Kitchen_Storage.200XB1_", "Kitchen_Storage.200XB2_" };
		script.startScene(kLines[_hotspot1Line], true);
		_hotspot1Line = (_hotspot1Line + 1) % 3;
		return;
	}
	if (object == "OBJ_P17_KITCHEN_STORAGE_HOTSPOT2") {
		static const char *const kLines[] = { "Kitchen_Storage.200X_", "Kitchen_Storage.200X1_", "Kitchen_Storage.200X2_" };
		script.startScene(kLines[_hotspot2Line], true);
		_hotspot2Line = (_hotspot2Line + 1) % 3;
		return;
	}
	if (object == "OBJ_P17_KITCHEN_STORAGE_HOTSPOT3") {
		if (_hotspot3 >= 0 && _engine.getRoom()->getAnim(_hotspot3) == "ANIM_P17_KITCHEN_STORAGE_HOTSPOT3A")
			script.startScene("Kitchen_Storage.200XA_", true);
		return;
	}
	if (object != kDoorObject)
		return;
	// The door: begun, on with movie 63, then 64 (open); with the
	// screwdriver, begun with movie 62, Fred turning, the screwdriver used
	// and the arrow back, the door's rollover the screwdriver. With the
	// arrow, movie 65 with Fred turning, then 200B1_, 200B2_, in turn.
	if (_doorClicked == 1) {
		_doorClicked = 2;
		exitHere(63, false);
		return;
	}
	if (_doorClicked == 2) {
		_doorClicked = 3;
		exitHere(64, false);
		return;
	}
	if (success) {
		_doorClicked++;
		exitHere(62, true);
		Inventory &inventory = *_engine.getInventory();
		inventory.removeItem(kScrewdriver);
		inventory.setCursor("ANIM_CURSORARROW");
		if (_door >= 0)
			_engine.getRoom()->setRollover(_door, "ANIM_CURSORSCREWDRIVER");
		debugC(1, kDebugScript, "kitchen storage: the screwdriver in the door");
		return;
	}
	if (_doorLine == 0) {
		_doorLine = 1;
		exitHere(65, true);
	} else if (_doorLine == 1) {
		_doorLine = 2;
		script.startScene("Kitchen_Storage.200B1_", true);
	} else {
		_doorLine = 0;
		script.startScene("Kitchen_Storage.200B2_", true);
	}
}

void KitchenStorageRoom::tick() {
	// 0x413B20, after the base tick. Each taking holds input. Fred idle, no
	// scene playing and the opening done: he takes the screwdriver
	// (OBJ_FRED_P17_TAKE_SCREWDRIVER, state 0xD, with
	// Kitchen_Storage.Fred_Takes_Screwdriver), hidden from the room; on his
	// step 33 it goes in the toolbar unless ever had; idle again,
	// Kitchen_Storage.200C_, blocking. Daphne likewise takes the shield
	// (OBJ_DAPHNE_P17_TAKES_SHIELD, Kitchen_Storage.Daphne_Takes_Shield);
	// idle again it goes in the toolbar unless had, and 200G_ if Brad has
	// been met, else 400A_. 0x437540 restarts the tick clock, which the
	// fork doesn't keep.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	if (_screwdriver) {
		Character *fred = script.getCharacter("FRED");
		if (!_fredTaking && fred && fred->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
			fred->special(room, sound, "OBJ_FRED_P17_TAKE_SCREWDRIVER");
			sound.play("Kitchen_Storage.Fred_Takes_Screwdriver");
			if (_screwdriverSprite >= 0)
				room.setHidden(_screwdriverSprite, true);
			_fredTaking = true;
		}
		if (_fredTaking && fred && fred->getSprite() >= 0 && room.getStep(fred->getSprite()) == 33 && !inventory.hasItem(kScrewdriver))
			inventory.addItem(kScrewdriver, "ANIM_TOOLBARSCREWDRIVER", "ANIM_CURSORSCREWDRIVER");
		if (_fredTaking && fred && fred->getState() == Character::kStateIdle) {
			_screwdriver = false;
			_fredTaking = false;
			script.startScene("Kitchen_Storage.200C_", true);
			debugC(1, kDebugScript, "kitchen storage: Fred has the screwdriver");
		}
	}
	if (_shield) {
		Character *daphne = script.getCharacter("DAPHNE");
		if (!_daphneTaking && daphne && daphne->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
			daphne->special(room, sound, "OBJ_DAPHNE_P17_TAKES_SHIELD");
			sound.play("Kitchen_Storage.Daphne_Takes_Shield");
			if (_shieldSprite >= 0)
				room.setHidden(_shieldSprite, true);
			_daphneTaking = true;
		}
		if (_daphneTaking && daphne && daphne->getState() == Character::kStateIdle) {
			_shield = false;
			_daphneTaking = false;
			if (!inventory.hasItem(kShield))
				inventory.addItem(kShield, "ANIM_TOOLBARSHIELD", "ANIM_CURSORSHIELD");
			script.startScene(inventory.hasMetSuspect("BRAD") ? "Kitchen_Storage.200G_" : "Kitchen_Storage.400A_", true);
			debugC(1, kDebugScript, "kitchen storage: Daphne has the shield");
		}
	}
}

void KitchenStorageRoom::saveKeys(Common::INIFile &profile) {
	// 0x414E40: DoorClicked.
	profile.setKey("DoorClicked", kStorage, Common::String::format("%d", _doorClicked));
}

void KitchenStorageRoom::loadKeys(const Common::INIFile &profile) {
	// 0x414E80 -> 0x40C670.
	Common::String v;
	if (profile.getKey("DoorClicked", kStorage, v))
		_doorClicked = atoi(v.c_str());
}

RoomClass *createStorageRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kStorage)
		return new KitchenStorageRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
