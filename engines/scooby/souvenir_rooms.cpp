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

// Phantom of the Knight's souvenir shop. The addresses are Phantom's.

#include "common/debug.h"

#include "scooby/character.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"
#include "scooby/souvenir_rooms.h"

namespace Scooby {

static const char *const kShop = "ROOM_P03_Souvenir_Shop";
static const char *const kBottle = "OBJ_INV_BOTTLE_EMPTY";
static const char *const kHotspot = "OBJ_P03_SOUVENIR_STORE_HOTSPOT1";

int SouvenirShopRoom::findSprite(const Common::String &object) const {
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void SouvenirShopRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x417170. The empty bottle, once ever taken, is a sprite under the
	// first layer at depth 0 showing ANIM_INV_BOTTLE_EMPTYB; until then a
	// requires object (0x408C80 with -1: the first layer). The hotspot is a
	// clickable as the default path builds it (0x408DC0 with 1, -1).
	if (object == kBottle) {
		if (_engine.getInventory()->hasItem(kBottle)) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = 0;
			claim.anim = "ANIM_INV_BOTTLE_EMPTYB";
			claim.hitType = 1;
		} else {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
			claim.depth = 0;
		}
	}
}

void SouvenirShopRoom::enter(const Common::String &from) {
	// 0x4172D0, after the base entry: nothing bought; from the town Fred
	// walks in (DL, 0x405F20). Without Jane screaming, the introduction
	// Souvenir_Shop.1201_ once (IntroDone). While she screams: one of her
	// screams (rand() % 5: 1, 2, 3, or 5 for 3 and 4), and 330A1_. Either
	// opening is unplayed, and +0x3C (0x40C7B0) runs again.
	RoomScript &script = *_engine.getScript();
	_bottle = _engine.getInventory()->hasItem(kBottle) ? -1 : findSprite(kBottle);
	_hotspot = findSprite(kHotspot);
	_bought = false;
	_fetching = false;
	_scream.clear();
	if (from == "ROOM_P04_Town") {
		Character *fred = script.getCharacter("FRED");
		if (fred)
			fred->walkIn(*_engine.getRoom(), *_engine.getSound(), "DL");
	}
	if (!_engine.getGameFlag("Screaming")) {
		if (!_introDone) {
			script.setOpening("Souvenir_Shop.1201_");
			_introDone = true;
			script.checkOpening();
		}
	} else {
		static const int kScreams[] = { 1, 2, 3, 5, 5 };
		_scream = Common::String::format("Souvenir_Shop.Jane_Screaming%d", kScreams[_engine.getRandomNumber(4)]);
		_engine.getSound()->play(_scream);
		script.setOpening("Souvenir_Shop.330A1_");
		script.checkOpening();
	}
	debugC(1, kDebugScript, "souvenir shop: from %s; opening '%s'; scream %s", from.c_str(), script.getOpening().c_str(),
	       _scream.empty() ? "none" : _scream.c_str());
}

void SouvenirShopRoom::event(const Common::String &object, bool success) {
	// 0x417060. The bottle with the tokens: they leave the toolbar, the
	// cursor is the arrow again, and the bottle is bought. With the arrow:
	// Souvenir_Shop.120A_, blocking. The hotspot: playing its B, 120X_;
	// else, its next state A, 120X1_; both blocking.
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	if (object == kBottle) {
		if (success) {
			inventory.removeItem("OBJ_INV_TOKENS");
			inventory.setCursor("ANIM_CURSORARROW");
			_bought = true;
			debugC(1, kDebugScript, "souvenir shop: the bottle bought");
		} else if (inventory.isDefaultCursor()) {
			script.startScene("Souvenir_Shop.120A_", true);
		}
	} else if (object == kHotspot && _hotspot >= 0) {
		if (_engine.getRoom()->getAnim(_hotspot) == "ANIM_P03_SOUVENIR_STORE_HOTSPOT1B")
			script.startScene("Souvenir_Shop.120X_", true);
		else if (script.getClickableState(_hotspot) == "ANIM_P03_SOUVENIR_STORE_HOTSPOT1A")
			script.startScene("Souvenir_Shop.120X1_", true);
	}
}

void SouvenirShopRoom::addBottle() {
	// 0x406DD0, unless the bottle was ever taken (0x407060).
	Inventory &inventory = *_engine.getInventory();
	if (!inventory.hasItem(kBottle))
		inventory.addItem(kBottle, "ANIM_TOOLBARSPORTS_BOTTLE_EMPTY", "ANIM_CURSORSPORTS_BOTTLE_EMPTY");
}

void SouvenirShopRoom::tick() {
	// 0x417400, after the base tick. Bought: once Fred is idle, no scene
	// plays and the opening is done, he fetches the bottle
	// (OBJ_FRED_P03_GET_WATER_BOTTLE, state 0xD) with
	// Souvenir_Shop.Fred_Takes_Bottle, and the shelf shows
	// ANIM_INV_BOTTLE_EMPTYB. Input is held and the cursor hidden meanwhile
	// (isBusy). Fred idle again: the bottle in the toolbar, and
	// Souvenir_Shop.120B_, blocking. 0x437540 restarts the game's tick
	// clock, which the fork doesn't keep.
	if (!_bought)
		return;
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Character *fred = script.getCharacter("FRED");
	if (!_fetching && fred && fred->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
		_fetching = true;
		fred->special(room, sound, "OBJ_FRED_P03_GET_WATER_BOTTLE");
		sound.play("Souvenir_Shop.Fred_Takes_Bottle");
		if (_bottle >= 0)
			room.startAnim(_bottle, "ANIM_INV_BOTTLE_EMPTYB", 1, -1);
		debugC(1, kDebugScript, "souvenir shop: Fred fetches the bottle");
	}
	if (_fetching && fred && fred->getState() == Character::kStateIdle) {
		_fetching = false;
		_bought = false;
		addBottle();
		script.startScene("Souvenir_Shop.120B_", true);
		debugC(1, kDebugScript, "souvenir shop: the bottle in the toolbar");
	}
}

void SouvenirShopRoom::roomLeft() {
	// 0x417580, after the base leave: left while bought, the bottle goes in
	// the toolbar.
	if (_bought)
		addBottle();
}

void SouvenirShopRoom::saveKeys(Common::INIFile &profile) {
	// 0x4175D0: IntroDone.
	profile.setKey("IntroDone", kShop, _introDone ? "Yes" : "No");
}

void SouvenirShopRoom::loadKeys(const Common::INIFile &profile) {
	// 0x417610 -> 0x40C560: only Yes reads as true.
	Common::String v;
	_introDone = profile.getKey("IntroDone", kShop, v) && v == "Yes";
}

RoomClass *createSouvenirRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kShop)
		return new SouvenirShopRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
