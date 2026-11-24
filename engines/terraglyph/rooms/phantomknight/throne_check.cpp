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

#include "terraglyph/formats/art_archive.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/class.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"
#include "terraglyph/rooms/phantomknight/throne.h"

namespace Terraglyph {

void TerraglyphEngine::checkThrone() {
	// Phantom's throne room (WP-062).
	const char *const kThroneRoom = "ROOM_P10_Throne_Room";
	const char *const kThrone = "OBJ_P10_THRONE_ROOM_THRONE";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kThroneRoom;
	if (!found) {
		debugC(3, kDebugScript, "throne check: no throne room");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kThroneRoom);
		debugC(3, kDebugScript, "throne check classes: Throne Room %s", dynamic_cast<ThroneRoomRoom *>(c) ? "throne room" : "-");
		delete c;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto throneRoom = [&]() { return dynamic_cast<ThroneRoomRoom *>(getRoomClass(kThroneRoom)); };
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
		const char *kind = r.isClickable(s) ? "clickable" : r.isRequiresObject(s) ? "requires" : "other";
		return Common::String::format("%s %s %s depth %d hit %d under %s hidden %d", kind, r.getObjectName(s).c_str(), r.getAnim(s).c_str(), r.getDepth(s),
		                              r.getHitType(s), p == 0 ? "root" : r.getLabel(p).c_str(), r.isHidden(s) ? 1 : 0);
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
	auto inSlot = [&](const char *item) {
		for (uint i = 0; i < Inventory::kSlotCount; i++) {
			if (_inventory->getSlot(i) == item)
				return 1;
		}
		return 0;
	};
	auto balls = [&]() {
		setGameFlag("RedBallFound", true);
		setGameFlag("BlueBallFound", true);
		setGameFlag("YellowBallFound", true);
	};
	// A fresh story; Jane's movie already played unless @p movie.
	auto story = [&](bool movieToCome) {
		newGameForCheck();
		throneRoom()->setJaneMoviePlayed(!movieToCome);
		_sound->stop();
	};

	// The placements: a new game; the ruby taken; the ruby and the balls;
	// the ruby used.
	{
		Common::String out;
		for (int k = 0; k < 4; k++) {
			story(false);
			if (k >= 1)
				_inventory->addItem("OBJ_INV_RUBY");
			if (k >= 2)
				balls();
			if (k == 3)
				_inventory->removeItem("OBJ_INV_RUBY");
			showRoom(kThroneRoom);
			static const char *const kCase[] = { "a new game", "the ruby taken", "and the balls found", "the ruby used" };
			const Common::String throne = sprite(find(kThrone));
			const Common::String jester = sprite(find("OBJ_JESTER"));
			out += Common::String::format("%s%s: throne %s, Jester %s", k ? "; " : "", kCase[k], throne.c_str(), jester.c_str());
		}
		const Common::String passage = sprite(find("OBJ_P10_TO_P19"));
		const Common::String doors = sprite(find("OBJ_P10_TO_P11"));
		out += Common::String::format("; to the secret passage %s; to the room of doors %s", passage.c_str(), doors.c_str());
		debugC(3, kDebugScript, "throne check placements: %s", out.c_str());
	}

	// Entry: the first time Jane's movie, then the walk-ins; the screaming
	// stops.
	{
		story(true);
		setGameFlag("Screaming", true);
		showRoom("ROOM_P11_Room_Of_Doors");
		showRoom(kThroneRoom);
		const Common::String who = walker();
		const int played = throneRoom()->isJaneMoviePlayed() ? 1 : 0;
		const Common::String screaming = getGameString("Screaming");
		tick1();
		Common::String out = Common::String::format("first, from P11_Room_Of_Doors: to %s movie %d, JaneMoviePlayed %d, Screaming %s, %s", next.c_str(), movie,
		                                            played, screaming.c_str(), who.c_str());
		static const char *const kFrom[] = { "ROOM_P11_Room_Of_Doors", "ROOM_P09_Arena1", "ROOM_P14_Arena2", "ROOM_P19_Secret_Passage2" };
		for (uint i = 0; i < ARRAYSIZE(kFrom); i++) {
			story(false);
			showRoom(kFrom[i]);
			showRoom(kThroneRoom);
			const Common::String w = walker();
			tick1();
			out += Common::String::format("; from %s: to '%s', %s", kFrom[i] + 5, next.c_str(), w.c_str());
		}
		debugC(3, kDebugScript, "throne check entry: %s", out.c_str());
	}

	// Jane and the duster: the arrow does nothing; the duster is taken.
	{
		story(false);
		_inventory->addItem("OBJ_INV_DUSTER");
		showRoom(kThroneRoom);
		_script->skip();
		ThroneRoomRoom *c = throneRoom();
		Character *jane = _script->getCharacter("JANE");
		c->click(jane ? jane->getSprite() : -1);
		tick1();
		Common::String out = Common::String::format("with the arrow: scene '%s', duster in a slot %d", _script->getScene().c_str(), inSlot("OBJ_INV_DUSTER"));
		_inventory->setCursor("ANIM_CURSORDUSTER");
		c->click(jane ? jane->getSprite() : -1);
		tick1();
		const Common::String scene = _script->getScene();
		const int held = _script->isBusy() ? 1 : 0;
		const int slot = inSlot("OBJ_INV_DUSTER");
		const int taken = _inventory->hasItem("OBJ_INV_DUSTER") ? 1 : 0;
		const int arrow = _inventory->isDefaultCursor() ? 1 : 0;
		out += Common::String::format("; with the duster: scene '%s' held %d, duster in a slot %d, ever taken %d, the arrow %d", scene.c_str(), held, slot, taken, arrow);
		debugC(3, kDebugScript, "throne check duster: %s", out.c_str());
	}

	// The third hotspot: on step 1 of its A, the gang is scared.
	{
		story(false);
		showRoom(kThroneRoom);
		_script->skip();
		const int h = find("OBJ_P10_THRONE_ROOM_HOTSPOT3");
		static const char *const kGang[] = { "DAPHNE", "SCOOBY", "SHAGGY", "VELMA", "FRED" };
		_script->clickClickable(h);
		int first = -1, last = -1, scared = 0;
		Common::String states;
		for (uint t = 1; t <= 60; t++) {
			tick1();
			if (_currentRoom->getStep(h) == 1 && _currentRoom->getAnim(h) == "ANIM_P10_THRONE_ROOM_HOTSPOT3A") {
				if (first < 0)
					first = t;
				last = t;
			}
			if (first == (int)t) {
				for (uint g = 0; g < ARRAYSIZE(kGang); g++) {
					Character *ch = _script->getCharacter(kGang[g]);
					const int st = ch ? ch->getState() : -1;
					scared += st == Character::kStateScared ? 1 : 0;
					states += Common::String::format("%s%s %d", g ? " " : "", kGang[g], st);
				}
			}
		}
		debugC(3, kDebugScript, "throne check hotspot: clicked at tick 0: step 1 of its A from tick %d to %d; then %s (scared %d of 5)", first, last,
		       states.empty() ? "-" : states.c_str(), scared);
	}

	// The throne: clicked without the balls; with them, the arrow and the
	// ruby.
	{
		story(false);
		_inventory->addItem("OBJ_INV_RUBY");
		showRoom(kThroneRoom);
		_script->skip();
		const int t = find(kThrone);
		_script->clickClickable(t);
		tick1();
		Common::String out = Common::String::format("no balls, clicked: scene '%s' held %d, to '%s', ruby in a slot %d", _script->getScene().c_str(),
		                                            _script->isBusy() ? 1 : 0, next.c_str(), inSlot("OBJ_INV_RUBY"));
		story(false);
		_inventory->addItem("OBJ_INV_RUBY");
		balls();
		showRoom(kThroneRoom);
		_script->skip();
		_script->sendEvent(kThrone, false);
		const Common::String arrowScene = _script->getScene();
		_script->skip();
		_inventory->setCursor("ANIM_CURSORRUBY");
		_script->sendEvent(kThrone, true);
		const int slot = inSlot("OBJ_INV_RUBY");
		const int arrow = _inventory->isDefaultCursor() ? 1 : 0;
		tick1();
		out += Common::String::format("; the balls found: the arrow: scene '%s'; the ruby: to %s movie %d, ruby in a slot %d, the arrow %d", arrowScene.c_str(),
		                              next.c_str(), movie, slot, arrow);
		showRoom(kThroneRoom);
		const Common::String after = sprite(find(kThrone));
		out += Common::String::format("; back: throne %s", after.c_str());
		debugC(3, kDebugScript, "throne check throne: %s", out.c_str());
	}

	// The profile.
	{
		story(true);
		ThroneRoomRoom *c = throneRoom();
		c->reset();
		Common::INIFile ini;
		c->saveKeys(ini);
		Common::String a, b;
		ini.getKey("JaneMoviePlayed", kThroneRoom, a);
		ini.getKey("Entrances", kThroneRoom, b);
		Common::String out = Common::String::format("a new game: JaneMoviePlayed=%s Entrances=%s", a.c_str(), b.c_str());
		showRoom(kThroneRoom);
		tick1();
		Common::INIFile ini2;
		c->saveKeys(ini2);
		ini2.getKey("JaneMoviePlayed", kThroneRoom, a);
		c->reset();
		c->loadKeys(ini2);
		out += Common::String::format("; after a visit: JaneMoviePlayed=%s; loaded: played %d", a.c_str(), c->isJaneMoviePlayed() ? 1 : 0);
		debugC(3, kDebugScript, "throne check profile: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Terraglyph
