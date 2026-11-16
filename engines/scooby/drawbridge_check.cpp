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

#include "scooby/art_archive.h"
#include "scooby/character.h"
#include "scooby/detection.h"
#include "scooby/drawbridge_rooms.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

void ScoobyEngine::checkDrawbridge() {
	// Phantom's drawbridge and town (WP-057): the classes, the new game's
	// story values, the drawbridge's reset, entry, flags, puzzle, exit, sign,
	// profile and revisit, and the town's walk-ins and screams.
	const char *const kDB = "ROOM_P01_Drawbridge";
	const char *const kTown = "ROOM_P04_Town";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kDB;
	if (!found) {
		debugC(3, kDebugScript, "drawbridge check: no drawbridge");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *a = createRoomClass(*this, kDB);
		RoomClass *b = createRoomClass(*this, kTown);
		debugC(3, kDebugScript, "drawbridge check classes: Drawbridge %s; Town %s", dynamic_cast<DrawbridgeRoom *>(a) ? "drawbridge" : "-",
		       dynamic_cast<TownRoom *>(b) ? "town" : "-");
		delete a;
		delete b;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto db = [&]() { return dynamic_cast<DrawbridgeRoom *>(getRoomClass(kDB)); };
	auto find = [&](const Common::String &object) {
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == object && !_currentRoom->isDetached(s))
				return (int)s;
		}
		return -1;
	};
	auto parentName = [&](int s) {
		const int p = _currentRoom->getParent(s);
		return p == 0 ? Common::String("root") : _currentRoom->getLabel(p);
	};
	// A sprite: its sequence, mode, step (r for a random start below 4),
	// depth, hit type and parent.
	auto sprite = [&](int s, bool randomStep) {
		if (s < 0)
			return Common::String("none");
		Room &r = *_currentRoom;
		const Common::String step = randomStep ? (r.getStep(s) < 4 ? Common::String("r") : Common::String::format("%u!", r.getStep(s)))
		                                       : Common::String::format("%u", r.getStep(s));
		return Common::String::format("%s %s mode %d step %s depth %d hit %d under %s", r.getObjectName(s).c_str(), r.getAnim(s).c_str(), r.getMode(s),
		                              step.c_str(), r.getDepth(s), r.getHitType(s), parentName(s).c_str());
	};
	static const int kBanners[4] = { 1, 2, 3, 4 };
	// Enter the drawbridge with these colours, from @p from (none: no room).
	auto enterDB = [&](const int flags[4], const char *from) {
		newGameForCheck();
		db()->setStates(kBanners, flags);
		db()->setPuzzleDone(false);
		if (from)
			showRoom(from);
		db()->clearSounds();
		showRoom(kDB);
	};

	// The new game's story values.
	{
		_profile = Common::INIFile();
		setStoryValues();
		static const char *const kNo[] = {
			"EwanPuzzle", "MariaPuzzle", "BradPuzzle", "SkullPuzzle", "SkullMovie", "SkullPuzzleAttempted", "CrestPuzzle",
			"RedBallFound", "BlueBallFound", "BlueBallSeen", "YellowBallFound", "RedBallTaken", "BlueBallTaken",
			"YellowBallTaken", "WellVisited", "MazeEntered", "PlaqueSeen"
		};
		int no = 0;
		for (uint i = 0; i < ARRAYSIZE(kNo); i++)
			no += getGameString(kNo[i]) == "No" ? 1 : 0;
		const Common::String who = getGameString("Whodunit");
		const bool villain = who == "MARIA" || who == "BRAD" || who == "EWAN" || who == "WIZARD" || who == "JESTER";
		int seen = 0;
		bool perm = true;
		for (uint i = 0; i < 4; i++) {
			const int v = getGameInt(Common::String::format("CrestSequence%u", i));
			perm = perm && v >= 1 && v <= 4 && !(seen & (1 << v));
			seen |= 1 << v;
		}
		bool digits = true;
		for (uint i = 0; i < 4; i++) {
			const Common::String v = getGameString(Common::String::format("SkullCombo%u", i));
			digits = digits && (v == "0" || v == "1" || v == "2" || v == "3");
		}
		debugC(3, kDebugScript, "drawbridge check story: %d of %u flags No, Screaming %s, Whodunit one of five %d, the crest a permutation of 1-4 %d, the skull's digits 0-3 %d",
		       no, ARRAYSIZE(kNo), getGameString("Screaming").c_str(), villain ? 1 : 0, perm ? 1 : 0, digits ? 1 : 0);
	}

	// The reset, 200 times.
	{
		DrawbridgeRoom *c = db();
		uint banners = 0, offsets = 0;
		bool done = false;
		for (uint k = 0; k < 200; k++) {
			c->reset();
			done = done || c->isPuzzleDone();
			for (uint i = 0; i < 4; i++) {
				banners |= 1 << c->getBannerState(i);
				offsets |= 1 << ((c->getFlagState(i) - c->getBannerState(i) + 4) % 4);
			}
		}
		Common::String b, o;
		for (int v = 0; v < 5; v++) {
			if (banners & (1 << v))
				b += Common::String::format(" %d", v);
			if (offsets & (1 << v))
				o += Common::String::format(" %d", v);
		}
		debugC(3, kDebugScript, "drawbridge check reset: banners seen%s; flags this many colours on from their banner%s; puzzle done %d", b.c_str(), o.c_str(), done ? 1 : 0);
	}

	// Entry from the town: flag 4 already on its banner.
	{
		static const int kFlagsIn[4] = { 2, 3, 4, 4 };
		enterDB(kFlagsIn, kTown);
		DrawbridgeRoom *c = db();
		Common::String out;
		for (uint i = 0; i < 4; i++) {
			const DrawbridgeRoom::Flag &f = c->getFlag(i);
			out += Common::String::format("%sflag %u %s, state %d next %d target %d clickable %d rollover %s", i ? "; " : "", i + 1, sprite(f.sprite, true).c_str(),
			                              f.state, f.next, f.target, f.enabled ? 1 : 0, f.sprite >= 0 ? _currentRoom->getRollover(f.sprite).c_str() : "-");
		}
		for (uint i = 0; i < 4; i++)
			out += Common::String::format("; banner %u %s", i + 1, sprite(c->getBanner(i), true).c_str());
		out += Common::String::format("; door %s; overlay %s; way out %s", sprite(c->getDoor(), false).c_str(), sprite(find("OBJ_P01_DRAWBRIDGE_OVERLAY"), false).c_str(),
		                              sprite(find("OBJ_P01_TO_P04"), false).c_str());
		Character *velma = _script->getCharacter("VELMA");
		out += Common::String::format("; snacks needed %u; VELMA state %d depth %d; sounds %s", _script->getSnacksNeeded(), velma ? velma->getState() : -1,
		                              velma && velma->getSprite() >= 0 ? _currentRoom->getDepth(velma->getSprite()) : 0, c->getSounds().c_str());
		debugC(3, kDebugScript, "drawbridge check entry: %s", out.c_str());
	}

	// A click on flag 1 (yellow on red): lowered, the next raised, waving.
	// Then a click in the lowering goes on at once.
	{
		static const int kFlagsIn[4] = { 2, 3, 4, 4 };
		Common::String out;
		for (int pass = 0; pass < 2; pass++) {
			enterDB(kFlagsIn, nullptr);
			DrawbridgeRoom *c = db();
			const int s = c->getFlag(0).sprite;
			c->clearSounds();
			int last = c->getFlag(0).state;
			Common::String seq;
			for (uint t = 1; t <= 200 && (t == 1 || c->getFlag(0).state > 4 || seq.empty()); t++) {
				if (t == 1 || (pass == 1 && t == 2))
					c->click(s);
				tick1();
				if (c->getFlag(0).state != last) {
					last = c->getFlag(0).state;
					seq += Common::String::format(" %d at %u (%s mode %d)", last, t, _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s));
				}
				if (c->getFlag(0).state <= 4 && !seq.empty())
					break;
			}
			out += Common::String::format("%s%s:%s; sounds %s", pass ? "; " : "", pass ? "clicked again at tick 2" : "clicked at tick 1", seq.c_str(), c->getSounds().c_str());
		}
		debugC(3, kDebugScript, "drawbridge check flag: %s", out.c_str());
	}

	// Velma idle, flag 4 moved off its banner: her line.
	{
		static const int kFlagsIn[4] = { 2, 3, 4, 4 };
		enterDB(kFlagsIn, nullptr);
		DrawbridgeRoom *c = db();
		Character *velma = _script->getCharacter("VELMA");
		const int before = velma ? velma->getState() : -1;
		c->click(c->getFlag(3).sprite);
		tick1();
		debugC(3, kDebugScript, "drawbridge check Velma: idle %d; flag 4 off its banner: her state %d, clip %s", before == Character::kStateIdle ? 1 : 0,
		       velma ? velma->getState() : -1, velma ? velma->getClipName().c_str() : "-");
	}

	// The last flag set: the puzzle done, the way out, the fanfares, the
	// drawbridge down. Then a click on a flag does nothing.
	{
		static const int kFlagsIn[4] = { 1, 2, 3, 3 };
		enterDB(kFlagsIn, nullptr);
		DrawbridgeRoom *c = db();
		const int s = c->getFlag(3).sprite;
		c->clearSounds();
		Common::String out;
		uint t = 1;
		for (; t <= 300 && !c->isPuzzleDone(); t++) {
			if (t == 1)
				c->click(s);
			tick1();
		}
		const int way = find("OBJ_P01_TO_P04");
		out = Common::String::format("done at tick %u, flag 4 state %d; flags clickable %d %d %d %d, rollovers '%s'; way out %s, to %s; sounds %s; fanfare %s",
		                             t - 1, c->getFlag(3).state, c->getFlag(0).enabled, c->getFlag(1).enabled, c->getFlag(2).enabled, c->getFlag(3).enabled,
		                             _currentRoom->getRollover(c->getFlag(0).sprite).c_str(), sprite(way, false).c_str(),
		                             way >= 0 ? _currentRoom->getDestination(way).c_str() : "-", c->getSounds().c_str(), c->getFanfare().c_str());
		for (uint k = 0; k < 5; k++)
			tick1();
		out += Common::String::format("; 5 ticks on, while it plays: door %s", sprite(c->getDoor(), false).c_str());
		c->clearSounds();
		_sound->stopClip("Drawbridge.Flag_Fanfare.2");
		tick1();
		out += Common::String::format("; it stopped, a tick on: door %s, sounds %s, scene '%s' held %d", sprite(c->getDoor(), false).c_str(), c->getSounds().c_str(),
		                              _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		_script->skip();
		c->clearSounds();
		const int state = c->getFlag(0).state;
		c->click(c->getFlag(0).sprite);
		tick1();
		out += Common::String::format("; flag 1 clicked: state %d to %d, sounds '%s'", state, c->getFlag(0).state, c->getSounds().c_str());
		debugC(3, kDebugScript, "drawbridge check solve: %s", out.c_str());

		// The way out: no snack yet, then one.
		_script->skip();
		const bool held = c->leaveBy("OBJ_P01_TO_P04");
		out = Common::String::format("no snack: taken %d, scene '%s' held %d, snack box flashing %d", held ? 1 : 0, _script->getScene().c_str(),
		                             _script->isBusy() ? 1 : 0, _inventory->isSnackFlashing() ? 1 : 0);
		_script->skip();
		_script->getMemory(kDB).snacksGiven = 1;
		out += Common::String::format("; a snack: taken %d", c->leaveBy("OBJ_P01_TO_P04") ? 1 : 0);
		debugC(3, kDebugScript, "drawbridge check way out: %s", out.c_str());

		// The profile, then a visit with the puzzle done.
		Common::INIFile ini;
		c->saveKeys(ini);
		Common::String keys;
		static const char *const kKeys[] = { "FlagState0", "BannerState0", "FlagState1", "BannerState1", "FlagState2", "BannerState2", "FlagState3", "BannerState3", "PuzzleDone" };
		for (uint i = 0; i < ARRAYSIZE(kKeys); i++) {
			Common::String v;
			ini.getKey(kKeys[i], kDB, v);
			keys += Common::String::format("%s%s=%s", i ? " " : "", kKeys[i], v.c_str());
		}
		newGameForCheck();
		db()->loadKeys(ini);
		showRoom(kTown);
		showRoom(kDB);
		c = db();
		out = Common::String::format("saved %s; loaded: puzzle done %d, flags %d %d %d %d", keys.c_str(), c->isPuzzleDone() ? 1 : 0, c->getFlag(0).state,
		                             c->getFlag(1).state, c->getFlag(2).state, c->getFlag(3).state);
		out += Common::String::format("; flag 1 clickable %d rollover '%s'; door %s; way out %s", c->getFlag(0).enabled ? 1 : 0,
		                              _currentRoom->getRollover(c->getFlag(0).sprite).c_str(), sprite(c->getDoor(), false).c_str(),
		                              sprite(find("OBJ_P01_TO_P04"), false).c_str());
		debugC(3, kDebugScript, "drawbridge check revisit: %s", out.c_str());
	}

	// The sign, three times.
	{
		static const int kFlagsIn[4] = { 2, 3, 4, 4 };
		enterDB(kFlagsIn, nullptr);
		Common::String out;
		for (uint k = 0; k < 3; k++) {
			_script->sendEvent("OBJ_INV_1428_SIGN", true);
			out += Common::String::format("%s'%s' held %d", k ? ", " : "", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
			_script->skip();
		}
		debugC(3, kDebugScript, "drawbridge check sign: %s", out.c_str());
	}

	// The town: who walks in, from each room; the screams.
	{
		static const char *const kFrom[] = { "ROOM_P01_Drawbridge", "ROOM_P03_Souvenir_Shop", "ROOM_P05_Gardeners_House", "ROOM_P11_Room_Of_Doors", "ROOM_P31_KST" };
		static const char *const kGang[] = { "FRED", "VELMA", "DAPHNE", "SHAGGY", "SCOOBY" };
		Common::String out;
		for (uint i = 0; i < ARRAYSIZE(kFrom); i++) {
			newGameForCheck();
			setGameFlag("Screaming", false);
			showRoom(kFrom[i]);
			showRoom(kTown);
			Common::String who;
			for (uint g = 0; g < ARRAYSIZE(kGang); g++) {
				Character *ch = _script->getCharacter(kGang[g]);
				if (ch && ch->getState() == Character::kStateEndWalk)
					who += Common::String::format("%s%s depth %d", who.empty() ? "" : " ", kGang[g], _currentRoom->getDepth(ch->getSprite()));
			}
			out += Common::String::format("%sfrom %s: %s", i ? "; " : "", kFrom[i] + 5, who.empty() ? "nobody" : who.c_str());
		}
		uint seen = 0, none = 0;
		for (uint k = 0; k < 200; k++) {
			newGameForCheck();
			setGameFlag("Screaming", k < 199);
			showRoom(kTown);
			const Common::String &sc = dynamic_cast<TownRoom *>(getRoomClass(kTown))->getScream();
			if (sc.empty())
				none++;
			else if (sc.hasPrefix("Town.Jane_Screaming") && sc.size() == 20)
				seen |= 1 << (sc[19] - '0');
		}
		Common::String s;
		for (int v = 0; v < 10; v++) {
			if (seen & (1 << v))
				s += Common::String::format(" %d", v);
		}
		out += Common::String::format("; 199 visits screaming, screams seen%s; 1 not screaming, no scream %u", s.c_str(), none);
		debugC(3, kDebugScript, "drawbridge check town: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Scooby
