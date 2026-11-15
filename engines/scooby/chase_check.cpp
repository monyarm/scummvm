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

#include "common/algorithm.h"
#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/character.h"
#include "scooby/chase_rooms.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"

namespace Scooby {

void ScoobyEngine::checkChases() {
	// Showdown's chase (WP-035): each branch's run-offs, by exit, by entry,
	// by time and by the oil can; the openings; the placements; and each
	// canned run-off. Phantom has no chase.
	static const char *const kRooms[] = {
		"ROOM_P24_Chase_B1", "ROOM_P25_Chase_B2", "ROOM_P26_Chase_B3", "ROOM_P27_Chase_B4", "ROOM_P28_Chase_B5",
		"ROOM_P29_Chase_B6", "ROOM_P33_Chase_C1", "ROOM_P37_Chase_C2", "ROOM_P38_Chase_C3", "ROOM_P39_Chase_C4",
		"ROOM_P34_Chase_C5", "ROOM_P35_Chase_C6", "ROOM_P25A_Chase_B2_Closeup", "ROOM_P40_Chase_Start"
	};
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kRooms[0];
	if (!found) {
		debugC(3, kDebugScript, "chase check: no chase rooms");
		return;
	}
	_checking = true;
	newGameForCheck();
	const char *const kStart = "ROOM_P40_Chase_Start";

	// Which class the factory builds.
	Common::String classes;
	for (uint i = 0; i < ARRAYSIZE(kRooms); i++) {
		RoomClass *c = createRoomClass(*this, kRooms[i]);
		classes += Common::String::format("%s %s; ", kRooms[i] + 5,
		                                  dynamic_cast<ChaseBRoom *>(c) ? "B" : dynamic_cast<ChaseCRoom *>(c) ? "C" :
		                                  dynamic_cast<CloseupRoom *>(c) ? "close-up" : c ? "other" : "-");
		delete c;
	}
	debugC(3, kDebugScript, "chase check classes: %s", classes.c_str());

	struct Fresh {
		ScoobyEngine &e;
		// The room's class as the factory builds it, then the room.
		void room(const Common::String &name) {
			RoomClass *c;
			if (e._roomClasses.tryGetVal(name, c)) {
				if (e._roomClass == c)
					e._roomClass = nullptr;
				e._script->setHooks(nullptr);
				delete c;
				e._roomClasses.erase(name);
			}
			e.showRoom(name);
		}
	} fresh = { *this };
	struct Find {
		static int sprite(ScoobyEngine &e, const Common::String &object) {
			for (uint s = 0; s < e._currentRoom->getSpriteCount(); s++) {
				if (e._currentRoom->getObjectName(s) == object)
					return s;
			}
			return -1;
		}
		static Common::String placed(ScoobyEngine &e, const char *prefix) {
			Room &r = *e._currentRoom;
			for (uint s = 0; s < r.getSpriteCount(); s++) {
				if (r.getObjectName(s).hasPrefix(prefix))
					return Common::String::format("%s %s depth %d mode %d hit %d hidden %d%s", r.getObjectName(s).c_str(), r.getAnim(s).c_str(),
					                              r.getDepth(s), r.getMode(s), r.getHitType(s), r.isHidden(s) ? 1 : 0,
					                              r.isRequiresObject(s) ? " requires" : "");
			}
			return Common::String::format("%s none", prefix);
		}
	};
	// Tick until the room changes, skipping scenes as they start; the tick
	// it changed on, or -1.
	struct Run {
		ScoobyEngine &e;
		Common::String next;
		int movie;
		int oilCanTick;
		int can;
		bool canHidden;
		int change(uint max) {
			oilCanTick = -1;
			canHidden = false;
			for (uint t = 1; t <= max; t++) {
				next.clear();
				movie = -1;
				const bool changed = e.tickOnce(next, movie);
				if (oilCanTick < 0 && e._inventory->hasItem("OBJ_INV_OILCAN")) {
					oilCanTick = t;
					canHidden = can >= 0 && e._currentRoom->isHidden(can);
				}
				if (changed)
					return t;
				if (e._script->isScenePlaying())
					e._script->skip();
			}
			return -1;
		}
	} run = { *this, Common::String(), -1, -1, -1, false };
	const uint kMax = 4000;

	// A run-off, started by @p how; then the ticks until the room changes.
	struct Branch {
		static Common::String line(ScoobyEngine &e) {
			ChaseBRoom *b = dynamic_cast<ChaseBRoom *>(e._roomClass);
			if (!b)
				return "no class";
			const int s = b->getBranchSprite();
			Room &r = *e._currentRoom;
			return Common::String::format("%s %s steps %u, %s, pending %s, held %d", s >= 0 ? r.getObjectName(s).c_str() : "-",
			                              s >= 0 ? r.getAnim(s).c_str() : "-", s >= 0 ? r.getStepCount(s) : 0,
			                              b->getLastSound().empty() ? "no sound" : b->getLastSound().c_str(),
			                              b->getPending().empty() ? "-" : b->getPending().c_str(), b->isBusy() ? 1 : 0);
		}
	};
	auto report = [&](const Common::String &room, const Common::String &how) {
		const Common::String before = Branch::line(*this);
		const int t = run.change(kMax);
		debugC(3, kDebugScript, "chase check %s by %s: %s; next %s movie %d at tick %d", room.c_str() + 5, how.c_str(), before.c_str(),
		       run.next.c_str(), run.movie, t);
	};

	// The openings, over 40 visits each.
	struct Openings {
		static Common::String seen(ScoobyEngine &e, Fresh &fresh, const char *from, const char *room) {
			Common::Array<Common::String> seen;
			int played = 0;
			for (uint k = 0; k < 40; k++) {
				fresh.room(from);
				fresh.room(room);
				const Common::String &o = e._script->getOpening();
				bool known = false;
				for (uint i = 0; i < seen.size(); i++)
					known = known || seen[i] == o;
				if (!known)
					seen.push_back(o);
				played += e._script->getMemory(room).openingPlayed ? 1 : 0;
			}
			Common::sort(seen.begin(), seen.end());
			Common::String out;
			for (uint i = 0; i < seen.size(); i++)
				out += Common::String::format("'%s' ", seen[i].c_str());
			return out + Common::String::format("played %d", played);
		}
	};
	static const char *const kBRooms[] = { "ROOM_P24_Chase_B1", "ROOM_P25_Chase_B2", "ROOM_P26_Chase_B3",
	                                       "ROOM_P27_Chase_B4", "ROOM_P28_Chase_B5", "ROOM_P29_Chase_B6" };
	for (uint i = 0; i < ARRAYSIZE(kBRooms); i++)
		debugC(3, kDebugScript, "chase check %s openings: %s", kBRooms[i] + 5, Openings::seen(*this, fresh, kStart, kBRooms[i]).c_str());
	debugC(3, kDebugScript, "chase check P25_Chase_B2 openings from the close-up: %s",
	       Openings::seen(*this, fresh, "ROOM_P25A_Chase_B2_Closeup", "ROOM_P25_Chase_B2").c_str());
	debugC(3, kDebugScript, "chase check P28_Chase_B5 openings from the corral: %s",
	       Openings::seen(*this, fresh, "ROOM_P30_Horseshoe_Corral", "ROOM_P28_Chase_B5").c_str());

	// The placements the class claims.
	for (uint i = 0; i < ARRAYSIZE(kBRooms); i++) {
		fresh.room(kStart);
		fresh.room(kBRooms[i]);
		debugC(3, kDebugScript, "chase check %s placed: %s; %s; %s", kBRooms[i] + 5, Find::placed(*this, "OBJ_BRANCH").c_str(),
		       Find::placed(*this, "OBJ_INV_OILCAN").c_str(), Find::placed(*this, "OBJ_P25_OVERLAY").c_str());
	}
	// Where the live run clicks (not compared).
	{
		struct Point {
			static Common::String of(ScoobyEngine &e, int sprite) {
				for (int y = 4; y < 480 && sprite >= 0; y += 8) {
					for (int x = 4; x < 640; x += 8) {
						if (e._currentRoom->hitTest(x, y) == sprite)
							return Common::String::format("%d %d", x, y);
					}
				}
				return "-";
			}
		};
		fresh.room(kStart);
		fresh.room("ROOM_P29_Chase_B6");
		const Common::String can = Point::of(*this, Find::sprite(*this, "OBJ_INV_OILCAN"));
		fresh.room(kStart);
		fresh.room("ROOM_P28_Chase_B5");
		debugC(3, kDebugScript, "chase points: OILCAN=%s CORRAL=%s", can.c_str(), Point::of(*this, Find::sprite(*this, "OBJ_P28_TO_P30")).c_str());
	}

	// The close-up's overlay and ICDouble.
	struct Icd {
		static Common::String line(ScoobyEngine &e) {
			Character *c = e._script->getCharacter("ICDOUBLE");
			return c ? Common::String::format("ICDOUBLE hit %d", e._currentRoom->getHitType(c->getSprite())) : Common::String("ICDOUBLE -");
		}
	};
	fresh.room(kStart);
	fresh.room("ROOM_P25_Chase_B2");
	debugC(3, kDebugScript, "chase check P25 overlay, no clue: %s; %s", Find::placed(*this, "OBJ_P25_OVERLAY").c_str(), Icd::line(*this).c_str());
	setGameString("ClueOwners0", "ICDOUBLE");
	fresh.room(kStart);
	fresh.room("ROOM_P25_Chase_B2");
	debugC(3, kDebugScript, "chase check P25 overlay, ICDouble's clue: %s; %s", Find::placed(*this, "OBJ_P25_OVERLAY").c_str(), Icd::line(*this).c_str());
	_inventory->addItem("OBJ_INV_SAFE_COMBO");
	fresh.room(kStart);
	fresh.room("ROOM_P25_Chase_B2");
	debugC(3, kDebugScript, "chase check P25 overlay, the combination: %s; %s", Find::placed(*this, "OBJ_P25_OVERLAY").c_str(), Icd::line(*this).c_str());
	newGameForCheck();

	// Each exit's run-off.
	for (uint i = 0; i < ARRAYSIZE(kBRooms); i++) {
		fresh.room(kStart);
		fresh.room(kBRooms[i]);
		Common::Array<Common::String> exits;
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (!_currentRoom->getDestination(s).empty())
				exits.push_back(_currentRoom->getObjectName(s));
		}
		if (exits.empty()) {
			// B1 and B4 run off as they are entered.
			report(kBRooms[i], "entry");
			continue;
		}
		for (uint k = 0; k < exits.size(); k++) {
			if (k) {
				fresh.room(kStart);
				fresh.room(kBRooms[i]);
			}
			clickExit(Find::sprite(*this, exits[k]));
			// A second click once the run-off has started changes nothing.
			if (k == 0)
				clickExit(Find::sprite(*this, exits[exits.size() - 1]));
			report(kBRooms[i], exits[k]);
		}
		// The countdown.
		fresh.room(kStart);
		fresh.room(kBRooms[i]);
		report(kBRooms[i], "time");
	}

	// Back from the close-up and from the corral.
	fresh.room("ROOM_P25A_Chase_B2_Closeup");
	fresh.room("ROOM_P25_Chase_B2");
	report("ROOM_P25_Chase_B2", "the close-up");
	fresh.room("ROOM_P30_Horseshoe_Corral");
	fresh.room("ROOM_P28_Chase_B5");
	report("ROOM_P28_Chase_B5", "the corral");

	// The oil can: taken, it goes in the toolbar on step 22 of the run-off.
	fresh.room(kStart);
	fresh.room("ROOM_P29_Chase_B6");
	{
		run.can = Find::sprite(*this, "OBJ_INV_OILCAN");
		_script->sendEvent("OBJ_INV_OILCAN", true);
		const Common::String before = Branch::line(*this);
		const int t = run.change(kMax);
		debugC(3, kDebugScript, "chase check P29_Chase_B6 by the oil can: %s; in the toolbar at tick %d, hidden %d; next %s movie %d at tick %d",
		       before.c_str(), run.oilCanTick, run.canHidden ? 1 : 0, run.next.c_str(), run.movie, t);
		run.can = -1;
	}
	fresh.room(kStart);
	fresh.room("ROOM_P29_Chase_B6");
	debugC(3, kDebugScript, "chase check P29_Chase_B6 with the oil can: %s", Find::placed(*this, "OBJ_INV_OILCAN").c_str());
	clickExit(Find::sprite(*this, "OBJ_P29_TO_P34"));
	report("ROOM_P29_Chase_B6", "OBJ_P29_TO_P34 with the oil can");
	newGameForCheck();

	// The canned run-offs.
	static const char *const kCRooms[] = { "ROOM_P33_Chase_C1", "ROOM_P37_Chase_C2", "ROOM_P38_Chase_C3",
	                                       "ROOM_P39_Chase_C4", "ROOM_P34_Chase_C5", "ROOM_P35_Chase_C6" };
	for (uint i = 0; i < ARRAYSIZE(kCRooms); i++) {
		fresh.room(kStart);
		fresh.room(kCRooms[i]);
		ChaseCRoom *c = dynamic_cast<ChaseCRoom *>(_roomClass);
		const Common::String placed = Find::placed(*this, "OBJ_CANNED");
		const int s = c ? c->getCannedSprite() : -1;
		const Common::String head = Common::String::format("%s, %s steps %u, held %d", c ? c->getLastSound().c_str() : "no class", placed.c_str(),
		                                                   s >= 0 ? _currentRoom->getStepCount(s) : 0, c && c->isBusy() ? 1 : 0);
		int over = -1;
		const int t = [&]() {
			for (uint k = 1; k <= kMax; k++) {
				run.next.clear();
				run.movie = -1;
				const bool changed = tickOnce(run.next, run.movie);
				if (over < 0 && s >= 0 && _currentRoom->getMode(s) == 0)
					over = k;
				if (changed)
					return (int)k;
			}
			return -1;
		}();
		debugC(3, kDebugScript, "chase check %s: %s; over at tick %d; next %s movie %d at tick %d", kCRooms[i] + 5, head.c_str(), over,
		       run.next.c_str(), run.movie, t);
	}
	newGameForCheck();

	// The close-up (WP-036).
	const char *const kCloseupRoom = "ROOM_P25A_Chase_B2_Closeup";
	const char *const kCombo = "OBJ_INV_SAFE_COMBO";
	auto closeup = [&]() { return dynamic_cast<CloseupRoom *>(_roomClass); };
	auto icdSprite = [&]() {
		Character *c = _script->getCharacter("ICDOUBLE");
		return c ? c->getSprite() : -1;
	};
	auto comboLine = [&]() {
		const int s = Find::sprite(*this, kCombo);
		if (s < 0)
			return Common::String("combination none");
		return Common::String::format("combination %s depth %d mode %d step %u of %u hidden %d%s", _currentRoom->getAnim(s).c_str(),
		                              _currentRoom->getDepth(s), _currentRoom->getMode(s), _currentRoom->getStep(s), _currentRoom->getStepCount(s),
		                              _currentRoom->isHidden(s) ? 1 : 0, _currentRoom->isRequiresObject(s) ? " requires" : "");
	};
	// One tick; whether the room changed.
	auto tick1 = [&]() {
		run.next.clear();
		run.movie = -1;
		return tickOnce(run.next, run.movie);
	};
	// The state a visit starts from: ICDouble met or not, the artifact built
	// or not, his clue given or not.
	auto setup = [&](bool met, bool built, bool owner) {
		newGameForCheck();
		if (met)
			_inventory->meetSuspect("ICDOUBLE");
		if (built)
			setGameInt("ArtifactsBuilt", 1);
		if (owner) {
			setGameString("CluesFound0", "IC_BINOCULARS");
			setGameString("ClueOwners0", "ICDOUBLE");
		}
		fresh.room(kStart);
		fresh.room(kCloseupRoom);
	};

	// The first visit: his introduction on Big Head, then the artifact, by
	// level and artifact.
	static const char *const kTypes[] = { "Bowl", "Deer", "Vase" };
	for (int level = 0; level < 3; level++) {
		Common::String out;
		for (uint i = 0; i < ARRAYSIZE(kTypes); i++) {
			newGameForCheck();
			setDifficulty(level);
			setGameString("ArtifactType", kTypes[i]);
			fresh.room(kStart);
			fresh.room(kCloseupRoom);
			const bool met = _inventory->hasMetSuspect("ICDOUBLE");
			const int t = run.change(5);
			out += Common::String::format("%s: met %d, next %s at tick %d, %s on %s then %s; ", kTypes[i], met ? 1 : 0, run.next.c_str(), t,
			                              getBigHeadScene().c_str(), getBigHeadBackground().c_str(), getBigHeadNext().c_str());
		}
		debugC(3, kDebugScript, "closeup check first visit, level %d: %s", level, out.c_str());
	}
	setDifficulty(0);

	// Met, the artifact not built: the combination waits, hidden; ICDouble
	// clicked goes back to the artifact.
	setup(true, false, false);
	setGameString("ArtifactType", "Deer");
	{
		const bool busy = closeup() && closeup()->isBusy();
		const Common::String placed = comboLine();
		_roomClass->click(icdSprite());
		const int t = run.change(5);
		debugC(3, kDebugScript, "closeup check met, no artifact: %s, held %d; ICDouble clicked: next %s at tick %d", placed.c_str(), busy ? 1 : 0,
		       run.next.c_str(), t);
	}

	// His clue: a 260C line (rand() % 3) as the visit starts, then he gives,
	// then the gang thanks him and goes to the suspects and clues.
	{
		Common::Array<Common::String> seen;
		for (uint k = 0; k < 40; k++) {
			setup(true, true, false);
			const Common::String s = closeup() ? closeup()->getLastScene() : Common::String("no class");
			bool known = false;
			for (uint i = 0; i < seen.size(); i++)
				known = known || seen[i] == s;
			if (!known)
				seen.push_back(s);
		}
		Common::sort(seen.begin(), seen.end());
		Common::String scenes;
		for (uint i = 0; i < seen.size(); i++)
			scenes += "'" + seen[i] + "' ";
		setup(true, true, false);
		// His own three, or a red herring about neither him nor the villain
		// when he is the villain (0x42F990): which, from rand().
		const Common::String villain = getGameString("Whodunit");
		const Common::String clue = getGameString("CluesFound0");
		const bool fits = villain == "ICDOUBLE" ? clue.hasPrefix("RH_") && !clue.hasPrefix("RH_IC_")
		                                        : clue == "IC_BINOCULARS" || clue == "IC_EYE_DOCTOR" || clue == "IC_MAP";
		const bool held = _script->isBusy();
		_script->skip();
		int gives = -1, thanks = -1, t = -1;
		Common::String givesAnim;
		for (uint k = 1; k <= kMax && t < 0; k++) {
			const bool changed = tick1();
			Character *c = _script->getCharacter("ICDOUBLE");
			if (gives < 0 && c && c->getState() == Character::kStateAct) {
				gives = k;
				givesAnim = Common::String::format("%s steps %u, held %d", _currentRoom->getAnim(c->getSprite()).c_str(),
				                                   _currentRoom->getStepCount(c->getSprite()), _roomClass->isBusy() ? 1 : 0);
			}
			if (thanks < 0 && _script->isScenePlaying() && _script->getScene() == "Global.Thank_You_")
				thanks = k;
			if (changed)
				t = k;
			if (_script->isScenePlaying())
				_script->skip();
		}
		debugC(3, kDebugScript, "closeup check his clue: lines %s; a clue from %s, fits %d, owner %d, scene held %d; gives at tick %d (%s); thanks at tick %d; next %s",
		       scenes.c_str(), getGameString("ClueOwners0").c_str(), fits ? 1 : 0, isClueOwner("ICDOUBLE") ? 1 : 0,
		       held ? 1 : 0, gives, givesAnim.c_str(), thanks, run.next.c_str());
	}

	// His clue given, the artifact built: his lines (rand() & 3), not held.
	setup(true, true, true);
	{
		Common::Array<Common::String> seen;
		for (uint k = 0; k < 40; k++) {
			_roomClass->click(icdSprite());
			const Common::String s = closeup() ? closeup()->getLastScene() : Common::String("no class");
			bool known = false;
			for (uint i = 0; i < seen.size(); i++)
				known = known || seen[i] == s;
			if (!known)
				seen.push_back(s);
			_script->skip();
		}
		Common::sort(seen.begin(), seen.end());
		Common::String scenes;
		for (uint i = 0; i < seen.size(); i++)
			scenes += "'" + seen[i] + "' ";
		debugC(3, kDebugScript, "closeup check ICDouble clicked after his clue: %sheld %d", scenes.c_str(), _script->isBusy() ? 1 : 0);
	}

	// The combination blows in: held from entry; 25 ticks on, from behind
	// Shaggy, in front of him on step 38; input free when it stops.
	setup(true, true, true);
	{
		const Common::String placed = comboLine();
		const bool held = closeup() && closeup()->isBusy();
		Character *shaggy = _script->getCharacter("SHAGGY");
		const int sd = shaggy ? _currentRoom->getDepth(shaggy->getSprite()) : 0;
		const int s = Find::sprite(*this, kCombo);
		int blown = -1, front = -1, over = -1, released = -1, behind = 0;
		for (uint k = 1; k <= 400 && s >= 0; k++) {
			tick1();
			if (blown < 0 && closeup()->hasComboBlownIn()) {
				blown = k;
				behind = _currentRoom->getDepth(s) - sd;
			}
			if (front < 0 && blown > 0 && _currentRoom->getDepth(s) == sd + 1)
				front = k;
			if (over < 0 && blown > 0 && _currentRoom->getMode(s) == 0)
				over = k;
			if (released < 0 && blown > 0 && !closeup()->isBusy())
				released = k;
		}
		Common::INIFile ini;
		_roomClass->saveKeys(ini);
		Common::String v;
		ini.getKey("ComboBlewOn", kCloseupRoom, v);
		debugC(3, kDebugScript, "closeup check the combination: %s, held %d; blows in at tick %d (%s, Shaggy %+d), in front at tick %d, over at tick %d, free at tick %d; ComboBlewOn=%s",
		       placed.c_str(), held ? 1 : 0, blown, closeup()->getLastSound().c_str(), behind, front, over, released, v.c_str());

		// Back again: it is there at once, input free.
		showRoom(kStart);
		showRoom(kCloseupRoom);
		debugC(3, kDebugScript, "closeup check back with it blown in: %s, held %d", comboLine().c_str(), closeup()->isBusy() ? 1 : 0);
		// Where the live run clicks (not compared).
		{
			struct Point {
				static Common::String of(ScoobyEngine &e, int sprite) {
					for (int y = 4; y < 480 && sprite >= 0; y += 8) {
						for (int x = 4; x < 640; x += 8) {
							if (e._currentRoom->hitTest(x, y) == sprite)
								return Common::String::format("%d %d", x, y);
						}
					}
					return "-";
				}
			};
			debugC(3, kDebugScript, "closeup points: COMBO=%s ICDOUBLE=%s", Point::of(*this, Find::sprite(*this, kCombo)).c_str(),
			       Point::of(*this, icdSprite()).c_str());
		}

		// Clicked: its scene, not held; then Shaggy takes it and it goes in
		// the toolbar.
		const int combo = Find::sprite(*this, kCombo);
		_script->sendEvent(kCombo, false);
		const Common::String scene = closeup()->getLastScene();
		const bool sceneHeld = _script->isBusy() || closeup()->isBusy();
		_script->skip();
		int takes = -1, toolbar = -1;
		Common::String takeAnim;
		bool hidden = false, heldTaking = false;
		for (uint k = 1; k <= kMax && toolbar < 0; k++) {
			tick1();
			Character *sh = _script->getCharacter("SHAGGY");
			if (takes < 0 && sh && sh->getState() == Character::kStateSpecial) {
				takes = k;
				takeAnim = Common::String::format("%s steps %u", _currentRoom->getAnim(sh->getSprite()).c_str(), _currentRoom->getStepCount(sh->getSprite()));
				hidden = combo >= 0 && _currentRoom->isHidden(combo);
				heldTaking = closeup()->isBusy();
			}
			if (_inventory->hasItem(kCombo))
				toolbar = k;
		}
		debugC(3, kDebugScript, "closeup check the combination clicked: %s, held %d; Shaggy takes it at tick %d (%s), it hidden %d, held %d; in the toolbar at tick %d, held %d",
		       scene.c_str(), sceneHeld ? 1 : 0, takes, takeAnim.c_str(), hidden ? 1 : 0, heldTaking ? 1 : 0, toolbar, closeup()->isBusy() ? 1 : 0);
		showRoom(kStart);
		showRoom(kCloseupRoom);
		debugC(3, kDebugScript, "closeup check back with it held: %s", comboLine().c_str());
	}

	// Leaving while Shaggy takes it: it goes in the toolbar anyway.
	setup(true, true, true);
	for (uint k = 0; k < 200; k++)
		tick1();
	_script->sendEvent(kCombo, false);
	_script->skip();
	for (uint k = 0; k < 2 && !_inventory->hasItem(kCombo); k++)
		tick1();
	{
		const bool before = _inventory->hasItem(kCombo);
		showRoom(kStart);
		debugC(3, kDebugScript, "closeup check left while Shaggy takes it: held before %d, after %d", before ? 1 : 0, _inventory->hasItem(kCombo) ? 1 : 0);
	}

	// The keys, and a new game's reset.
	{
		CloseupRoom *c = dynamic_cast<CloseupRoom *>(createRoomClass(*this, kCloseupRoom));
		Common::INIFile ini;
		ini.setKey("ComboBlewOn", kCloseupRoom, "Yes");
		c->loadKeys(ini);
		const bool yes = c->hasComboBlownIn();
		c->reset();
		const bool reset = c->hasComboBlownIn();
		Common::INIFile none;
		c->loadKeys(none);
		debugC(3, kDebugScript, "closeup check keys: ComboBlewOn=Yes reads %d, reset %d, missing %d", yes ? 1 : 0, reset ? 1 : 0, c->hasComboBlownIn() ? 1 : 0);
		delete c;
	}
	newGameForCheck();
	_checking = false;
	debugC(3, kDebugScript, "chase check: done");
}

} // End of namespace Scooby
