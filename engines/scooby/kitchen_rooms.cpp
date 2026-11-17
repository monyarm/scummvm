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

// Phantom of the Knight's kitchen. The addresses are Phantom's.

#include "common/debug.h"

#include "scooby/character.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/kitchen_rooms.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kKitchen = "ROOM_P13_Kitchen";
static const char *const kGreaseSpot = "OBJ_P13_KITCHEN_HOTSPOT1";
static const char *const kScrewdriver = "OBJ_INV_SCREWDRIVER";
static const char *const kBottleFull = "OBJ_INV_BOTTLE_FULL";

static bool inSlot(ScoobyEngine &engine, const char *item) {
	// 0x4070C0: the item is in one of the toolbar's slots.
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (engine.getInventory()->getSlot(i) == item)
			return true;
	}
	return false;
}

int KitchenRoom::findSprite(const Common::String &object) const {
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void KitchenRoom::reset() {
	// 0x412CF0: no line, nothing pending, his lines from the first, EwanIntro
	// not had.
	_line.clear();
	_giving = false;
	_thanks = false;
	_lineIndex = 0;
	_introDone = false;
}

void KitchenRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x413740. The grease (hotspot 1, a requires object for the empty
	// bottle) only with the screwdriver ever had and the full bottle never:
	// under the first layer at depth 0, hit by its step polygon; else not
	// built. The way to the storage room isn't built until Ewan's quiz is
	// won (EwanPuzzle, game +0x254).
	Inventory &inventory = *_engine.getInventory();
	if (object == kGreaseSpot) {
		if (inventory.hasItem(kScrewdriver) && !inventory.hasItem(kBottleFull)) {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
			claim.depth = 0;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	} else if (object == "OBJ_P13_TO_P17" && !_engine.getGameFlag("EwanPuzzle")) {
		claim.build = Room::Claim::kSkip;
	}
}

void KitchenRoom::enter(const Common::String &from) {
	// 0x412EA0, after the base entry. From the dining room Daphne walks in
	// (UR, 0x405E60); from the storage room Daphne (UL, 0x405EE0) and Fred
	// (UR). His quiz won and his clue not given (0x40A870):
	// Kitchen.190B_, blocking, his clue (game vtable +0x44), and he gives it
	// when idle. Else, won: he stirs (STIRRING, state 0x10) without the
	// screwdriver or with the mirror used; with the screwdriver he sleeps
	// (SLEEPING, Kitchen.Ewan_Sleeping); either way at depth -1. Nothing
	// pending, and Ewan hit by his step polygon (+0x04 = 2).
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	_grease1 = findSprite(kGreaseSpot);
	_clicked = -1;
	auto walk = [&](const char *who, const char *direction) {
		Character *c = script.getCharacter(who);
		if (c)
			c->walkIn(room, sound, direction);
	};
	if (from == "ROOM_P12_Dining_Room") {
		walk("DAPHNE", "UR");
	} else if (from == "ROOM_P17_Kitchen_Storage") {
		walk("DAPHNE", "UL");
		walk("FRED", "UR");
	}
	const bool won = _engine.getGameFlag("EwanPuzzle");
	Character *ewan = script.getCharacter("EWAN");
	if (won && !_engine.isClueOwner("EWAN")) {
		script.startScene("Kitchen.190B_", true);
		_engine.giveClue("EWAN");
		_giving = true;
	} else if (won && ewan) {
		_sleeping = inventory.hasItem(kScrewdriver) && !(inventory.hasItem("OBJ_INV_MIRROR") && !inSlot(_engine, "OBJ_INV_MIRROR"));
		if (_sleeping) {
			ewan->actLoop(room, sound, "SLEEPING");
			sound.play("Kitchen.Ewan_Sleeping");
		} else {
			ewan->actLoop(room, sound, "STIRRING");
		}
		if (ewan->getSprite() >= 0)
			room.setDepth(ewan->getSprite(), -1);
	}
	_grease = false;
	_fetching = false;
	if (ewan && ewan->getSprite() >= 0)
		room.setHitType(ewan->getSprite(), 2);
	debugC(1, kDebugScript, "kitchen: from %s%s%s", from.c_str(), _giving ? "; Ewan's clue" : "", won && !_giving ? (_sleeping ? "; Ewan sleeps" : "; Ewan stirs") : "");
}

void KitchenRoom::roomLeft() {
	// 0x4138D0, after the base leave: left with the bottle at the grease,
	// the full bottle goes in the toolbar.
	Inventory &inventory = *_engine.getInventory();
	if (_grease && !inventory.hasItem(kBottleFull))
		inventory.addItem(kBottleFull, "ANIM_TOOLBARSPORTS_BOTTLE_FILLED", "ANIM_CURSORSPORTS_BOTTLE_FILLED");
	if (!_line.empty()) {
		_engine.getScript()->releaseSpeechLock();
		_line.clear();
	}
}

bool KitchenRoom::click(int sprite) {
	_clicked = sprite;
	return false;
}

void KitchenRoom::event(const Common::String &object, bool success) {
	// 0x4136C0. The grease with the empty bottle: Fred fills it (+0xC4);
	// with the arrow, Kitchen.190C_, blocking. Hotspot 3 scares Daphne.
	if (object == kGreaseSpot) {
		if (success)
			_grease = true;
		else
			_engine.getScript()->startScene("Kitchen.190C_", true);
	} else if (object == "OBJ_P13_KITCHEN_HOTSPOT3") {
		Character *daphne = _engine.getScript()->getCharacter("DAPHNE");
		if (daphne)
			daphne->setState(*_engine.getRoom(), *_engine.getSound(), Character::kStateScared);
	}
}

void KitchenRoom::exitMovie(const Common::String &destination, int &movie) {
	// 0x412D20: to the storage room with the screwdriver ever had, no movie.
	if (destination == "ROOM_P17_Kitchen_Storage" && _engine.getInventory()->hasItem(kScrewdriver))
		movie = -1;
}

void KitchenRoom::tick() {
	// 0x4130D0, after the base tick.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	const int clicked = _clicked;
	_clicked = -1;
	Character *ewan = script.getCharacter("EWAN");

	// Ewan clicked with the arrow (game +0x20 bit 4, his +0x08).
	if (ewan && clicked >= 0 && clicked == ewan->getSprite() && inventory.isDefaultCursor()) {
		if (!_engine.getGameFlag("EwanPuzzle")) {
			// His quiz not won: the first time he is met and introduces
			// himself on Big Head (Kitchen.190A_ on BIG_HEAD_BG_P13), on to
			// his quiz; after, straight to it (game vtable +0x04).
			if (!_introDone) {
				inventory.meetSuspect("EWAN");
				_engine.startBigHead("Kitchen.190A_", "BIG_HEAD_BG_P13", "ROOM_Ewan");
				_introDone = true;
			} else {
				_engine.requestRoom("ROOM_Ewan");
			}
		} else if (!_sleeping) {
			// Won and awake: his lines in turn (1010G_, 1010G1_, 1010G3_),
			// said as a sound with the speech lock (0x40A230).
			if (script.acquireSpeechLock()) {
				static const char *const kLines[] = { "Kitchen.1010G_1_EWAN", "Kitchen.1010G1_1_EWAN", "Kitchen.1010G3_1_EWAN" };
				_line = kLines[_lineIndex];
				_lineIndex = (_lineIndex + 1) % 3;
				sound.play(_line);
			}
		} else {
			// Asleep: 190F_, 190F1_, 190F2_ in turn, blocking.
			static const char *const kScenes[] = { "Kitchen.190F_", "Kitchen.190F1_", "Kitchen.190F2_" };
			script.startScene(kScenes[_lineIndex], true);
			_lineIndex = (_lineIndex + 1) % 3;
		}
	}
	// His line over (0x435A10): the speech lock free again.
	if (!_line.empty() && !sound.isClipPlaying(_line)) {
		_line.clear();
		script.releaseSpeechLock();
	}

	// His clue: once he is idle he GIVES, input held; idle again, the room's
	// exit handler (+0x24) to ROOM_Suspect_Chooser with Global.Thank_You_.
	bool clueStep = true;
	if (ewan) {
		if (ewan->getState() != Character::kStateIdle) {
			clueStep = false;
		} else {
			if (_giving) {
				ewan->act(room, sound, "GIVES");
				_giving = false;
				_thanks = true;
			}
			clueStep = ewan->getState() == Character::kStateIdle;
		}
	}
	if (clueStep && _thanks) {
		_thanks = false;
		RoomScript::Exit exit;
		exit.destination = "ROOM_Suspect_Chooser";
		exit.scene = "Global.Thank_You_";
		exit.movie = -1;
		script.takeExit(exit);
		debugC(1, kDebugScript, "kitchen: thanks, then the suspects and clues");
	}

	// The bottle at the grease: input held. Fred idle, no scene playing and
	// the opening done: he fetches it (OBJ_FRED_P13_GET_GREASE, state 0xD,
	// Kitchen.Get_Grease); the grease hidden, the empty bottle used, the
	// arrow back. Idle again: the full bottle, unless ever had, and
	// Kitchen.190G_, blocking.
	if (!_grease)
		return;
	Character *fred = script.getCharacter("FRED");
	if (!_fetching && fred && fred->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
		fred->special(room, sound, "OBJ_FRED_P13_GET_GREASE");
		sound.play("Kitchen.Get_Grease");
		if (_grease1 >= 0)
			room.setHidden(_grease1, true);
		inventory.removeItem("OBJ_INV_BOTTLE_EMPTY");
		inventory.setCursor("ANIM_CURSORARROW");
		_fetching = true;
		debugC(1, kDebugScript, "kitchen: Fred gets the grease");
	}
	if (_fetching && fred && fred->getState() == Character::kStateIdle) {
		_grease = false;
		_fetching = false;
		if (!inventory.hasItem(kBottleFull))
			inventory.addItem(kBottleFull, "ANIM_TOOLBARSPORTS_BOTTLE_FILLED", "ANIM_CURSORSPORTS_BOTTLE_FILLED");
		script.startScene("Kitchen.190G_", true);
		debugC(1, kDebugScript, "kitchen: the bottle is full");
	}
}

void KitchenRoom::saveKeys(Common::INIFile &profile) {
	// 0x413920: EwanIntro.
	profile.setKey("EwanIntro", kKitchen, _introDone ? "Yes" : "No");
}

void KitchenRoom::loadKeys(const Common::INIFile &profile) {
	// 0x413960 -> 0x40C560: only Yes reads as true.
	Common::String v;
	_introDone = profile.getKey("EwanIntro", kKitchen, v) && v == "Yes";
}

RoomClass *createKitchenRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kKitchen)
		return new KitchenRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
