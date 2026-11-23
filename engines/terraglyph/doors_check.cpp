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

#include "common/config-manager.h"
#include "common/debug.h"

#include "terraglyph/art_archive.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/doors_rooms.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkDoors() {
	// Phantom's room of doors and its Jester (WP-061), and Phantom's clues.
	const char *const kDoors = "ROOM_P11_Room_Of_Doors";
	const char *const kTown = "ROOM_P04_Town";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kDoors;
	if (!found) {
		debugC(3, kDebugScript, "doors check: no room of doors");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kDoors);
		debugC(3, kDebugScript, "doors check classes: Room of Doors %s", dynamic_cast<RoomOfDoorsRoom *>(c) ? "room of doors" : "-");
		delete c;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto doors = [&]() { return dynamic_cast<RoomOfDoorsRoom *>(getRoomClass(kDoors)); };
	auto find = [&](const char *prefix) {
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s).hasPrefix(prefix))
				return (int)s;
		}
		return -1;
	};
	auto sprite = [&](int s) {
		if (s < 0)
			return Common::String("none");
		Room &r = *_currentRoom;
		const int p = r.getParent(s);
		return Common::String::format("%s %s depth %d hit %d under %s", r.getObjectName(s).c_str(), r.getAnim(s).c_str(), r.getDepth(s), r.getHitType(s),
		                              p == 0 ? "root" : r.getLabel(p).c_str());
	};
	auto walker = [&]() {
		static const char *const kGang[] = { "FRED", "VELMA", "DAPHNE", "SHAGGY", "SCOOBY" };
		Common::String who;
		for (uint g = 0; g < ARRAYSIZE(kGang); g++) {
			Character *ch = _script->getCharacter(kGang[g]);
			if (ch && ch->getState() == Character::kStateEndWalk)
				who += Common::String::format("%s%s depth %d", who.empty() ? "" : " ", kGang[g], _currentRoom->getDepth(ch->getSprite()));
		}
		return who.empty() ? Common::String("nobody") : who;
	};
	auto useDuster = [&]() {
		_inventory->addItem("OBJ_INV_DUSTER");
		_inventory->removeItem("OBJ_INV_DUSTER");
	};
	// A fresh story; the duster used or not.
	auto story = [&](bool duster) {
		newGameForCheck();
		setGameFlag("Screaming", false);
		for (uint i = 0; i < 4; i++)
			setGameInt(Common::String::format("CrestSequence%u", i), (int)(i == 0 ? 2 : i == 1 ? 4 : i == 2 ? 1 : 3));
		if (duster)
			useDuster();
		_sound->stop();
	};

	// Phantom's clues: the Jester's own, and as the villain a red herring.
	{
		Common::String out;
		bool own = true, herring = true;
		for (uint k = 0; k < 50; k++) {
			story(false);
			setGameString("Whodunit", "MARIA");
			giveClue("JESTER");
			const Common::String c = getGameString("CluesFound0");
			own = own && (c == "JESTER_COUPON" || c == "JESTER_SIGN" || c == "JESTER_BOOK");
			story(false);
			setGameString("Whodunit", "JESTER");
			giveClue("JESTER");
			const Common::String h = getGameString("CluesFound0");
			herring = herring && h.hasPrefix("RH_") && !h.hasPrefix("RH_JESTER");
		}
		debugC(3, kDebugScript, "doors check clues: the Jester's own of his three %d; as the villain, a red herring not his %d", own ? 1 : 0, herring ? 1 : 0);
	}

	// The placements, without and with the duster used; the walk-ins.
	{
		story(false);
		showRoom(kDoors);
		Common::String out = Common::String::format("duster not used: dining room's door %s, Jester %s", sprite(find("OBJ_P11_ROD_DINING_ROOM_DOOR")).c_str(),
		                                            sprite(find("OBJ_JESTER")).c_str());
		story(true);
		showRoom(kDoors);
		out += Common::String::format("; used: dining room's door %s, Jester %s", sprite(find("OBJ_P11_ROD_DINING_ROOM_DOOR")).c_str(), sprite(find("OBJ_JESTER")).c_str());
		static const char *const kFrom[] = { "ROOM_P12_Dining_Room", "ROOM_P13_Kitchen", "ROOM_P20_Kings_Armory", "ROOM_P20_Kings_Armory_Slide",
		                                     "ROOM_P10_Throne_Room", kTown, "ROOM_P31_KST" };
		for (uint i = 0; i < ARRAYSIZE(kFrom); i++) {
			story(false);
			showRoom(kFrom[i]);
			showRoom(kDoors);
			out += Common::String::format("; from %s: %s", kFrom[i] + 5, walker().c_str());
		}
		debugC(3, kDebugScript, "doors check placements: %s", out.c_str());
	}

	// Jane's crying, the ways out and their scenes.
	{
		story(false);
		showRoom(kDoors);
		RoomOfDoorsRoom *c = doors();
		Common::String out = Common::String::format("crying %d, scream %s", c->isCrying() ? 1 : 0, c->getScream().empty() ? "none" : "one");
		auto way = [&](const char *dest) {
			Common::String scene = "-";
			c->leaving(dest, scene);
			return scene;
		};
		uint armory = 0;
		for (uint k = 0; k < 60; k++) {
			const Common::String s = way("ROOM_P20_Kings_Armory");
			armory |= s == "Room_Of_Doors.140A_" ? 1 : s == "Room_Of_Doors.140A1_" ? 2 : s == "Room_Of_Doors.140A2_" ? 4 : 8;
		}
		out += Common::String::format("; to the armory: 140A_ %d, 140A1_ %d, 140A2_ %d, other %d", armory & 1 ? 1 : 0, armory & 2 ? 1 : 0, armory & 4 ? 1 : 0, armory & 8 ? 1 : 0);
		const Common::String toThrone = way("ROOM_P10_Throne_Room");
		out += Common::String::format("; to the throne room: '%s', crying %d, ThroneScene %d", toThrone.c_str(), c->isCrying() ? 1 : 0,
		                              c->isThroneScene() ? 1 : 0);
		uint throne = 0;
		for (uint k = 0; k < 60; k++) {
			const Common::String s = way("ROOM_P10_Throne_Room");
			throne |= s == "Room_Of_Doors.140B_" ? 1 : s == "Room_Of_Doors.140B1_" ? 2 : s == "Room_Of_Doors.140B2_" ? 4 : 8;
		}
		out += Common::String::format("; again: 140B_ %d, 140B1_ %d, 140B2_ %d, other %d", throne & 1 ? 1 : 0, throne & 2 ? 1 : 0, throne & 4 ? 1 : 0, throne & 8 ? 1 : 0);
		uint dining = 0;
		for (uint k = 0; k < 40; k++) {
			const Common::String s = way("ROOM_P12_Dining_Room");
			dining |= s == "Room_Of_Doors.140C_" ? 1 : s == "Room_Of_Doors.140C1_" ? 2 : 8;
		}
		out += Common::String::format("; to the dining room: 140C_ %d, 140C1_ %d, other %d; to the town '%s'", dining & 1 ? 1 : 0, dining & 2 ? 1 : 0,
		                              dining & 8 ? 1 : 0, way(kTown).c_str());
		showRoom(kTown);
		showRoom(kDoors);
		out += Common::String::format("; back: scream %s", c->getScream().empty() ? "none" : "one");
		debugC(3, kDebugScript, "doors check ways: %s", out.c_str());
	}

	// The dining room's door, locked: Velma tries it.
	{
		story(false);
		showRoom(kDoors);
		_script->skip();
		RoomOfDoorsRoom *c = doors();
		Common::String obj;
		for (uint s = 0; s < _currentRoom->getSpriteCount() && obj.empty(); s++) {
			if (_currentRoom->getDestination(s) == "ROOM_P12_Dining_Room")
				obj = _currentRoom->getObjectName(s);
		}
		_script->takeExit(_script->readExit(obj));
		Common::String out = Common::String::format("the door: locked %d, busy %d", c->isLocked() ? 1 : 0, c->isBusy() ? 1 : 0);
		Character *velma = _script->getCharacter("VELMA");
		int tries = -1, after = -1;
		Common::String anim;
		for (uint t = 1; t <= 400 && after < 0; t++) {
			tick1();
			if (tries < 0 && velma->getState() == Character::kStateSpecial) {
				tries = t;
				anim = _currentRoom->getAnim(velma->getSprite());
			}
			if (tries >= 0 && !c->isLocked())
				after = t;
		}
		out += Common::String::format("; Velma tries it at tick %d (%s, Velma_Door_Locked playing %d); at tick %d scene '%s' held %d, locked %d", tries, anim.c_str(),
		                              _sound->isClipPlaying("Room_Of_Doors.Velma_Door_Locked") ? 1 : 0, after, _script->getScene().c_str(), _script->isBusy() ? 1 : 0,
		                              c->isLocked() ? 1 : 0);
		debugC(3, kDebugScript, "doors check locked: %s", out.c_str());
	}

	// The idle chatter: none while Jane cries; then each of five.
	{
		story(false);
		showRoom(kDoors);
		_script->skip();
		RoomOfDoorsRoom *c = doors();
		c->idle();
		Common::String out = Common::String::format("crying: scene '%s'", _script->getScene().c_str());
		Common::String dummy;
		c->leaving("ROOM_P10_Throne_Room", dummy);
		_script->getMemory(kDoors).openingPlayed = true;
		uint seen = 0;
		for (uint k = 0; k < 100; k++) {
			_script->skip();
			Character *velma = _script->getCharacter("VELMA");
			velma->setState(*_currentRoom, *_sound, Character::kStateIdle);
			c->idle();
			const Common::String s = _script->getScene();
			static const char *const kScenes[] = { "Room_Of_Doors.1401_", "Room_Of_Doors.1402_", "Room_Of_Doors.140D_", "Room_Of_Doors.140D2_", "Global.1000A2_" };
			for (uint i = 0; i < 5; i++) {
				if (s == kScenes[i])
					seen |= 1 << i;
			}
			if (s == "Global.1000A2_" && velma->getState() != Character::kStateAct)
				seen |= 1 << 5;
		}
		out += Common::String::format("; after: 1401_ %d, 1402_ %d, 140D_ %d, 140D2_ %d, 1000A2_ %d (Velma's GLASSES missing %d)", seen & 1 ? 1 : 0, seen & 2 ? 1 : 0,
		                              seen & 4 ? 1 : 0, seen & 8 ? 1 : 0, seen & 16 ? 1 : 0, seen & 32 ? 1 : 0);
		debugC(3, kDebugScript, "doors check idle: %s", out.c_str());
	}

	// The Jester: introduced on a click, then lines; the balls he takes and
	// his talk through the crest; his clue; the crest solved.
	{
		auto jester = [&]() { return _script->getCharacter("JESTER"); };
		// Run ticks, skipping each scene as it starts; the scenes in order.
		auto scenes = [&](uint maxTicks) {
			Common::String out;
			Common::String last;
			for (uint t = 0; t < maxTicks; t++) {
				tick1();
				const Common::String s = _script->getScene();
				if (!s.empty() && s != last) {
					out += Common::String::format("%s%s", out.empty() ? "" : " ", s.c_str());
					last = s;
					_script->skip();
				} else if (s.empty()) {
					last.clear();
				}
				if (!next.empty())
					break;
			}
			return out.empty() ? Common::String("none") : out;
		};
		story(true);
		showRoom(kDoors);
		_script->skip();
		RoomOfDoorsRoom *c = doors();
		Character *j = jester();
		c->click(j->getSprite());
		tick1();
		Common::String out = Common::String::format("first click: to %s, '%s' on %s back to %s, met %d", next.c_str(), getBigHeadScene().c_str(),
		                                            getBigHeadBackground().c_str(), getBigHeadNext().c_str(), _inventory->hasMetSuspect("JESTER") ? 1 : 0);
		showRoom(kTown);
		showRoom(kDoors);
		_script->skip();
		c->click(jester()->getSprite());
		tick1();
		const Common::String line = _script->getScene();
		const bool isLine = line == "Global.1010J_" || line == "Global.1010J1_" || line == "Global.Jester1_" || line == "Global.Jester2_" || line == "Global.Jester3_";
		out += Common::String::format("; JesterIntro %d; next click: one of his five lines %d", c->isJesterIntro() ? 1 : 0, isLine ? 1 : 0);
		debugC(3, kDebugScript, "doors check jester: %s", out.c_str());

		// The red ball found: taken, then the talk; without and with the
		// plaque seen.
		out.clear();
		for (int seen = 0; seen < 2; seen++) {
			story(true);
			setGameFlag("RedBallFound", true);
			setGameFlag("PlaqueSeen", seen != 0);
			showRoom(kDoors);
			_script->skip();
			const Common::String taken = getGameString("RedBallTaken");
			const Common::String anim = _currentRoom->getAnim(jester()->getSprite());
			const int state = jester()->getState();
			const Common::String talk = scenes(4000);
			out += Common::String::format("%splaque seen %d: RedBallTaken %s, Jester %s (state %d); scenes %s", seen ? "; " : "", seen, taken.c_str(), anim.c_str(),
			                              state, talk.c_str());
		}
		debugC(3, kDebugScript, "doors check talk: %s", out.c_str());

		// All three: the clue, then thanks and the suspects and clues.
		story(true);
		setGameString("Whodunit", "MARIA");
		setGameFlag("RedBallFound", true);
		setGameFlag("RedBallTaken", true);
		setGameFlag("BlueBallFound", true);
		setGameFlag("BlueBallTaken", true);
		setGameFlag("YellowBallFound", true);
		setGameFlag("YellowBallTaken", true);
		showRoom(kDoors);
		_script->skip();
		const Common::String clue = scenes(4000);
		out = Common::String::format("scenes %s; owner %d; then to %s", clue.c_str(), isClueOwner("JESTER") ? 1 : 0, next.c_str());
		// The crest solved, no full bottle: this room through movie 57.
		story(true);
		setGameFlag("CrestPuzzle", true);
		c->setJesterIntroForCheck(true);
		showRoom(kDoors);
		_script->skip();
		c->click(jester()->getSprite());
		tick1();
		out += Common::String::format("; crest solved: to %s movie %d", next.c_str(), movie);
		debugC(3, kDebugScript, "doors check clue: %s", out.c_str());
	}

	// The profile.
	{
		story(false);
		RoomOfDoorsRoom *c = doors();
		Common::INIFile ini;
		c->saveKeys(ini);
		Common::String a, b, d;
		ini.getKey("Crying", kDoors, a);
		ini.getKey("ThroneScene", kDoors, b);
		ini.getKey("JesterIntro", kDoors, d);
		debugC(3, kDebugScript, "doors check profile: a new game: Crying=%s ThroneScene=%s JesterIntro=%s", a.c_str(), b.c_str(), d.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Terraglyph
