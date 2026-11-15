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

// The script level 3 checks of each room's setup and of Town Center
// (WP-029), driven through the engine with the sound mixer's clips.

#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/room_setup.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"
#include "scooby/story_rooms.h"

namespace Scooby {

void ScoobyEngine::checkRoomSetup() {
	// Every room, entered from nowhere with fresh state: the music its entry
	// plays from a fresh start, its opening, whether input is held for it,
	// and the tick the opening starts. Then the music's variants in turn.
	_checking = true;
	const uint kMaxTicks = 400;
	Common::String next;
	int movie;
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String name = _art->getRoomName(i);
		// Not from the ghost spot, which gives a room a random opening.
		if (_currentRoom->getName() == "ROOM_Ghost_Spot")
			showRoom(_art->getRoomName(i == 0 ? 1 : 0));
		delete _music;
		_music = new Music(*_sound);
		_sound->stop();
		_script->resetMemories();
		_requestedRoom.clear();
		_currentRoom->setCursor("");
		_roomClass = nullptr;
		_script->setHooks(nullptr);
		if (!showRoom(name))
			continue;
		const Common::String music = _sound->isClipPlaying(_music->getClip()) ? _music->getClip() : Common::String("-");
		const bool held = _script->isBusy();
		const Common::String opening = _script->getOpening();
		int starts = -1;
		for (uint t = 0; t < kMaxTicks && starts < 0; t++) {
			next.clear();
			if (tickOnce(next, movie))
				break;
			if (_script->isScenePlaying())
				starts = t;
		}
		debugC(3, kDebugScript, "setup check %s: music %s, opening '%s', held %d, starts %d", name.c_str(), music.c_str(),
		       opening.c_str(), held ? 1 : 0, opening.empty() ? -1 : starts);
		_sound->stop();
	}

	// A group's variants in turn, as the group changes and as its clip ends.
	Common::String groupRoom, otherRoom;
	for (uint i = 0; i < _art->getRoomCount() && (groupRoom.empty() || otherRoom.empty()); i++) {
		const RoomSetup s = getRoomSetup(getGameId(), _art->getRoomName(i));
		const int g = atoi(s.music);
		if (g >= 1 && groupRoom.empty())
			groupRoom = s.room;
		else if (g >= 1 && atoi(getRoomSetup(getGameId(), groupRoom).music) != g && otherRoom.empty())
			otherRoom = s.room;
	}
	delete _music;
	_music = new Music(*_sound);
	Common::String turns;
	for (int k = 0; k < 4 && !otherRoom.empty(); k++) {
		_script->resetMemories();
		showRoom(groupRoom);
		turns += _music->getClip() + " ";
		_script->resetMemories();
		showRoom(otherRoom);
	}
	debugC(3, kDebugScript, "setup check music: %s and %s in turn: %s", groupRoom.c_str(), otherRoom.c_str(), turns.c_str());
	_script->resetMemories();
	showRoom(groupRoom);
	Common::String ends = _music->getClip();
	for (int k = 0; k < 4; k++) {
		_sound->stopClip(_music->getClip());
		_music->tick();
		ends += " " + _music->getClip();
	}
	debugC(3, kDebugScript, "setup check music: %s as each clip ends: %s", groupRoom.c_str(), ends.c_str());
	_sound->stop();
	delete _music;
	_music = new Music(*_sound);
	_script->resetMemories();
	_checking = false;
	debugC(3, kDebugScript, "setup check: done");
}

void ScoobyEngine::checkTownCenter() {
	// Showdown's Town Center from a new game, every step against the trace.
	const Common::String kRoom = "ROOM_P01_Town_Center";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kRoom;
	if (!found)
		return;
	_checking = true;
	const uint kMaxTicks = 3000;
	struct Fresh {
		ScoobyEngine &e;
		void room(const Common::String &name) {
			RoomClass *c;
			if (e._roomClasses.tryGetVal(name, c)) {
				if (e._roomClass == c)
					e._roomClass = nullptr;
				e._script->setHooks(nullptr);
				delete c;
				e._roomClasses.erase(name);
			}
		}
	} fresh = { *this };
	// Run until @p done, or a room change; the ticks run, or -1.
	struct Run {
		ScoobyEngine &e;
		Common::String next;
		int movie;
		int until(bool (*done)(ScoobyEngine &), uint max) {
			for (uint t = 1; t <= max; t++) {
				next.clear();
				if (e.tickOnce(next, movie))
					return -1;
				if (done(e))
					return t;
			}
			return -1;
		}
	} run = { *this, Common::String(), -1 };
	struct Is {
		static bool noScene(ScoobyEngine &e) { return !e._script->isScenePlaying(); }
		static bool scene(ScoobyEngine &e) { return e._script->isScenePlaying(); }
		static bool idle(ScoobyEngine &e) { return !e._script->isScenePlaying() && !e._roomClass->isBusy(); }
	};
	TownCenterRoom *tc = nullptr;
	Character *c;
	// Scenes run at the voice clips' own pace, not the ticks', so the check
	// names each scene as it starts and skips it.
	struct Skip {
		ScoobyEngine &e;
		Common::String scene() {
			const Common::String s = e._script->isScenePlaying() ? e._script->getScene() : Common::String("-");
			e._script->skip();
			return s;
		}
	} skip = { *this };
	const char *const kGang3[] = { "DAPHNE", "SCOOBY", "SHAGGY" };

	// 1. From Street 1 after a new game: Fred walks in; the opening holds
	// input, then plays.
	fresh.room(kRoom);
	_script->resetMemories();
	_inventory->reset();
	setGameFlag("MoaningActive", true);
	showRoom("ROOM_P03_Street1");
	showRoom(kRoom);
	tc = static_cast<TownCenterRoom *>(_roomClass);
	c = _script->getCharacter("FRED");
	debugC(3, kDebugScript, "town check entry from Street1: FRED state %d %s, opening '%s', held %d", c ? c->getState() : -1,
	       c ? _currentRoom->getAnim(c->getSprite()).c_str() : "-", _script->getOpening().c_str(), _script->isBusy() ? 1 : 0);
	int t = run.until(Is::scene, kMaxTicks);
	Common::String scene = skip.scene();
	run.until(Is::noScene, 1);
	debugC(3, kDebugScript, "town check opening: %s starts after %d ticks; skipped, held %d, played %d", scene.c_str(), t,
	       _script->isBusy() ? 1 : 0, _script->getMemory(kRoom).openingPlayed ? 1 : 0);

	// 2. The exits before a snack: three scenes in turn, the snack box
	// flashes, the room stays.
	int exitSprite = -1;
	for (uint s = 0; s < _currentRoom->getSpriteCount() && exitSprite < 0; s++) {
		if (!_currentRoom->getDestination(s).empty())
			exitSprite = s;
	}
	for (int k = 1; k <= 4; k++) {
		_inventory->takeLog();
		clickExit(exitSprite);
		scene = skip.scene();
		run.next.clear();
		run.until(Is::scene, 120);
		const Common::String log = _inventory->takeLog();
		debugC(3, kDebugScript, "town check exit %d by %s: scene %s, then %s; %s", k, _currentRoom->getObjectName(exitSprite).c_str(),
		       scene.c_str(), run.next.empty() ? "stays" : run.next.c_str(), log.c_str());
	}

	// Points the live test can click: the first grid point each hits.
	struct Point {
		static Common::String of(ScoobyEngine &e, int sprite) {
			for (int y = 4; y < 480 && sprite >= 0; y += 8) {
				for (int x = 4; x < 640; x += 8) {
					if (e._currentRoom->hitTest(x, y) == sprite)
						return Common::String::format("%d,%d", x, y);
				}
			}
			return "-";
		}
	};
	c = _script->getCharacter("SCOOBY");
	int hangingAt = -1;
	for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
		if (_currentRoom->getObjectName(s) == "OBJ_P01_TC_HOTSPOT4")
			hangingAt = s;
	}
	debugC(3, kDebugScript, "town check points: exit %s, scooby %s, snack box %s, hanging sign %s", Point::of(*this, exitSprite).c_str(),
	       Point::of(*this, c ? c->getSprite() : -1).c_str(), Point::of(*this, _inventory->getSnackBoxSprite()).c_str(),
	       Point::of(*this, hangingAt).c_str());

	// 3. A snack for Scooby: the count, the line, and the exit lets the gang go.
	_inventory->setSnacks(5);
	_inventory->click(_inventory->getSnackBoxSprite());
	run.until(Is::noScene, 1);
	c = _script->getCharacter("SCOOBY");
	_inventory->click(c ? c->getSprite() : -1);
	run.until(Is::noScene, 1);
	scene = skip.scene();
	const Common::String fed = _inventory->takeLog();
	debugC(3, kDebugScript, "town check snack: %sline %s; snacks given %u", fed.c_str(), scene.c_str(),
	       _script->getMemory(kRoom).snacksGiven);
	run.until(Is::noScene, 60);
	clickExit(exitSprite);
	skip.scene();
	run.next.clear();
	t = run.until(Is::scene, kMaxTicks);
	debugC(3, kDebugScript, "town check exit after the snack: goes to %s", t < 0 && !run.next.empty() ? run.next.c_str() : "nowhere");

	// 4. The hanging sign, the sign, and Scooby taking it.
	_script->resetMemories();
	_script->getMemory(kRoom).openingPlayed = true;
	showRoom(kRoom);
	tc = static_cast<TownCenterRoom *>(_roomClass);
	int hanging = -1, sign = -1, hotspot2 = -1;
	for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
		if (_currentRoom->getObjectName(s) == "OBJ_P01_TC_HOTSPOT4")
			hanging = s;
		if (_currentRoom->getObjectName(s) == "OBJ_INV_SIGN")
			sign = s;
		if (_currentRoom->getObjectName(s) == "OBJ_P01_TC_HOTSPOT2")
			hotspot2 = s;
	}
	debugC(3, kDebugScript, "town check sign: hanging sign %s depth %d hidden %d; sign %s depth %d hidden %d",
	       hanging >= 0 ? (_currentRoom->isClickable(hanging) ? "clickable" : "?") : "missing", hanging >= 0 ? _currentRoom->getDepth(hanging) : 0,
	       hanging >= 0 ? _currentRoom->isHidden(hanging) : -1, sign >= 0 ? (_currentRoom->isRequiresObject(sign) ? "requires" : "?") : "missing",
	       sign >= 0 ? _currentRoom->getDepth(sign) : 0, sign >= 0 ? _currentRoom->isHidden(sign) : -1);
	_script->clickClickable(hanging);
	t = run.until(Is::scene, kMaxTicks);
	const bool shows = sign >= 0 && !_currentRoom->isHidden(sign);
	scene = skip.scene();
	debugC(3, kDebugScript, "town check sign: clicked %d; after %d ticks the sign shows %d, hanging hidden %d, scene %s",
	       tc->isSignClicked() ? 1 : 0, t, shows ? 1 : 0, hanging >= 0 && _currentRoom->isHidden(hanging) ? 1 : 0, scene.c_str());
	run.until(Is::noScene, 1);
	debugC(3, kDebugScript, "town check points: sign %s", Point::of(*this, sign).c_str());
	_inventory->takeLog();
	_inventory->click(sign);
	run.until(Is::noScene, 1);
	const bool held = tc->isBusy();
	struct Taking {
		static bool started(ScoobyEngine &e) {
			Character *s = e._script->getCharacter("SCOOBY");
			return s && e._currentRoom->isHidden(s->getSprite());
		}
	};
	const int start = run.until(Taking::started, kMaxTicks);
	t = run.until(Is::scene, kMaxTicks);
	scene = skip.scene();
	debugC(3, kDebugScript, "town check take: held %d, starts after %d ticks, done %d ticks later, slot 0 %s, scene %s, held %d",
	       held ? 1 : 0, start, t, _inventory->getSlot(0).c_str(), scene.c_str(), tc->isBusy() ? 1 : 0);
	run.until(Is::noScene, 1);

	// The second hotspot: clicked until its C state, whose step 7
	// frightens three of the gang.
	Common::String frights;
	for (int k = 0; k < 3 && hotspot2 >= 0; k++) {
		const Common::String state = _script->getClickableState(hotspot2);
		_script->clickClickable(hotspot2);
		for (uint n = 0; n < 400; n++) {
			run.until(Is::noScene, 1);
			if (_currentRoom->getStep(hotspot2) == 7 && _currentRoom->getAnim(hotspot2) == "ANIM_P01_TC_HOTSPOT2C") {
				for (uint g = 0; g < ARRAYSIZE(kGang3); g++) {
					Character *m = _script->getCharacter(kGang3[g]);
					frights += Common::String::format("%s %d ", kGang3[g], m ? m->getState() : -1);
				}
				break;
			}
			if (_currentRoom->isStopped(hotspot2))
				break;
		}
		frights += "(" + state + ") ";
		run.until(Is::noScene, 40);
	}
	debugC(3, kDebugScript, "town check hotspot 2: %s", frights.c_str());

	// 5. The keys: SignClicked, then a visit with the sign taken.
	Common::INIFile ini;
	tc->saveKeys(ini);
	Common::String v;
	ini.getKey("SignClicked", kRoom, v);
	tc->reset();
	tc->loadKeys(ini);
	debugC(3, kDebugScript, "town check keys: SignClicked=%s reads back %d", v.c_str(), tc->isSignClicked() ? 1 : 0);
	showRoom(kRoom);
	int signNow = -1, hangingNow = -1;
	for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
		if (_currentRoom->getObjectName(s) == "OBJ_INV_SIGN")
			signNow = s;
		if (_currentRoom->getObjectName(s) == "OBJ_P01_TC_HOTSPOT4")
			hangingNow = s;
	}
	debugC(3, kDebugScript, "town check return: sign %s, hanging sign hidden %d", signNow < 0 ? "not built" : "built",
	       hangingNow >= 0 ? _currentRoom->isHidden(hangingNow) : -1);

	// The profile: every room's section; Town Center's with SignClicked.
	_script->getMemory(kRoom).openingPlayed = true;
	_script->getMemory(kRoom).snacksGiven = 1;
	Common::INIFile saved;
	buildProfile(saved);
	Common::String op, sg, sc, street;
	saved.getKey("OpeningPlayed", kRoom, op);
	saved.getKey("SnacksGiven", kRoom, sg);
	saved.getKey("SignClicked", kRoom, sc);
	saved.getKey("OpeningPlayed", "ROOM_P03_Street1", street);
	uint sections = 0;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		sections += saved.hasSection(_art->getRoomName(i)) ? 1 : 0;
	_script->resetMemories();
	tc->reset();
	applyProfile(saved);
	debugC(3, kDebugScript, "town check profile: %u of %u rooms have a section; OpeningPlayed=%s SnacksGiven=%s SignClicked=%s, "
	       "Street1 OpeningPlayed=%s; read back %d %u %d", sections, _art->getRoomCount(), op.c_str(), sg.c_str(), sc.c_str(),
	       street.c_str(), _script->getMemory(kRoom).openingPlayed ? 1 : 0, _script->getMemory(kRoom).snacksGiven,
	       tc->isSignClicked() ? 1 : 0);

	// 6. Idle chatter: the first after the opening, until one starts.
	_script->resetMemories();
	_script->getMemory(kRoom).openingPlayed = true;
	showRoom(kRoom);
	Common::String chatter;
	for (int k = 0; k < 6; k++) {
		t = run.until(Is::scene, 3000);
		chatter += Common::String::format("%s@%d ", skip.scene().c_str(), t);
		run.until(Is::noScene, 1);
	}
	debugC(3, kDebugScript, "town check idle: %s", chatter.c_str());

	_sound->stop();
	_inventory->reset();
	_script->resetMemories();
	fresh.room(kRoom);
	_checking = false;
	debugC(3, kDebugScript, "town check: done");
}

} // End of namespace Scooby
