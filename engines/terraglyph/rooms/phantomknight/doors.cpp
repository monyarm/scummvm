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

// Phantom of the Knight's room of doors and its Jester. The addresses are
// Phantom's.

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/rooms/phantomknight/doors.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kDoors = "ROOM_P11_Room_Of_Doors";
static const char *const kDuster = "OBJ_INV_DUSTER";

static bool inSlot(TerraglyphEngine &engine, const char *item) {
	// 0x4070C0: the item is in one of the toolbar's slots.
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (engine.getInventory()->getSlot(i) == item)
			return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// The Jester

void JesterControl::enter(TerraglyphEngine &engine) {
	// 0x412B80: the room's Jester, if it has one; nothing pending. Each ball
	// found and not yet taken he takes (TAKES_RED, _BLUE, _YELLOW, once each,
	// as RedBallTaken and so on); taking the first one found starts his talk
	// through the crest.
	_jester = engine.getScript()->getCharacter("JESTER");
	_scene.clear();
	_doneFlag = nullptr;
	_clicked = -1;
	if (!_jester)
		return;
	_talk = _d = _next = _e = _e5 = _e6 = _end = false;
	_count = 0;
	Room &room = *engine.getRoom();
	Sound &sound = *engine.getSound();
	const bool red = engine.getGameFlag("RedBallFound");
	const bool blue = engine.getGameFlag("BlueBallFound");
	const bool yellow = engine.getGameFlag("YellowBallFound");
	auto start = [&]() { _talk = _d = true; };
	bool blueBranch = false;
	if (!red || engine.getGameFlag("RedBallTaken")) {
		blueBranch = blue;
	} else {
		engine.setGameFlag("RedBallTaken", true);
		_jester->act(room, sound, "TAKES_RED");
		if (!blue) {
			if (!yellow)
				start();
		} else {
			blueBranch = true;
		}
	}
	if (blueBranch && !engine.getGameFlag("BlueBallTaken")) {
		engine.setGameFlag("BlueBallTaken", true);
		_jester->act(room, sound, "TAKES_BLUE");
		if (!red) {
			if (!yellow)
				start();
		}
	}
	if (yellow && !engine.getGameFlag("YellowBallTaken")) {
		engine.setGameFlag("YellowBallTaken", true);
		_jester->act(room, sound, "TAKES_YELLOW");
		if (!blue && !red)
			start();
	}
}

void JesterControl::startScene(TerraglyphEngine &engine, const Common::String &scene, bool *done) {
	// 0x40B400 on the room, holding input, with @p done as its done flag.
	if (engine.getScript()->startScene(scene, true) && done) {
		_scene = scene;
		_doneFlag = done;
	}
}

void JesterControl::say(TerraglyphEngine &engine) {
	// One of his lines (rand() % 5), holding input.
	static const char *const kLines[] = { "Global.1010J_", "Global.1010J1_", "Global.Jester1_", "Global.Jester2_", "Global.Jester3_" };
	startScene(engine, kLines[engine.getRandomNumber(4)], nullptr);
}

void JesterControl::tick(TerraglyphEngine &engine) {
	// 0x4122E0.
	if (!_jester)
		return;
	RoomScript &script = *engine.getScript();
	Room &room = *engine.getRoom();
	Sound &sound = *engine.getSound();
	Inventory &inventory = *engine.getInventory();
	const int clicked = _clicked;
	_clicked = -1;
	// A scene's done flag, set as it ends.
	if (_doneFlag && script.getScene() != _scene) {
		*_doneFlag = true;
		_doneFlag = nullptr;
		_scene.clear();
	}
	auto idle = [&]() { return _jester->getState() == Character::kStateIdle; };

	// All three balls found and his clue not yet given: Global.Jester.Clue_,
	// holding input, and his clue (game vtable +0x44); he gives it when idle.
	if (!engine.isClueOwner("JESTER") && idle() && engine.getGameFlag("BlueBallFound") && engine.getGameFlag("YellowBallFound") &&
	    engine.getGameFlag("RedBallFound")) {
		startScene(engine, "Global.Jester.Clue_", nullptr);
		engine.giveClue("JESTER");
		_giving = true;
		debugC(1, kDebugScript, "jester: his clue");
	}
	if (idle() && _giving) {
		_jester->act(room, sound, "GIVES");
		_giving = false;
		_thanks = true;
	}
	if (_thanks && idle()) {
		// Given and idle again: the gang thanks him and goes to the suspects
		// and clues, through the room's exit handler.
		_thanks = false;
		RoomScript::Exit exit;
		exit.destination = "ROOM_Suspect_Chooser";
		exit.scene = "Global.Thank_You_";
		exit.movie = -1;
		script.takeExit(exit);
		debugC(1, kDebugScript, "jester: thanks, then the suspects and clues");
	}

	// His talk through the crest: 220D<digit>_ for each of its four, then,
	// once the plaque has been seen, 220E_, 220E<digit>_ for each, 220E5_ and
	// 220E6_. Each holds input, and the next waits for its done flag, for
	// him idle and for no room change asked for.
	if (idle() && !engine.hasRequestedRoom() && _talk) {
		auto digit = [&]() { return (char)('0' + engine.getGameInt(Common::String::format("CrestSequence%d", _count))); };
		if (_d) {
			_d = false;
			if (_count == 4) {
				_count = 0;
				_next = true;
			} else {
				startScene(engine, Common::String::format("Souvenir_Shop_Room.220D%c_", digit()), &_d);
				_count++;
			}
		}
		if (!_next) {
			if (_e) {
				_e = false;
				if (_count == 4) {
					_count = 0;
					_e5 = true;
				} else {
					startScene(engine, Common::String::format("Souvenir_Shop_Room.220E%c_", digit()), &_e);
					_count++;
				}
			}
		} else if (!engine.getGameFlag("PlaqueSeen")) {
			_next = false;
			_talk = false;
		} else {
			_next = false;
			startScene(engine, "Souvenir_Shop_Room.220E_", &_e);
		}
		if (_e5) {
			_e5 = false;
			startScene(engine, "Souvenir_Shop_Room.220E5_", &_e6);
		} else if (_e6) {
			_e6 = false;
			startScene(engine, "Souvenir_Shop_Room.220E6_", &_end);
		} else if (_end) {
			_end = false;
			_talk = false;
		}
	}

	// Clicked with the arrow.
	if (clicked < 0 || clicked != _jester->getSprite() || !inventory.isDefaultCursor())
		return;
	const Common::String here = room.getName();
	if (!introDone && here == kDoors) {
		// The first time, in the room of doors: he is met, and introduces
		// himself on Big Head (220B_, BIG_HEAD_BG_P11), back here.
		if (!inventory.hasMetSuspect("JESTER"))
			inventory.meetSuspect("JESTER");
		introDone = true;
		engine.startBigHead("Souvenir_Shop_Room.220B_", "BIG_HEAD_BG_P11", kDoors);
		debugC(1, kDebugScript, "jester: his introduction");
		return;
	}
	if (!engine.getGameFlag("CrestPuzzle")) {
		// With a ball found, his talk through the crest again (back to idle,
		// state 6); else a line.
		if (engine.getGameFlag("RedBallFound") || engine.getGameFlag("BlueBallFound") || engine.getGameFlag("YellowBallFound")) {
			_talk = _d = true;
			_jester->setState(room, sound, Character::kStateIdle);
			return;
		}
		say(engine);
		return;
	}
	// The crest solved: without the full bottle ever, or with it still in a
	// slot, this room again through movie 54 (the royal gallery) or 57
	// (game vtable +0x08); else a line.
	if (!inventory.hasItem("OBJ_INV_BOTTLE_FULL") || inSlot(engine, "OBJ_INV_BOTTLE_FULL")) {
		engine.requestRoom(here, here == "ROOM_P07_Royal_Gallery" ? 54 : 57);
		return;
	}
	say(engine);
}

// ---------------------------------------------------------------------------
// The room of doors

bool RoomOfDoorsRoom::isDusterUsed(TerraglyphEngine &engine) {
	return engine.getInventory()->hasItem(kDuster) && !inSlot(engine, kDuster);
}

void RoomOfDoorsRoom::reset() {
	// 0x4157B0: Jane crying, the throne room's scene not had, the Jester not
	// introduced.
	_crying = true;
	_throneScene = false;
	_jesterIntro = false;
}

void RoomOfDoorsRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x415F80: the dining room's door, once the duster is used, a sprite
	// under the first layer at depth -1000; until then not built. The Jester
	// (OBJ_JESTER*) isn't built until the duster is used.
	if (object == "OBJ_P11_ROD_DINING_ROOM_DOOR") {
		if (isDusterUsed(_engine)) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = -1000;
			claim.hitType = 1;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	} else if (object.hasPrefix("OBJ_JESTER") && !isDusterUsed(_engine)) {
		claim.build = Room::Claim::kSkip;
	}
}

void RoomOfDoorsRoom::enter(const Common::String &from) {
	// 0x4157F0, after the base entry: from the dining room Velma walks in
	// (U); from the kitchen or the king's armory (or its slide) Fred (UL);
	// from the throne room Fred (UR); from the town Fred (DR). The door not
	// tried; the Jester's entry, with JesterIntro. While Jane cries, one of
	// her screams (rand() % 5: 1, 2, 3, or 5 for 3 and 4).
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	const char *who = nullptr;
	const char *direction = nullptr;
	if (from == "ROOM_P12_Dining_Room") {
		who = "VELMA";
		direction = "U";
	} else if (from == "ROOM_P13_Kitchen" || from == "ROOM_P20_Kings_Armory" || from == "ROOM_P20_Kings_Armory_Slide") {
		who = "FRED";
		direction = "UL";
	} else if (from == "ROOM_P10_Throne_Room") {
		who = "FRED";
		direction = "UR";
	} else if (from == "ROOM_P04_Town") {
		who = "FRED";
		direction = "DR";
	}
	if (who) {
		Character *c = script.getCharacter(who);
		if (c)
			c->walkIn(room, sound, direction);
	}
	_locked = false;
	_velma = false;
	_jester.enter(_engine);
	_jester.introDone = _jesterIntro;
	_scream.clear();
	if (_crying) {
		static const int kScreams[] = { 1, 2, 3, 5, 5 };
		_scream = Common::String::format("Room_of_Doors.Jane_Screaming%d", kScreams[_engine.getRandomNumber(4)]);
		sound.play(_scream);
	}
	debugC(1, kDebugScript, "room of doors: from %s; scream %s", from.c_str(), _scream.empty() ? "none" : _scream.c_str());
}

void RoomOfDoorsRoom::roomLeft() {
	// 0x4157D0, after the base leave: JesterIntro kept.
	_jesterIntro = _jester.introDone;
}

bool RoomOfDoorsRoom::click(int sprite) {
	_jester.click(sprite);
	return false;
}

void RoomOfDoorsRoom::tick() {
	// 0x415AB0, after the base tick: the Jester; then the dining room's door
	// tried: once Velma is idle, no scene plays and the opening is done, she
	// tries it (OBJ_VELMA_P11_DOOR_LOCKED, with Velma_Door_Locked); input is
	// held meanwhile; Velma idle again: Room_of_Doors.130B_, blocking, and
	// input back. 0x437540 restarts the tick clock, which the fork doesn't
	// keep.
	_jester.tick(_engine);
	if (!_locked)
		return;
	RoomScript &script = *_engine.getScript();
	Character *velma = script.getCharacter("VELMA");
	if (!_velma && velma && velma->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
		velma->special(*_engine.getRoom(), *_engine.getSound(), "OBJ_VELMA_P11_DOOR_LOCKED");
		_engine.getSound()->play("Room_Of_Doors.Velma_Door_Locked");
		_velma = true;
	}
	if (_velma && velma && velma->getState() == Character::kStateIdle) {
		script.startScene("Room_of_Doors.130B_", true);
		_velma = false;
		_locked = false;
	}
}

void RoomOfDoorsRoom::idle() {
	// 0x4159D0 (+0x34): once the opening has played and Jane has stopped
	// crying, rand() % 5: 1401_, 1402_, 140D_, 140D2_, or, with Velma idle,
	// her GLASSES and Global.1000A2_. None holds input.
	RoomScript &script = *_engine.getScript();
	if (!script.getMemory(kDoors).openingPlayed || _crying)
		return;
	static const char *const kScenes[] = { "Room_Of_Doors.1401_", "Room_Of_Doors.1402_", "Room_Of_Doors.140D_", "Room_Of_Doors.140D2_" };
	const uint r = _engine.getRandomNumber(4);
	if (r < 4) {
		script.startScene(kScenes[r], false);
		return;
	}
	Character *velma = script.getCharacter("VELMA");
	if (velma && velma->getState() == Character::kStateIdle) {
		velma->act(*_engine.getRoom(), *_engine.getSound(), "GLASSES");
		script.startScene("Global.1000A2_", false);
	}
}

bool RoomOfDoorsRoom::leaveBy(const Common::String &exitObject) {
	// 0x415C00: to the dining room before the duster is used, the door is
	// tried instead (+0xB8). An exit with no object (the Jester's thanks)
	// goes on.
	if (exitObject.empty())
		return false;
	const RoomScript::Exit exit = _engine.getScript()->readExit(exitObject);
	if (exit.destination != "ROOM_P12_Dining_Room" || isDusterUsed(_engine))
		return false;
	_locked = true;
	debugC(1, kDebugScript, "room of doors: the dining room's door is locked");
	return true;
}

void RoomOfDoorsRoom::leaving(const Common::String &destination, Common::String &scene) {
	// 0x415C00's scenes, then the default handler. To the king's armory:
	// 140A_, 140A1_ or 140A2_ (rand() % 3). To the throne room: Jane stops
	// crying and her screams stop; the first time 160D1_, after that 140B_,
	// 140B1_ or 140B2_ (rand() % 3). To the dining room (the duster used):
	// 140C_ or 140C1_ (rand() & 1).
	if (destination == "ROOM_P20_Kings_Armory") {
		static const char *const kScenes[] = { "Room_Of_Doors.140A_", "Room_Of_Doors.140A1_", "Room_Of_Doors.140A2_" };
		scene = kScenes[_engine.getRandomNumber(2)];
	} else if (destination == "ROOM_P10_Throne_Room") {
		_crying = false;
		static const int kScreams[] = { 1, 2, 3, 4, 5 };
		for (uint i = 0; i < ARRAYSIZE(kScreams); i++)
			_engine.getSound()->stopClip(Common::String::format("Room_of_Doors.Jane_Screaming%d", kScreams[i]));
		if (_throneScene) {
			static const char *const kScenes[] = { "Room_Of_Doors.140B_", "Room_Of_Doors.140B1_", "Room_Of_Doors.140B2_" };
			scene = kScenes[_engine.getRandomNumber(2)];
		} else {
			scene = "Room_of_Doors.160D1_";
			_throneScene = true;
		}
	} else if (destination == "ROOM_P12_Dining_Room") {
		scene = _engine.getRandomNumber(1) == 0 ? "Room_Of_Doors.140C_" : "Room_Of_Doors.140C1_";
	}
}

void RoomOfDoorsRoom::saveKeys(Common::INIFile &profile) {
	// 0x416140: Crying, ThroneScene, JesterIntro.
	profile.setKey("Crying", kDoors, _crying ? "Yes" : "No");
	profile.setKey("ThroneScene", kDoors, _throneScene ? "Yes" : "No");
	profile.setKey("JesterIntro", kDoors, _jesterIntro ? "Yes" : "No");
}

void RoomOfDoorsRoom::loadKeys(const Common::INIFile &profile) {
	// 0x4161C0 -> 0x40C560: only Yes reads as true; a missing key reads as
	// empty, so false.
	Common::String v;
	_crying = profile.getKey("Crying", kDoors, v) && v == "Yes";
	_throneScene = profile.getKey("ThroneScene", kDoors, v) && v == "Yes";
	_jesterIntro = profile.getKey("JesterIntro", kDoors, v) && v == "Yes";
}

RoomClass *createDoorsRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kDoors)
		return new RoomOfDoorsRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
