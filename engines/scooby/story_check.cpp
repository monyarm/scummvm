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

#include "common/config-manager.h"
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

static const char *const kRoomTownCenter = "ROOM_P01_Town_Center";

void ScoobyEngine::checkRoomSetup() {
	// Every room, entered from nowhere with fresh state: the music its entry
	// plays from a fresh start, its opening, whether input is held for it,
	// and the tick the opening starts. Then the music's variants in turn.
	_checking = true;
	const uint kMaxTicks = 400;
	// As a new game has it (0x42DBC0): the Sheriff waits for the sign, so
	// Street 1 doesn't start his clue on entry.
	setGameFlag("SheriffWaiting", true);
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
		Common::String opening = _script->getOpening();
		// Three of Chase B's rooms draw their opening each visit (0x41FA80,
		// rand() & 3): the line names the set, so runs compare.
		if ((name == "ROOM_P25_Chase_B2" || name == "ROOM_P28_Chase_B5" || name == "ROOM_P29_Chase_B6") && opening.hasPrefix("Chase_Branch_3.230"))
			opening = "Chase_Branch_3.230[_|B2_|C_|D_]";
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
	_profile.removeKey("SheriffWaiting", "Game");
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
	// No ghost in place of a click (1 in 30): the check is the same on every run.
	_script->setGhostChance(nullptr, 0);
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
	// The Global line is Fred's A or B or Velma's C by rand(); the line
	// names the set, so runs compare.
	struct Same {
		static Common::String of(const Common::String &in) {
			Common::String out = in;
			static const char *const kLines[] = { "Global.Snack_GivenA_", "Global.Snack_GivenB_", "Global.Snack_GivenC_" };
			for (uint i = 0; i < ARRAYSIZE(kLines); i++) {
				for (size_t at = out.find(kLines[i]); at != Common::String::npos; at = out.find(kLines[i]))
					out = Common::String(out.c_str(), at) + "Global.Snack_Given[ABC]_" + Common::String(out.c_str() + at + strlen(kLines[i]));
			}
			return out;
		}
	};
	const Common::String fed = Same::of(_inventory->takeLog());
	debugC(3, kDebugScript, "town check snack: %sline %s; snacks given %u", fed.c_str(), Same::of(scene).c_str(),
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
	// Which scene, and when, come from rand(); the line reports what the
	// trace fixes. With moans every chatter is a scene, 25-54 seconds
	// (900-1944 ticks, +1 for the tick it runs out) after the last; without
	// them only 1100D_ plays.
	setGameFlag("MoaningActive", true);
	uint inTime = 0, known = 0;
	const int kRuns = 6;
	for (int k = 0; k < kRuns; k++) {
		t = run.until(Is::scene, 3000);
		const Common::String s = skip.scene();
		inTime += (t >= 900 && t <= 1945) ? 1 : 0;
		known += (s == "Town_Center.100B_" || s == "Town_Center.100B2_" || s == "Town_Center.100B3_" || s == "Town_Center.1100D_") ? 1 : 0;
		run.until(Is::noScene, 1);
	}
	setGameFlag("MoaningActive", false);
	uint others = 0;
	for (int k = 0; k < kRuns; k++) {
		t = run.until(Is::scene, 4000);
		if (t >= 0 && skip.scene() != "Town_Center.1100D_")
			others++;
		run.until(Is::noScene, 1);
	}
	debugC(3, kDebugScript, "town check idle: with moans %u of %d after 25-54 s, %u of %d a 100B or 1100D scene; without, %u other scenes",
	       inTime, kRuns, known, kRuns, others);

	_sound->stop();
	_inventory->reset();
	_script->resetMemories();
	fresh.room(kRoom);
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
	debugC(3, kDebugScript, "town check: done");
}

void ScoobyEngine::checkStreets() {
	// Showdown's Street 2 and Street 3 (WP-033), every step against the trace.
	const Common::String kStreet2 = "ROOM_P04_Street2", kStreet3 = "ROOM_P11_Street3";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kStreet2;
	if (!found)
		return;
	_checking = true;
	// No ghost in place of a click (1 in 30): the check is the same on every run.
	_script->setGhostChance(nullptr, 0);
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
	};
	struct Skip {
		ScoobyEngine &e;
		Common::String scene() {
			const Common::String s = e._script->isScenePlaying() ? e._script->getScene() : Common::String("-");
			e._script->skip();
			return s;
		}
	} skip = { *this };
	struct Find {
		static int sprite(ScoobyEngine &e, const Common::String &object) {
			for (uint s = 0; s < e._currentRoom->getSpriteCount(); s++) {
				if (e._currentRoom->getObjectName(s) == object)
					return s;
			}
			return -1;
		}
		static Common::String point(ScoobyEngine &e, int sprite) {
			for (int y = 4; y < 480 && sprite >= 0; y += 8) {
				for (int x = 4; x < 640; x += 8) {
					if (e._currentRoom->hitTest(x, y) == sprite)
						return Common::String::format("%d,%d", x, y);
				}
			}
			return "-";
		}
		static int exitTo(ScoobyEngine &e, const char *room) {
			for (uint s = 0; s < e._currentRoom->getSpriteCount(); s++) {
				if (e._currentRoom->getDestination(s) == room)
					return s;
			}
			return -1;
		}
		static Common::String who(ScoobyEngine &e, const char *name) {
			Character *c = e._script->getCharacter(name);
			if (!c)
				return Common::String::format("%s -", name);
			return Common::String::format("%s %d %s depth %d", name, c->getState(), e._currentRoom->getAnim(c->getSprite()).c_str(),
			                              e._currentRoom->getDepth(c->getSprite()));
		}
	};
	const uint kMax = 3000;
	newGameForCheck();

	// Street 2 from Town Center: the moaning opening; Fred and Daphne walk in.
	showRoom(kRoomTownCenter);
	_script->getMemory(kRoomTownCenter).openingPlayed = true;
	debugC(3, kDebugScript, "street check points: Town Center to Street 2 %s, to Street 3 %s", Find::point(*this, Find::exitTo(*this, "ROOM_P04_Street2")).c_str(),
	       Find::point(*this, Find::exitTo(*this, "ROOM_P11_Street3")).c_str());
	showRoom(kStreet2);
	debugC(3, kDebugScript, "street check 2 from Town Center: opening '%s', held %d; %s; %s", _script->getOpening().c_str(),
	       _script->isBusy() ? 1 : 0, Find::who(*this, "FRED").c_str(), Find::who(*this, "DAPHNE").c_str());
	int t = run.until(Is::scene, kMax);
	Common::String scene = skip.scene();
	run.until(Is::noScene, 1);
	debugC(3, kDebugScript, "street check 2 opening: %s after %d ticks; played %d", scene.c_str(), t,
	       _script->getMemory(kStreet2).openingPlayed ? 1 : 0);
	// A second visit: the opening again, while the moans are on.
	showRoom(kStreet3);
	showRoom(kStreet2);
	debugC(3, kDebugScript, "street check 2 again: opening '%s', held %d", _script->getOpening().c_str(), _script->isBusy() ? 1 : 0);
	setGameFlag("MoaningActive", false);
	showRoom(kStreet3);
	showRoom(kStreet2);
	debugC(3, kDebugScript, "street check 2 without moans: opening '%s', held %d", _script->getOpening().c_str(), _script->isBusy() ? 1 : 0);
	setGameFlag("MoaningActive", true);
	_script->skip();

	// Gummy, the donkey, the window and the clock, as the story moves on.
	struct Placed {
		static Common::String line(ScoobyEngine &e) {
			Common::String out;
			static const char *const kObjects[] = { "OBJ_GUMMY", "OBJ_DONKEY", "OBJ_P04_STREET2_WINDOW", "OBJ_P04_STREET2_CLOCK" };
			for (uint i = 0; i < ARRAYSIZE(kObjects); i++) {
				int s = -1;
				for (uint k = 0; k < e._currentRoom->getSpriteCount() && s < 0; k++) {
					if (e._currentRoom->getObjectName(k).hasPrefix(kObjects[i]))
						s = k;
				}
				if (s < 0)
					out += Common::String::format("%s none; ", kObjects[i]);
				else
					out += Common::String::format("%s %s x %d hit %d; ", e._currentRoom->getObjectName(s).c_str(), e._currentRoom->getAnim(s).c_str(),
					                              e._currentRoom->getX(s), e._currentRoom->getHitType(s));
			}
			return out;
		}
	};
	debugC(3, kDebugScript, "street check 2 before Artie: %s", Placed::line(*this).c_str());
	_inventory->meetSuspect("ARTIE");
	showRoom(kStreet3);
	showRoom(kStreet2);
	_script->skip();
	Character *gummy = _script->getCharacter("GUMMY");
	debugC(3, kDebugScript, "street check 2 Artie met: %s Gummy state %d", Placed::line(*this).c_str(), gummy ? gummy->getState() : -1);
	setGameFlag("GummyPuzzle", true);
	setGameString("ClueOwners0", "GUMMY");
	_inventory->addItem("OBJ_INV_CLOCK");
	showRoom(kStreet3);
	showRoom(kStreet2);
	_script->skip();
	gummy = _script->getCharacter("GUMMY");
	debugC(3, kDebugScript, "street check 2 Gummy's clue given: %s Gummy state %d", Placed::line(*this).c_str(), gummy ? gummy->getState() : -1);

	// The hotspots' lines, input on; the exit to the general store.
	Common::String lines;
	static const char *const kHotspots[] = { "OBJ_P04_STREET2_HOTSPOT1", "OBJ_P04_STREET2_HOTSPOT2" };
	for (uint i = 0; i < ARRAYSIZE(kHotspots); i++) {
		const int s = Find::sprite(*this, kHotspots[i]);
		_script->clickClickable(s);
		run.until(Is::scene, 5);
		const bool busy = _script->isBusy();
		lines += Common::String::format("%s: %s, held %d; ", kHotspots[i], skip.scene().c_str(), busy ? 1 : 0);
		run.until(Is::noScene, 60);
	}
	debugC(3, kDebugScript, "street check 2 hotspots: %s", lines.c_str());
	debugC(3, kDebugScript, "street check points: Street 2 hotspot 1 %s, to the store %s, to Town Center %s", Find::point(*this, Find::sprite(*this, kHotspots[0])).c_str(),
	       Find::point(*this, Find::exitTo(*this, "ROOM_P10_Front_Of_General_Store")).c_str(), Find::point(*this, Find::exitTo(*this, "ROOM_P01_Town_Center")).c_str());
	int store = -1;
	for (uint s = 0; s < _currentRoom->getSpriteCount() && store < 0; s++) {
		if (_currentRoom->getDestination(s) == "ROOM_P10_Front_Of_General_Store")
			store = s;
	}
	clickExit(store);
	scene = skip.scene();
	debugC(3, kDebugScript, "street check 2 exit to the store: scene %s", scene.hasPrefix("Street_2.General_Store") ? "Street_2.General_Store[12]_" : scene.c_str());
	run.until(Is::scene, kMax);

	// Street 3: who walks in from where.
	static const char *const kFrom[] = { "ROOM_P01_Town_Center", "ROOM_P12_Saloon", "ROOM_P13_Bank" };
	static const char *const kWho[] = { "FRED", "VELMA", "SHAGGY" };
	for (uint i = 0; i < ARRAYSIZE(kFrom); i++) {
		showRoom(kFrom[i]);
		_script->skip();
		showRoom(kStreet3);
		debugC(3, kDebugScript, "street check 3 from %s: %s, opening '%s'", kFrom[i], Find::who(*this, kWho[i]).c_str(), _script->getOpening().c_str());
		_script->skip();
		run.until(Is::noScene, 1);
	}
	// The first hotspot's scenes in turn.
	Common::String turns;
	const int hotspot = Find::sprite(*this, "OBJ_P11_STREET3_HOTSPOT");
	struct Rest {
		static bool hotspot(ScoobyEngine &e) { return !e._script->isClickablePlaying(e._checkSprite) && !e._script->isScenePlaying(); }
	};
	_checkSprite = hotspot;
	for (int k = 0; k < 4; k++) {
		_script->clickClickable(hotspot);
		run.until(Is::scene, 5);
		turns += skip.scene() + " ";
		run.until(Rest::hotspot, kMax);
	}
	debugC(3, kDebugScript, "street check 3 hotspot: %s", turns.c_str());
	// The second cuts the music's group; when it's still, the next variant.
	const int hotspot2 = Find::sprite(*this, "OBJ_P11_STREET3_HOTSPOT2");
	debugC(3, kDebugScript, "street check points: Street 3 hotspot 2 %s, hotspot %s", Find::point(*this, hotspot2).c_str(), Find::point(*this, hotspot).c_str());
	const Common::String before = _music->getClip();
	_script->clickClickable(hotspot2);
	run.until(Is::noScene, 1);
	const int during = _music->getGroup();
	struct Still {
		static bool hotspot2(ScoobyEngine &e) { return e._music->getGroup() == 2; }
	};
	t = run.until(Still::hotspot2, kMax);
	debugC(3, kDebugScript, "street check 3 hotspot 2: music %s, group %d while it plays %d steps, group 2 again after %d ticks: %s",
	       before.c_str(), during, (int)_currentRoom->getStepCount(hotspot2), t, _music->getClip().c_str());
	// The third: its scene on step 3 of its B state.
	const int hotspot3 = Find::sprite(*this, "OBJ_P11_STREET3_HOTSPOT3");
	Common::String third;
	// Each click: the first scene while it plays, and the step it starts on;
	// a scene skipped on step 3 starts again while the step lasts, as the
	// tick tests it each time.
	_checkSprite = hotspot3;
	for (int k = 0; k < 3; k++) {
		const Common::String state = _script->getClickableState(hotspot3);
		_script->clickClickable(hotspot3);
		Common::String first = "-";
		int step = -1;
		run.until(Is::noScene, 1);
		for (uint n = 0; n < kMax && _script->isClickablePlaying(hotspot3); n++) {
			if (_script->isScenePlaying()) {
				if (first == "-") {
					first = _script->getScene();
					step = _currentRoom->getStep(hotspot3);
				}
				_script->skip();
			}
			run.until(Is::noScene, 1);
		}
		third += Common::String::format("%s: %s at step %d; ", state.c_str(), first.c_str(), step);
		run.until(Rest::hotspot, kMax);
	}
	debugC(3, kDebugScript, "street check 3 hotspot 3: %s", third.c_str());
	// Idle chatter: 100B2_ (with moans) or 1100A_, none without moans on rand 0.
	uint known = 0, others = 0;
	for (int k = 0; k < 6; k++) {
		t = run.until(Is::scene, 4000);
		const Common::String s = skip.scene();
		known += (s == "Street_3.100B2_" || s == "Street_3.1100A_") ? 1 : 0;
		run.until(Is::noScene, 1);
	}
	setGameFlag("MoaningActive", false);
	for (int k = 0; k < 4; k++) {
		t = run.until(Is::scene, 4000);
		if (t >= 0 && skip.scene() != "Street_3.1100A_")
			others++;
		run.until(Is::noScene, 1);
	}
	debugC(3, kDebugScript, "street check 3 idle: with moans %u of 6 a 100B2 or 1100A scene; without, %u other scenes", known, others);

	_sound->stop();
	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
	debugC(3, kDebugScript, "street check: done");
}

void ScoobyEngine::checkClues() {
	// 0x42F990's rules over many draws, from fresh clue slots: the villain
	// gives a red herring about neither the villain nor himself; anyone else
	// one of their own three until all three are found, then (level 0) their
	// fourth or fifth, or (above) a red herring; a clue owner nothing more.
	static const char *const kSuspects[] = { "ARTIE", "GUMMY", "ICDOUBLE", "JENNY", "SHERIFF" };
	static const char *const kPrefix[] = { "ARTIE_", "GUMMY_", "IC_", "JENNY_", "SHERIFF_" };
	if (getGameId() != "showdown")
		return;
	struct Fresh {
		static void clues(ScoobyEngine &e) {
			for (int i = 0; i < 5; i++) {
				e.setGameString(Common::String::format("CluesFound%d", i), "");
				e.setGameString(Common::String::format("ClueOwners%d", i), "");
			}
		}
	};
	const int kDraws = 40;
	uint villainOk = 0, ownOk = 0, extraOk = 0, herringOk = 0, onceOk = 0, cases = 0;
	const int level = _difficulty;
	for (uint v = 0; v < ARRAYSIZE(kSuspects); v++) {
		setGameString("Whodunit", kSuspects[v]);
		for (uint g = 0; g < ARRAYSIZE(kSuspects); g++) {
			for (int d = 0; d < kDraws; d++) {
				cases++;
				Fresh::clues(*this);
				giveClue(kSuspects[g]);
				const Common::String clue = getGameString("CluesFound0");
				const bool owner = getGameString("ClueOwners0") == kSuspects[g];
				if (v == g) {
					// A red herring, about neither of them.
					villainOk += (owner && clue.hasPrefix("RH_") && !clue.hasPrefix(Common::String("RH_") + kPrefix[v])) ? 1 : 0;
				} else {
					ownOk += (owner && clue.hasPrefix(kPrefix[g])) ? 1 : 0;
				}
				// A clue owner gives nothing more.
				giveClue(kSuspects[g]);
				onceOk += getGameString("CluesFound1").empty() ? 1 : 0;
			}
			if (v == g)
				continue;
			// All three of their own found: the fourth or fifth at level 0, a
			// red herring above.
			for (int l = 0; l <= 1; l++) {
				_difficulty = l;
				for (int d = 0; d < kDraws; d++) {
					Fresh::clues(*this);
					for (int k = 0; k < 3; k++) {
						static const char *const kOwn[][3] = {
							{ "ARTIE_ESCAPE_BOOK", "ARTIE_CLASS_LIST", "ARTIE_MAP" },
							{ "GUMMY_COWBELL", "GUMMY_HORSESHOE_LIFT", "GUMMY_MAP" },
							{ "IC_BINOCULARS", "IC_EYE_DOCTOR", "IC_MAP" },
							{ "JENNY_CASSETTE_TAPE", "JENNY_GHOST_TOWN_BOOK", "JENNY_MAP" },
							{ "SHERIFF_SPURS", "SHERIFF_FEATHER", "SHERIFF_MAP" }
						};
						setGameString(Common::String::format("CluesFound%d", k), kOwn[g][k]);
					}
					giveClue(kSuspects[g]);
					const Common::String clue = getGameString("CluesFound3");
					if (l == 0)
						extraOk += (clue.hasPrefix(kPrefix[g]) && !clue.hasSuffix("_MAP") && clue != getGameString("CluesFound0") &&
						            clue != getGameString("CluesFound1") && clue != getGameString("CluesFound2")) ? 1 : 0;
					else
						herringOk += (clue.hasPrefix("RH_") && !clue.hasPrefix(Common::String("RH_") + kPrefix[g]) &&
						              !clue.hasPrefix(Common::String("RH_") + kPrefix[v])) ? 1 : 0;
				}
			}
			_difficulty = level;
		}
	}
	Fresh::clues(*this);
	setNewClue(false);
	debugC(3, kDebugScript, "clue check: %u draws; villain's red herring %u of %d, own clue %u of %d, once %u of %u; all three found: level 0 extra %u of %d, level 1 red herring %u of %d",
	       cases, villainOk, 5 * kDraws, ownOk, 20 * kDraws, onceOk, cases, extraOk, 20 * kDraws, herringOk, 20 * kDraws);
}

void ScoobyEngine::checkStreet1() {
	// Showdown's Street 1 (WP-034), every step against the trace.
	const Common::String kStreet1 = "ROOM_P03_Street1";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kStreet1;
	if (!found)
		return;
	_checking = true;
	// No ghost in place of a click (1 in 30): the check is the same on every run.
	_script->setGhostChance(nullptr, 0);
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
		static bool free(ScoobyEngine &e) { return !e._script->isScenePlaying() && !(e._roomClass && e._roomClass->isBusy()); }
	};
	struct Find {
		static int sprite(ScoobyEngine &e, const Common::String &object) {
			for (uint s = 0; s < e._currentRoom->getSpriteCount(); s++) {
				if (e._currentRoom->getObjectName(s) == object)
					return s;
			}
			return -1;
		}
		static Common::String placed(ScoobyEngine &e) {
			Common::String out;
			static const char *const kObjects[] = { "OBJ_SHERIFF", "OBJ_P03_TO_P02", "OBJ_P03_STREET1_DOOR_OVERLAY", "OBJ_INV_STRONGBOXKEY", "OBJ_P03_STREET1_TROUGH" };
			for (uint i = 0; i < ARRAYSIZE(kObjects); i++) {
				const int s = sprite(e, kObjects[i]);
				if (s < 0)
					out += Common::String::format("%s none; ", kObjects[i]);
				else
					out += Common::String::format("%s %s%s%s; ", kObjects[i], e._currentRoom->getAnim(s).c_str(), e._currentRoom->isHidden(s) ? " hidden" : "",
					                              e._currentRoom->getRequired(s).empty() ? "" : (Common::String(" asks ") + e._currentRoom->getRequired(s)).c_str());
			}
			return out;
		}
	};
	// Name each scene as it starts and skip it (see the town check).
	struct Skip {
		ScoobyEngine &e;
		Common::String scene() {
			const Common::String s = e._script->isScenePlaying() ? e._script->getScene() : Common::String("-");
			e._script->skip();
			return s;
		}
	} skip = { *this };
	const uint kMax = 3000;
	newGameForCheck();
	setGameFlag("SheriffWaiting", true);
	setGameFlag("SheriffPuzzle", false);
	setGameString("Whodunit", "JENNY");

	// 1. From Town Center: Fred walks in DR; the moaning opening. No Sheriff
	// before Artie's clue; the office shut.
	showRoom(kRoomTownCenter);
	_script->getMemory(kRoomTownCenter).openingPlayed = true;
	showRoom(kStreet1);
	Character *fred = _script->getCharacter("FRED");
	debugC(3, kDebugScript, "street1 check from Town Center: FRED %d %s; opening '%s', held %d", fred ? fred->getState() : -1,
	       fred ? _currentRoom->getAnim(fred->getSprite()).c_str() : "-", _script->getOpening().c_str(), _script->isBusy() ? 1 : 0);
	_script->skip();
	debugC(3, kDebugScript, "street1 check before Artie's clue: %s", Find::placed(*this).c_str());

	// 2. The Sheriff: his introduction on Big Head, then his close-up.
	setGameString("ClueOwners0", "ARTIE");
	setGameString("CluesFound0", "ARTIE_MAP");
	showRoom(kRoomTownCenter);
	showRoom(kStreet1);
	_script->skip();
	run.until(Is::noScene, 1);
	debugC(3, kDebugScript, "street1 check after Artie's clue: %s", Find::placed(*this).c_str());
	Character *sheriff = _script->getCharacter("SHERIFF");
	const int sheriffSprite = sheriff ? sheriff->getSprite() : -1;
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
	debugC(3, kDebugScript, "street1 check points: Sheriff %s, trough %s, van %s", Point::of(*this, sheriffSprite).c_str(),
	       Point::of(*this, Find::sprite(*this, "OBJ_P03_STREET1_TROUGH")).c_str(), Point::of(*this, _inventory->getVanSprite()).c_str());
	_requestedRoom.clear();
	_roomClass->click(sheriffSprite);
	const Common::String first = _requestedRoom;
	const bool met = _inventory->hasMetSuspect("SHERIFF");
	_requestedRoom.clear();
	_roomClass->click(sheriffSprite);
	debugC(3, kDebugScript, "street1 check Sheriff clicked: first goes to %s, met %d, intro played %d; then to %s", first.c_str(), met ? 1 : 0,
	       static_cast<Street1Room *>(_roomClass)->isIntroPlayed() ? 1 : 0, _requestedRoom.c_str());
	_requestedRoom.clear();

	// 3. His puzzle, waiting for the sign: the hints in turn, input on.
	setGameFlag("SheriffPuzzle", true);
	Common::String hints;
	for (int k = 0; k < 4; k++) {
		_roomClass->click(sheriffSprite);
		hints += Common::String::format("%s held %d; ", skip.scene().c_str(), _script->isBusy() ? 1 : 0);
		run.until(Is::noScene, 1);
	}
	debugC(3, kDebugScript, "street1 check hints: %s", hints.c_str());

	// 4. The sign: his line, then his clue, he gives it, and the gang thanks
	// him and goes to the suspects and clues.
	_inventory->addItem("OBJ_INV_SIGN");
	_inventory->setCursor("ANIM_CURSORPOP_SIGN");
	const bool taken = _roomClass->click(sheriffSprite);
	Common::String steps = Common::String::format("taken %d, sign %s, waiting %d; ", taken ? 1 : 0, (_inventory->getSlot(0) == "OBJ_INV_SIGN" || _inventory->getSlot(1) == "OBJ_INV_SIGN") ? "kept" : "given",
	                                              getGameFlag("SheriffWaiting") ? 1 : 0);
	steps += skip.scene() + "; ";
	run.until(Is::scene, 5);
	// Which of the three lines, and which of his own three clues, come from
	// rand(); the line names the sets.
	const Common::String line = skip.scene();
	const Common::String clue = getGameString("CluesFound1");
	const bool own = clue == "SHERIFF_SPURS" || clue == "SHERIFF_FEATHER" || clue == "SHERIFF_MAP";
	steps += (line.hasPrefix("Street_1.130C") ? Common::String("Street_1.130C[123]_") : line) + "; clue " + (own ? Common::String("one of his own three") : clue) +
	         " from " + getGameString("ClueOwners1") + "; ";
	struct Giving {
		static bool started(ScoobyEngine &e) {
			Character *s = e._script->getCharacter("SHERIFF");
			return s && s->getState() == Character::kStateAct;
		}
	};
	int t = run.until(Giving::started, kMax);
	steps += Common::String::format("gives after %d ticks (%s), held %d; ", t, sheriff ? _currentRoom->getAnim(sheriffSprite).c_str() : "-",
	                                _roomClass->isBusy() ? 1 : 0);
	run.until(Is::scene, kMax);
	steps += skip.scene() + "; ";
	run.next.clear();
	run.until(Is::scene, kMax);
	debugC(3, kDebugScript, "street1 check sign: %sthen %s", steps.c_str(), run.next.c_str());

	// 5. Back, his clue given: the office open, he stands at (501, 270), a
	// line for him.
	showRoom(kRoomTownCenter);
	showRoom(kStreet1);
	_script->skip();
	run.until(Is::noScene, 1);
	sheriff = _script->getCharacter("SHERIFF");
	_roomClass->click(sheriff ? sheriff->getSprite() : -1);
	debugC(3, kDebugScript, "street1 check after his clue: %s Sheriff at %d,%d; clicked: %s", Find::placed(*this).c_str(),
	       sheriff ? _currentRoom->getX(sheriff->getSprite()) : 0, sheriff ? _currentRoom->getY(sheriff->getSprite()) : 0, skip.scene().c_str());
	run.until(Is::noScene, 1);

	// 6. The trough: lines before the handle, the handle, then four pumps.
	const int trough = Find::sprite(*this, "OBJ_P03_STREET1_TROUGH");
	struct Use {
		static void with(ScoobyEngine &e, int sprite, const char *cursor) {
			e._inventory->setCursor(cursor);
			e._inventory->click(sprite);
		}
	};
	Common::String lines;
	for (int k = 0; k < 4; k++) {
		Use::with(*this, trough, "ANIM_CURSORARROW");
		run.until(Is::scene, 3);
		lines += skip.scene() + " ";
		run.until(Is::noScene, 1);
	}
	debugC(3, kDebugScript, "street1 check trough before the handle: %s", lines.c_str());
	_inventory->addItem("OBJ_INV_PUMP_HANDLE");
	Use::with(*this, trough, "ANIM_CURSORPUMP_HANDLE");
	run.until(Is::scene, 3);
	Common::String pumps = Common::String::format("handle %s; ", _inventory->getSlot(0) == "OBJ_INV_PUMP_HANDLE" ? "kept" : "on the pump");
	struct Street1 {
		static Street1Room *room(ScoobyEngine &e) { return static_cast<Street1Room *>(e._roomClass); }
	};
	// Each pump: what Shaggy becomes, the scenes that start (each skipped),
	// the trough's sequence and the key.
	for (int k = 0; k < 5; k++) {
		if (k > 0)
			Use::with(*this, trough, "ANIM_CURSORARROW");
		Common::String shaggyAs = "-", scenes;
		for (uint n = 0; n < kMax; n++) {
			run.until(Is::noScene, 1);
			Character *shaggy = _script->getCharacter("SHAGGY");
			if (shaggy && shaggy->getState() == Character::kStateSpecial)
				shaggyAs = _currentRoom->getObjectName(shaggy->getSprite());
			if (_script->isScenePlaying())
				scenes += skip.scene() + " ";
			if (!Street1::room(*this)->isPumping() && !_script->isScenePlaying() && n > 2)
				break;
		}
		const int keySprite = Find::sprite(*this, "OBJ_INV_STRONGBOXKEY");
		pumps += Common::String::format("%s, scenes %s, %s, %d pumps, key %s; ", shaggyAs.c_str(), scenes.empty() ? "- " : scenes.c_str(),
		                                _currentRoom->getAnim(trough).c_str(), Street1::room(*this)->getPumpAttempts(),
		                                keySprite >= 0 && !_currentRoom->isHidden(keySprite) ? "shows" : "hidden");
	}
	debugC(3, kDebugScript, "street1 check pump: %s", pumps.c_str());

	// 7. The key: Scooby takes it.
	const int key = Find::sprite(*this, "OBJ_INV_STRONGBOXKEY");
	Use::with(*this, key, "ANIM_CURSORARROW");
	run.until(Is::noScene, 2);
	Character *scooby = _script->getCharacter("SCOOBY");
	const Common::String taking = scooby ? _currentRoom->getObjectName(scooby->getSprite()) : "-";
	t = run.until(Is::free, kMax);
	debugC(3, kDebugScript, "street1 check key: Scooby as %s, done after %d ticks, key %s", taking.c_str(), t,
	       _inventory->hasItem("OBJ_INV_STRONGBOXKEY") ? "in the toolbar" : "not taken");

	// 8. The keys, saved and read back.
	Common::INIFile ini;
	_roomClass->saveKeys(ini);
	Common::String pa, sip, kv;
	ini.getKey("PumpAttempts", kStreet1, pa);
	ini.getKey("SheriffIntroPlayed", kStreet1, sip);
	ini.getKey("KeyVisible", kStreet1, kv);
	_roomClass->reset();
	_roomClass->loadKeys(ini);
	debugC(3, kDebugScript, "street1 check keys: PumpAttempts=%s SheriffIntroPlayed=%s KeyVisible=%s; read back %d %d %d", pa.c_str(), sip.c_str(), kv.c_str(),
	       Street1::room(*this)->getPumpAttempts(), Street1::room(*this)->isIntroPlayed() ? 1 : 0, Street1::room(*this)->isKeyVisible() ? 1 : 0);

	// 9. The second hotspot: its scene as it plays its A.
	showRoom(kRoomTownCenter);
	showRoom(kStreet1);
	_script->skip();
	run.until(Is::noScene, 1);
	const int hotspot2 = Find::sprite(*this, "OBJ_P03_STREET1_HOTSPOT2");
	Common::String states;
	for (int k = 0; k < 3; k++) {
		const Common::String state = _script->getClickableState(hotspot2);
		_script->clickClickable(hotspot2);
		run.until(Is::scene, 2);
		states += Common::String::format("%s: %s; ", state.c_str(), skip.scene().c_str());
		for (uint n = 0; n < kMax && _script->isClickablePlaying(hotspot2); n++) {
			_script->skip();
			run.until(Is::noScene, 1);
		}
	}
	debugC(3, kDebugScript, "street1 check hotspot 2: %s", states.c_str());

	// 10. The brick: the Sheriff gone, the trough asks for it; washed, the
	// final clue, movie 14 and the suspects and clues.
	_inventory->addItem("OBJ_INV_BRICK");
	showRoom(kRoomTownCenter);
	showRoom(kStreet1);
	_script->skip();
	run.until(Is::noScene, 1);
	debugC(3, kDebugScript, "street1 check brick taken: %s", Find::placed(*this).c_str());
	Use::with(*this, Find::sprite(*this, "OBJ_P03_STREET1_TROUGH"), "ANIM_CURSORBRICK");
	run.until(Is::scene, 3);
	const Common::String washed = skip.scene();
	run.next.clear();
	t = run.until(Is::scene, kMax);
	debugC(3, kDebugScript, "street1 check brick: %s, brick %s, FinalClue %d; then movie %d and %s", washed.c_str(),
	       _inventory->hasItem("OBJ_INV_BRICK") && _inventory->getSlot(0) == "OBJ_INV_BRICK" ? "kept" : "used", getGameFlag("FinalClue") ? 1 : 0,
	       run.movie, run.next.c_str());

	// 11. To the post office with the lantern: no scene.
	Common::String scene = "Street_1.Something_";
	static_cast<Street1Room *>(getRoomClass(kStreet1))->leaving("ROOM_P08_Post_Office", scene);
	Common::String without = scene;
	_inventory->addItem("OBJ_INV_LANTERN");
	scene = "Street_1.Something_";
	static_cast<Street1Room *>(getRoomClass(kStreet1))->leaving("ROOM_P08_Post_Office", scene);
	debugC(3, kDebugScript, "street1 check to the post office: scene '%s' without the lantern, '%s' with it", without.c_str(), scene.c_str());

	_sound->stop();
	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
	debugC(3, kDebugScript, "street1 check: done");
}

void ScoobyEngine::newGameForCheck() {
	// A fresh story for the checks: every room's memory and class, the
	// inventory and Showdown's values.
	_script->resetMemories();
	_script->resetClickableStates();
	_inventory->reset();
	_profile = Common::INIFile();
	Common::Array<RoomClass *> classes = getAllRoomClasses();
	for (uint i = 0; i < classes.size(); i++)
		classes[i]->reset();
	setGameFlag("MoaningActive", true);
}

} // End of namespace Scooby