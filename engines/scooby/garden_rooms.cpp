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

// Phantom of the Knight's gardener's house, garden and garden plaque. The
// addresses are Phantom's.

#include "common/debug.h"

#include "scooby/character.h"
#include "scooby/detection.h"
#include "scooby/garden_rooms.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kHouse = "ROOM_P05_Gardeners_House";
static const char *const kGarden = "ROOM_P06_Garden";
static const char *const kPlaqueRoom = "ROOM_Garden_Plaque";
static const char *const kPlaque = "OBJ_P05_GARDENERS_HOUSE_PLACQUE";
static const char *const kDoor = "OBJ_P05_GARDENERS_HOUSE_DOOR";
static const char *const kTracks = "OBJ_P06_GARDEN_TRACKS";

// Game +0x287: the plaque was tried. A new game (0x42E880) and the plaque's
// entry clear it; a press sets it. The profile doesn't keep it.
static bool g_plaqueTried = false;

static int findSprite(Room &room, const Common::String &object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

static void walkIn(ScoobyEngine &engine, const char *who, const char *direction) {
	Character *c = engine.getScript()->getCharacter(who);
	if (c)
		c->walkIn(*engine.getRoom(), *engine.getSound(), direction);
}

// ---------------------------------------------------------------------------
// The gardener's house

void GardenersHouseRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x412110: with CrestPuzzle the plaque isn't built; without it, the way
	// to the garden isn't. The door a sprite under the first layer at depth
	// 0, stopped on its first step, or its last with CrestPuzzle (mode 5).
	const bool solved = _engine.getGameFlag("CrestPuzzle");
	if (object == kPlaque && solved) {
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_P05_TO_P06" && !solved) {
		claim.build = Room::Claim::kSkip;
	} else if (object == kDoor) {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
		claim.hitType = 1;
	}
}

void GardenersHouseRoom::arrive(const Common::String &from) {
	// 0x411C70 (+0x2C), in the base entry. Back from the plaque: with
	// CrestPuzzle, Gardeners_House.130C_; untried, no opening; tried and
	// wrong, 130B_, 130B1_, 130B2_ in turn. From elsewhere while Jane
	// screams: Intro2_ (rand() even) or Intro_. Each unplayed, then +0x3C.
	RoomScript &script = *_engine.getScript();
	const char *scene;
	if (from == kPlaqueRoom) {
		if (_engine.getGameFlag("CrestPuzzle")) {
			scene = "Gardeners_House.130C_";
		} else if (!GardenPlaqueRoom::wasTried()) {
			script.setOpening("");
			return;
		} else {
			static const char *const kLines[] = { "Gardeners_House.130B_", "Gardeners_House.130B1_", "Gardeners_House.130B2_" };
			scene = kLines[_returnLine];
			_returnLine = (_returnLine + 1) % 3;
		}
	} else {
		if (!_engine.getGameFlag("Screaming"))
			return;
		scene = _engine.getRandomNumber(1) == 0 ? "Gardeners_House.Intro2_" : "Gardeners_House.Intro_";
	}
	script.setOpening(scene);
	script.checkOpening();
}

void GardenersHouseRoom::enter(const Common::String &from) {
	// 0x412110's door, then 0x411D60 after the base entry. Back from the
	// plaque: with CrestPuzzle, Puzzle_Right, Door and the door opening
	// (mode 3); tried and wrong, Puzzle_Wrong. From the town Daphne walks in
	// (UR), from the garden Fred (UL). While Jane screams, one of her
	// screams (rand() % 5: 1, 2, 3, or 5 for 3 and 4).
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	_door = findSprite(room, kDoor);
	if (_door >= 0)
		room.setPlayer(_door, _engine.getGameFlag("CrestPuzzle") ? 5 : 0, -1, 3);
	if (from == kPlaqueRoom) {
		if (_engine.getGameFlag("CrestPuzzle")) {
			sound.play("Gardeners_House.Puzzle_Right");
			sound.play("Gardeners_House.Door");
			if (_door >= 0)
				room.setPlayer(_door, 3, -1, 3);
		} else if (GardenPlaqueRoom::wasTried()) {
			sound.play("Gardeners_House.Puzzle_Wrong");
		}
	}
	if (from == "ROOM_P04_Town")
		walkIn(_engine, "DAPHNE", "UR");
	if (from == kGarden)
		walkIn(_engine, "FRED", "UL");
	_scream.clear();
	if (_engine.getGameFlag("Screaming")) {
		static const int kScreams[] = { 1, 2, 3, 5, 5 };
		_scream = Common::String::format("Gardeners_House.Jane_Screaming%d", kScreams[_engine.getRandomNumber(4)]);
		sound.play(_scream);
	}
	debugC(1, kDebugScript, "gardener's house: from %s; opening '%s'; scream %s", from.c_str(), _engine.getScript()->getOpening().c_str(),
	       _scream.empty() ? "none" : _scream.c_str());
}

void GardenersHouseRoom::event(const Common::String &object, bool success) {
	// 0x411F10: the plaque clicked sets PlaqueSeen. With no ball found yet,
	// Big Head says 130A_, 130A1_, 130A2_ in turn on BIG_HEAD_BG_PLAQUE and
	// comes back here; with one, the plaque's puzzle.
	if (object != kPlaque)
		return;
	_engine.setGameFlag("PlaqueSeen", true);
	if (!_engine.getGameFlag("RedBallFound") && !_engine.getGameFlag("YellowBallFound") && !_engine.getGameFlag("BlueBallFound")) {
		static const char *const kLines[] = { "Gardeners_House.130A_", "Gardeners_House.130A1_", "Gardeners_House.130A2_" };
		const char *scene = kLines[_bigHeadLine];
		_bigHeadLine = (_bigHeadLine + 1) % 3;
		debugC(1, kDebugScript, "gardener's house: the plaque, Big Head %s", scene);
		_engine.startBigHead(scene, "BIG_HEAD_BG_PLAQUE", kHouse);
		return;
	}
	debugC(1, kDebugScript, "gardener's house: the plaque's puzzle");
	_engine.requestRoom(kPlaqueRoom);
}

// ---------------------------------------------------------------------------
// The garden

void GardenRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x411B50: the tracks aren't built until the maze has been entered.
	if (object == kTracks && !_engine.getGameFlag("MazeEntered"))
		claim.build = Room::Claim::kSkip;
}

void GardenRoom::enter(const Common::String &from) {
	// 0x411A80, after the base entry: the tracks' first line next; from the
	// gardener's house Velma walks in (U).
	_tracksLine = 0;
	if (from == kHouse)
		walkIn(_engine, "VELMA", "U");
}

void GardenRoom::event(const Common::String &object, bool success) {
	// 0x411AD0: the tracks say 230X_, 230X1_, 230X2_ in turn, blocking.
	if (object != kTracks)
		return;
	static const char *const kLines[] = { "Garden.230X_", "Garden.230X1_", "Garden.230X2_" };
	const int line = _tracksLine;
	_tracksLine = (_tracksLine + 1) % 3;
	_engine.getScript()->startScene(kLines[line], true);
}

// ---------------------------------------------------------------------------
// The garden plaque

const char *GardenPlaqueRoom::getButtonObject(uint i) {
	static const char *const kObjects[kButtons] = { "OBJ_GARDEN_PLAQUE_LION", "OBJ_GARDEN_PLAQUE_BUSH", "OBJ_GARDEN_PLAQUE_DEER", "OBJ_GARDEN_PLAQUE_DRAGON" };
	return kObjects[i];
}

int GardenPlaqueRoom::getButtonValue(uint i) {
	static const int kValues[kButtons] = { 3, 1, 2, 4 };
	return kValues[i];
}

bool GardenPlaqueRoom::wasTried() {
	return g_plaqueTried;
}

void GardenPlaqueRoom::reset() {
	g_plaqueTried = false;
}

void GardenPlaqueRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x40F940: each button a requires object (0x402D90) under the first
	// layer at depth 0, hit by its polygon, stopped on step 0, not pressed.
	for (uint i = 0; i < kButtons; i++) {
		if (object == getButtonObject(i)) {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
			claim.depth = 0;
			return;
		}
	}
}

void GardenPlaqueRoom::enter(const Common::String &from) {
	// 0x40F940's buttons, then 0x40F8A0 after the base entry: no presses,
	// all right so far, the toolbar hidden (isToolbarHidden), the plaque not
	// tried.
	Room &room = *_engine.getRoom();
	for (uint i = 0; i < kButtons; i++) {
		_buttons[i] = findSprite(room, getButtonObject(i));
		_pressed[i] = false;
		if (_buttons[i] >= 0)
			room.setPlayer(_buttons[i], 0, 0, 3);
	}
	_presses = 0;
	_allRight = true;
	_clicked = -1;
	g_plaqueTried = false;
}

bool GardenPlaqueRoom::click(int sprite) {
	_clicked = sprite;
	return false;
}

void GardenPlaqueRoom::tick() {
	// 0x40F8E0, after the base tick: each button not pressed shows step 1
	// under the mouse and step 0 otherwise; clicked without telling the
	// room, it counts as pressed.
	Room &room = *_engine.getRoom();
	for (uint i = 0; i < kButtons; i++) {
		if (_pressed[i] || _buttons[i] < 0)
			continue;
		const bool over = _engine.getHover() == _buttons[i];
		if (over && _clicked == _buttons[i]) {
			_pressed[i] = true;
			continue;
		}
		room.setPlayer(_buttons[i], 0, over ? 1 : 0, 3);
	}
	_clicked = -1;
}

void GardenPlaqueRoom::event(const Common::String &object, bool success) {
	// 0x40FB20: a button not yet pressed shows step 2 and counts; one that
	// isn't the crest's next (CrestSequence<presses>) spoils the try. The
	// plaque is tried, and Button_Click plays. After four: input held and
	// the cursor faded, CrestPuzzle if all were right, and back to the
	// gardener's house.
	Room &room = *_engine.getRoom();
	for (uint i = 0; i < kButtons; i++) {
		if (object != getButtonObject(i) || _pressed[i])
			continue;
		if (_engine.getGameInt(Common::String::format("CrestSequence%d", _presses)) != getButtonValue(i))
			_allRight = false;
		if (_buttons[i] >= 0)
			room.setPlayer(_buttons[i], 0, 2, 3);
		_pressed[i] = true;
		_presses++;
		g_plaqueTried = true;
		_engine.getSound()->play("Gardeners_House.Button_Click");
		debugC(1, kDebugScript, "garden plaque: %s pressed, %d of 4, all right %d", object.c_str(), _presses, _allRight ? 1 : 0);
		break;
	}
	if (_presses == (int)kButtons) {
		if (_allRight)
			_engine.setGameFlag("CrestPuzzle", true);
		debugC(1, kDebugScript, "garden plaque: done, %s", _allRight ? "right" : "wrong");
		_engine.requestRoom(kHouse);
	}
}

RoomClass *createGardenRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kHouse)
		return new GardenersHouseRoom(engine);
	if (room == kGarden)
		return new GardenRoom(engine);
	if (room == kPlaqueRoom)
		return new GardenPlaqueRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
