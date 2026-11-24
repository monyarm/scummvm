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
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"
#include "terraglyph/rooms/showdown/store.h"

namespace Terraglyph {

static const char *const kFront = "ROOM_P10_Front_Of_General_Store";
static const char *const kStore = "ROOM_P15_General_Store_Interior";
static const char *const kArrow = "ANIM_CURSORARROW";
static const char *const kClock = "OBJ_INV_CLOCK";
static const char *const kNoFace = "OBJ_P10_FRONT_OF_GS_CLOCK_NOFACE";
static const char *const kHammer = "OBJ_INV_HAMMER";
static const char *const kSnake = "OBJ_P15_GSI_SNAKE";
static const char *const kLantern = "OBJ_INV_LANTERN";
static const char *const kBlanket = "OBJ_INV_BLANKET";
static const char *const kSnakeIdle = "ANIM_P15_GSI_SNAKEIDLE";
static const char *const kSnakeBlanket = "ANIM_P15_GSI_SNAKEBLANKET";
static const char *const kIdleSound = "General_Store_Interior.Snake_Idle";
static const char *const kBlanketScene = "General_Store_Interior.Click_Blanket_On_Snake_";

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

// ---------------------------------------------------------------------------
// The front of the store

void StoreFrontRoom::reset() {
	// 0x40F740, after the base reset.
	_clockTaken = false;
	_clockLine = 0;
}

void StoreFrontRoom::prepare() {
	// 0x40F890 (+0x30): while MoaningActive, 110A_ unplayed; otherwise no
	// opening, its played flag as it was (0x40C6B0).
	RoomScript &script = *_engine.getScript();
	if (_engine.getGameFlag("MoaningActive")) {
		script.setOpening("Front_of_General_Store.110A_");
		return;
	}
	RoomScript::Memory &memory = script.getMemory(kFront);
	const bool played = memory.openingPlayed;
	script.setOpening("");
	memory.openingPlayed = played;
}

void StoreFrontRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x40FA40. The clock is built as the default path builds a requires
	// object (0x408C80 with -1), so it is left to it. The clock without its
	// face a sprite under layer 1 (game +0x5C) at depth 1.
	if (object == kNoFace) {
		claim.build = Room::Claim::kSprite;
		claim.layer = 1;
		claim.depth = 1;
	}
}

void StoreFrontRoom::enter(const Common::String &from) {
	// 0x40FB70, after the base entry. The hammer used: the broken clock in
	// the toolbar and off the wall; otherwise the faceless clock hidden.
	// Fred walks in down and left from Street 2.
	Room &room = *_engine.getRoom();
	Inventory &inventory = *_engine.getInventory();
	_lastScene.clear();
	_clock = findSprite(room, kClock);
	_noFace = findSprite(room, kNoFace);
	if (used(inventory, kHammer)) {
		if (!inventory.hasItem(kClock))
			inventory.addItem(kClock, "ANIM_TOOLBARBROKEN_CLOCK", "ANIM_CURSORBROKEN_CLOCK");
		if (_clock >= 0)
			room.setHidden(_clock, true);
	} else if (_noFace >= 0) {
		room.setHidden(_noFace, true);
	}
	if (from == "ROOM_P04_Street2") {
		if (Character *fred = _engine.getScript()->getCharacter("FRED"))
			fred->walkIn(room, *_engine.getSound(), "DL");
	}
	_scoobyWaiting = false;
}

void StoreFrontRoom::event(const Common::String &object, bool success) {
	// 0x40F8D0. Every scene here holds input.
	RoomScript &script = *_engine.getScript();
	const char *scene = nullptr;
	if (object == kClock) {
		if (success) {
			// The hammer: Take_Clock_, the hammer used up, the arrow back;
			// the tick goes on when the scene is over.
			scene = "Front_of_General_Store.Take_Clock_";
			_clockTaken = true;
			Inventory &inventory = *_engine.getInventory();
			inventory.removeItem(kHammer);
			inventory.setCursor(kArrow);
		} else {
			static const char *const kLines[] = { "Front_of_General_Store.A_Clock_", "Front_of_General_Store.A_Clockb_",
			                                      "Front_of_General_Store.A_Clockb3_" };
			scene = kLines[_clockLine];
			_clockLine = (_clockLine + 1) % 3;
		}
	} else if (object == "OBJ_P10_FRONT_OF_GS_HOTSPOT2") {
		// The door: rand() & 1.
		scene = _engine.getRandomNumber(1) == 0 ? "Front_of_General_Store.Door_Locked_" : "Front_of_General_Store.Door_Lockedb_";
	} else if (object == "OBJ_P10_FRONT_OF_GS_HOTSPOT3") {
		scene = "Front_of_General_Store.110X_";
	} else if (object == "OBJ_P10_FRONT_OF_GS_HOTSPOT1") {
		_scoobyWaiting = true;
	}
	if (scene && script.startScene(scene, true))
		_lastScene = scene;
}

void StoreFrontRoom::tick() {
	// 0x40F760, after the base tick.
	RoomScript &script = *_engine.getScript();
	// The clock broken, no scene, the opening done: back here through
	// movie 2 (game vtable +0x08).
	if (_clockTaken && isFree(script)) {
		_clockTaken = false;
		debugC(1, kDebugScript, "store front: the clock, through movie 2, at tick %u", script.getTick());
		_engine.requestRoom(kFront, 2);
	}
	// Hotspot 1: Scooby is scared once neither of its sounds is playing in
	// its first half second (0x434BA0, 0x434CE0).
	if (_scoobyWaiting) {
		static const char *const kSounds[] = { "SFX_P10_FRONT_OF_GS_HOTSPOT1A", "SFX_P10_FRONT_OF_GS_HOTSPOT1B" };
		for (uint i = 0; i < ARRAYSIZE(kSounds); i++) {
			const int32 ms = _engine.getSound()->getClipMillis(kSounds[i]);
			if (ms >= 0 && ms <= 500)
				return;
		}
		if (Character *scooby = script.getCharacter("SCOOBY"))
			scooby->setState(*_engine.getRoom(), *_engine.getSound(), Character::kStateScared);
		_scoobyWaiting = false;
		debugC(1, kDebugScript, "store front: Scooby scared at tick %u", script.getTick());
	}
}

// ---------------------------------------------------------------------------
// The store

void GeneralStoreRoom::reset() {
	// 0x412630, after the base reset.
	_hotspot1 = 0;
}

void GeneralStoreRoom::playSound(const char *name) {
	const bool found = _engine.getSound()->play(name);
	_lastSound = Common::String::format("%s found %d", name, found ? 1 : 0);
}

void GeneralStoreRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x412BD0. The snake a requires object, as the default path builds it:
	// under the blanket once the blanket is used, with its idle sound at
	// 93% (0x434B60) and at depth 500; otherwise on its idle, stopped (the
	// tick starts it). The lantern a requires object (0x408C80 with -1)
	// until taken.
	Inventory &inventory = *_engine.getInventory();
	if (object == kSnake) {
		claim.build = Room::Claim::kRequires;
		_covered = used(inventory, kBlanket);
		if (_covered) {
			playSound(kIdleSound);
			_engine.getSound()->setFilePercent(kIdleSound, 93);
			claim.anim = kSnakeBlanket;
			claim.depth = 500;
		} else {
			claim.anim = kSnakeIdle;
		}
	} else if (object == kLantern) {
		if (inventory.hasItem(kLantern)) {
			claim.build = Room::Claim::kSkip;
		} else {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
		}
	}
}

void GeneralStoreRoom::enter(const Common::String &from) {
	// 0x412DF0, after the base entry: nothing under way; Shaggy walks in
	// down and left from the back alley.
	Room &room = *_engine.getRoom();
	_snake = findSprite(room, kSnake);
	_lantern = findSprite(room, kLantern);
	if (_snake >= 0 && !_covered)
		room.startAnim(_snake, kSnakeIdle, 0, -1);
	_snakeLine = 0;
	_strike = false;
	_blanketDone = false;
	_tossing = false;
	_lanternClicked = false;
	_grabbing = false;
	_blanketScene = false;
	_lastScene.clear();
	if (from == "ROOM_P19_Back_Alley") {
		if (Character *shaggy = _engine.getScript()->getCharacter("SHAGGY"))
			shaggy->walkIn(room, *_engine.getSound(), "DL");
	}
}

void GeneralStoreRoom::roomLeft() {
	// 0x4129B0, after the base leave: the lantern clicked, it is taken.
	Inventory &inventory = *_engine.getInventory();
	if (_lanternClicked && !inventory.hasItem(kLantern))
		inventory.addItem(kLantern, "ANIM_TOOLBARLANTERN", "ANIM_CURSORLANTERN");
}

void GeneralStoreRoom::event(const Common::String &object, bool success) {
	// 0x412A00. Every scene here holds input.
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	const char *scene = nullptr;
	bool snakeLine = false;
	if (object == kSnake) {
		if (success) {
			// The blanket: used up, the arrow back, and its scene; Fred tosses
			// it when the scene is over.
			inventory.removeItem(kBlanket);
			inventory.setCursor(kArrow);
			if (script.startScene(kBlanketScene, true)) {
				_blanketScene = true;
				_lastScene = kBlanketScene;
			}
			return;
		}
		snakeLine = true;
	} else if (object == kLantern && inventory.isDefaultCursor()) {
		// With the snake under the blanket Fred takes it; otherwise the
		// snake has a line, and strikes.
		if (used(inventory, kBlanket)) {
			_lanternClicked = true;
			return;
		}
		snakeLine = true;
	} else if (object == "OBJ_P15_GSI_HOTSPOT1") {
		const int old = _hotspot1++;
		if (old == 0) {
			_hotspot1 = 0;
			scene = "General_Store_Interior.120X_";
		}
	} else if (object == "OBJ_P15_GSI_HOTSPOT2") {
		scene = "General_Store_Interior.120X2_";
	}
	if (snakeLine) {
		// Three lines in turn, and the snake strikes on its next step 0.
		static const char *const kLines[] = { "General_Store_Interior.Click_On_Snake_", "General_Store_Interior.Click_On_Snakeb_",
		                                       "General_Store_Interior.Click_On_Snakec_" };
		const int old = _snakeLine++;
		if (old < 3)
			scene = kLines[old];
		if (old == 2)
			_snakeLine = 0;
		_strike = true;
	}
	if (scene && script.startScene(scene, true))
		_lastScene = scene;
}

void GeneralStoreRoom::tick() {
	// 0x412650, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	Character *fred = script.getCharacter("FRED");

	// The blanket scene's done flag, set by the base tick as it ends.
	if (_blanketScene && script.getScene() != kBlanketScene) {
		_blanketScene = false;
		_blanketDone = true;
	}

	// The snake stopped: its idle sound, and its idle, looping.
	if (_snake >= 0 && room.getMode(_snake) == 0) {
		playSound(kIdleSound);
		room.startAnim(_snake, kSnakeIdle, 1, -1);
	}

	// The blanket: Fred tosses it over the snake; idle again, the snake is
	// under it, at depth 500, its idle sound at 93%, and input is free.
	if (_blanketDone) {
		if (!_tossing && isIdle(fred) && isFree(script)) {
			_tossing = true;
			fred->special(room, sound, "OBJ_FRED_P15_TOSS_BLANKET");
		}
		if (_tossing && isIdle(fred)) {
			sound.setFilePercent(kIdleSound, 93);
			if (_snake >= 0) {
				room.startAnim(_snake, kSnakeBlanket, 1, -1);
				room.setDepth(_snake, 500);
			}
			_covered = true;
			_blanketDone = false;
			_tossing = false;
			debugC(1, kDebugScript, "store: the snake under the blanket at tick %u", script.getTick());
		}
	}

	// The lantern: Fred takes it; on his step 62 it is in the toolbar; idle
	// again, Click_On_Lantern_.
	if (_lanternClicked) {
		if (!_grabbing && isIdle(fred) && isFree(script)) {
			_grabbing = true;
			if (_lantern >= 0)
				room.setHidden(_lantern, true);
			playSound("Fred_Takes_Lantern");
			fred->special(room, sound, "OBJ_FRED_P15_GRAB_LANTERN");
		}
		if (_grabbing && fred) {
			if (room.getStep(fred->getSprite()) == 62 && !inventory.hasItem(kLantern))
				inventory.addItem(kLantern, "ANIM_TOOLBARLANTERN", "ANIM_CURSORLANTERN");
			if (isIdle(fred)) {
				_lanternClicked = false;
				_grabbing = false;
				if (script.startScene("General_Store_Interior.Click_On_Lantern_", true))
					_lastScene = "General_Store_Interior.Click_On_Lantern_";
			}
		}
	}

	// A strike waiting: on the snake's step 0, its idle sound stops and it
	// strikes once.
	if (_strike && _snake >= 0 && room.getStep(_snake) == 0) {
		_strike = false;
		sound.stopClip(kIdleSound);
		playSound("General_Store_Interior.Snake_Attack");
		room.startAnim(_snake, "ANIM_P15_GSI_SNAKEANIM", 3, -1);
		debugC(1, kDebugScript, "store: the snake strikes at tick %u", script.getTick());
	}
}

RoomClass *createStoreRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kFront)
		return new StoreFrontRoom(engine);
	if (room == kStore)
		return new GeneralStoreRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
