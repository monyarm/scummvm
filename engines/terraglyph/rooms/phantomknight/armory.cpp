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

// Phantom of the Knight's king's armory and its slide. The addresses are
// Phantom's.

#include "common/debug.h"

#include "terraglyph/rooms/phantomknight/armory.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kArmory = "ROOM_P20_Kings_Armory";
static const char *const kSlide = "ROOM_P20_Kings_Armory_Slide";
static const char *const kDuster = "OBJ_INV_DUSTER";
static const char *const kBottle = "OBJ_INV_BOTTLE_FULL";

static bool inSlot(TerraglyphEngine &engine, const char *item) {
	// 0x4070C0: the item is in one of the toolbar's slots.
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (engine.getInventory()->getSlot(i) == item)
			return true;
	}
	return false;
}

void KingsArmoryRoom::reset() {
	// 0x40EC20: the scratch's lines from the first, the intro not had.
	_scratchLine = 0;
	_introDone = false;
}

void KingsArmoryRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x40EF10: the duster, never had, a requires object (0x408C80 with
	// -1) the arrow answers; had, not built.
	if (object == kDuster)
		claim.build = _engine.getInventory()->hasItem(kDuster) ? Room::Claim::kSkip : Room::Claim::kRequires;
	if (claim.build == Room::Claim::kRequires) {
		claim.layer = 0;
		claim.depth = 0;
	}
}

void KingsArmoryRoom::enter(const Common::String &from) {
	// 0x40EC40. The armory with the full bottle used (ever had and in no
	// slot): the slide room (game vtable +0x04) instead, without the base
	// entry; the fork's base entry has run already. Else nothing pending,
	// the base entry, and the intro: without Jane screaming (game +0x288)
	// Kings_Armory.150_ once (IntroDone); screaming, one of her five
	// screams (rand() % 5) and Kings_Armory.Intro2_ or Intro_ (rand() & 1,
	// even for Intro2_). Either opening unplayed, then +0x3C (0x40C7B0).
	Inventory &inventory = *_engine.getInventory();
	RoomScript &script = *_engine.getScript();
	if (_name == kArmory && inventory.hasItem(kBottle) && !inSlot(_engine, kBottle)) {
		_engine.requestRoom(kSlide);
		debugC(1, kDebugScript, "king's armory: the bottle used, to the slide");
		return;
	}
	_duster = false;
	_taking = false;
	_scream.clear();
	_duster1 = -1;
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == kDuster)
			_duster1 = s;
	}
	if (!_engine.getGameFlag("Screaming")) {
		if (!_introDone) {
			script.setOpening("Kings_Armory.150_");
			_introDone = true;
			script.checkOpening();
		}
	} else {
		_scream = Common::String::format("Kings_Armory.Jane_Screaming%d", _engine.getRandomNumber(4) + 1);
		_engine.getSound()->play(_scream);
		script.setOpening(_engine.getRandomNumber(1) == 0 ? "Kings_Armory.Intro2_" : "Kings_Armory.Intro_");
		script.checkOpening();
	}
	debugC(1, kDebugScript, "king's armory: %s from %s; opening '%s'; scream %s", _name.c_str(), from.c_str(), script.getOpening().c_str(),
	       _scream.empty() ? "none" : _scream.c_str());
}

void KingsArmoryRoom::roomLeft() {
	// 0x40F150, after the base leave: left while the duster is being taken,
	// it goes in the toolbar all the same.
	Inventory &inventory = *_engine.getInventory();
	if (_duster && !inventory.hasItem(kDuster))
		inventory.addItem(kDuster, "ANIM_TOOLBARDUSTER", "ANIM_CURSORDUSTER");
}

void KingsArmoryRoom::event(const Common::String &object, bool success) {
	// 0x40EDD0. The duster: Scooby takes it. The full bottle on the scratch
	// or the armour: movie 7 to the slide room (game vtable +0x08), the
	// bottle used, the arrow back. The arrow on the scratch: 150C_, 150C1_,
	// 150C2_ in turn, blocking; on the armour, movie 44 back to this room.
	if (object == kDuster) {
		_duster = true;
		return;
	}
	const bool scratch = object == "OBJ_P20_KINGS_ARMORY_SCRATCH";
	if (!scratch && object != "OBJ_P20_KINGS_ARMORY_MAN")
		return;
	if (success) {
		_engine.requestRoom(kSlide, 7);
		Inventory &inventory = *_engine.getInventory();
		inventory.removeItem(kBottle);
		inventory.setCursor("ANIM_CURSORARROW");
		debugC(1, kDebugScript, "king's armory: the bottle on the %s", scratch ? "scratch" : "armour");
		return;
	}
	if (scratch) {
		static const char *const kLines[] = { "Kings_Armory.150C_", "Kings_Armory.150C1_", "Kings_Armory.150C2_" };
		_engine.getScript()->startScene(kLines[_scratchLine], true);
		_scratchLine = (_scratchLine + 1) % 3;
	} else {
		_engine.requestRoom(_name, 44);
	}
}

void KingsArmoryRoom::tick() {
	// 0x40EFE0, after the base tick. The duster being taken holds input:
	// Scooby idle, no scene playing and the opening done, he takes it
	// (OBJ_SCOOBY_P20_TAKES_DUSTER, state 0xD, with
	// Kings_Armory.Scooby_Takes_Duster), and it is hidden; idle again, it
	// goes in the toolbar unless ever had, and Kings_Armory.150B_,
	// blocking. 0x437540 restarts the tick clock, which the fork doesn't
	// keep.
	if (!_duster)
		return;
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Character *scooby = script.getCharacter("SCOOBY");
	if (!_taking && scooby && scooby->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
		_taking = true;
		scooby->special(room, sound, "OBJ_SCOOBY_P20_TAKES_DUSTER");
		sound.play("Kings_Armory.Scooby_Takes_Duster");
		if (_duster1 >= 0)
			room.setHidden(_duster1, true);
	}
	if (_taking && scooby && scooby->getState() == Character::kStateIdle) {
		_duster = false;
		_taking = false;
		Inventory &inventory = *_engine.getInventory();
		if (!inventory.hasItem(kDuster))
			inventory.addItem(kDuster, "ANIM_TOOLBARDUSTER", "ANIM_CURSORDUSTER");
		script.startScene("Kings_Armory.150B_", true);
		debugC(1, kDebugScript, "king's armory: Scooby has the duster");
	}
}

void KingsArmoryRoom::saveKeys(Common::INIFile &profile) {
	// 0x40F1A0: IntroDone, in this room's section.
	profile.setKey("IntroDone", _name, _introDone ? "Yes" : "No");
}

void KingsArmoryRoom::loadKeys(const Common::INIFile &profile) {
	// 0x40F1E0 -> 0x40C560: only Yes reads as true.
	Common::String v;
	_introDone = profile.getKey("IntroDone", _name, v) && v == "Yes";
}

RoomClass *createArmoryRoom(TerraglyphEngine &engine, const Common::String &room) {
	// The factory builds the class for both rooms (0x40EBC0).
	if (room == kArmory || room == kSlide)
		return new KingsArmoryRoom(engine, room);
	return nullptr;
}

} // End of namespace Terraglyph
