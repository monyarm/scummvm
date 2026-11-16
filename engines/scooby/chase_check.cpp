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
#include "common/config-manager.h"
#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/boot_hill_rooms.h"
#include "scooby/character.h"
#include "scooby/chase_rooms.h"
#include "scooby/detection.h"
#include "scooby/hotel_rooms.h"
#include "scooby/jail_rooms.h"
#include "scooby/office_rooms.h"
#include "scooby/alley_rooms.h"
#include "scooby/stage_rooms.h"
#include "scooby/store_rooms.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"
#include "scooby/tunnel_rooms.h"
#include "scooby/vault_rooms.h"

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

	// The artifact puzzle (WP-037).
	static const char *const kArtTypes[] = { "Bowl", "Deer", "Vase" };
	static const char *const kArtSizes[] = { "09", "12", "15" };
	{
		Common::String artClasses;
		for (uint t = 0; t < 3; t++) {
			for (uint s = 0; s < 3; s++) {
				const Common::String name = Common::String::format("ROOM_Artifact_%s_%s", kArtTypes[t], kArtSizes[s]);
				RoomClass *c = createRoomClass(*this, name);
				artClasses += Common::String::format("%s %s; ", name.c_str() + 14, dynamic_cast<ArtifactRoom *>(c) ? "artifact" : c ? "other" : "-");
				delete c;
			}
		}
		debugC(3, kDebugScript, "artifact check classes: %s", artClasses.c_str());
	}
	auto art = [&]() { return dynamic_cast<ArtifactRoom *>(_roomClass); };
	// Each room as it is entered: the pieces in build order, where each
	// starts and where it goes; the outline; the artifact.
	for (uint t = 0; t < 3; t++) {
		for (uint s = 0; s < 3; s++) {
			const Common::String name = Common::String::format("ROOM_Artifact_%s_%s", kArtTypes[t], kArtSizes[s]);
			fresh.room(kCloseupRoom);
			fresh.room(name);
			ArtifactRoom *a = art();
			Common::String out;
			for (uint i = 0; a && i < a->getPieceCount(); i++) {
				const int sp = a->getPieceSprite(i);
				out += Common::String::format("%s (%d,%d)->(%d,%d) d%d h%d; ", _currentRoom->getAnim(sp).c_str() + 14, a->getPiecePos(i).x, a->getPiecePos(i).y,
				                              a->getPieceTarget(i).x, a->getPieceTarget(i).y, _currentRoom->getDepth(sp), _currentRoom->getHitType(sp));
			}
			Common::String outline = "none";
			for (uint k = 0; k < _currentRoom->getSpriteCount(); k++) {
				if (_currentRoom->getAnim(k).hasSuffix("COMPLETE"))
					outline = Common::String::format("%s d%d", _currentRoom->getAnim(k).c_str(), _currentRoom->getDepth(k));
			}
			const int m = a ? a->getMorphSprite() : -1;
			debugC(3, kDebugScript, "artifact check %s: %u pieces: %soutline %s; artifact %s d%d hidden %d steps %u; %s",
			       name.c_str() + 14, a ? a->getPieceCount() : 0, out.c_str(), outline.c_str(), m >= 0 ? _currentRoom->getAnim(m).c_str() : "none",
			       m >= 0 ? _currentRoom->getDepth(m) : 0, m >= 0 && _currentRoom->isHidden(m) ? 1 : 0, m >= 0 ? _currentRoom->getStepCount(m) : 0,
			       a ? a->getLastSound().c_str() : "");
		}
	}

	// Playing Deer 09 with the mouse: the hover, a pick-up, the carry, the
	// snap, a drop, a piece carried exactly onto its place, then all of them.
	{
		struct Mouse {
			ScoobyEngine &e;
			void to(const Common::Point &p) {
				e._mousePos = p;
				e.updateHover(p.x, p.y);
			}
			// A point on @p sprite's own pixels, or (-1, -1).
			Common::Point on(int sprite) {
				for (int y = 0; y < 480; y += 2) {
					for (int x = 0; x < 640; x += 2) {
						if (e._currentRoom->hitTest(x, y) == sprite)
							return Common::Point(x, y);
					}
				}
				return Common::Point(-1, -1);
			}
			void click() {
				if (e._roomClass)
					e._roomClass->click(e._hover);
			}
		} mouse = { *this };
		newGameForCheck();
		fresh.room(kCloseupRoom);
		fresh.room("ROOM_Artifact_Deer_09");
		ArtifactRoom *a = art();
		// Where the live run picks each piece up and lets it go 3 and 3 from
		// its place (not compared).
		{
			Common::String pts;
			for (uint i = 0; i < a->getPieceCount(); i++) {
				const Common::Point on = mouse.on(a->getPieceSprite(i));
				const Common::Point to = a->getPieceTarget(i) - (a->getPiecePos(i) - on) + Common::Point(3, 3);
				pts += Common::String::format("%d %d>%d %d|", on.x, on.y, to.x, to.y);
			}
			debugC(3, kDebugScript, "artifact points: %s", pts.c_str());
		}
		auto piece = [&](uint i) {
			const int sp = a->getPieceSprite(i);
			return Common::String::format("piece %u at (%d,%d) d%d placed %d", i, a->getPiecePos(i).x, a->getPiecePos(i).y, _currentRoom->getDepth(sp),
			                              a->isPlaced(i) ? 1 : 0);
		};
		// The hover: the open hand over a loose piece.
		const Common::Point on0 = mouse.on(a->getPieceSprite(0));
		mouse.to(on0);
		tick1();
		updateHover(on0.x, on0.y);
		const Common::String hoverCursor = _inventory->getCursor();
		mouse.to(Common::Point(600, 20));
		tick1();
		const Common::String awayCursor = _inventory->getCursor();
		// Picked up, then carried 15 to the right of its place, then 5 and 5
		// from it, then snapped in.
		const Common::Point grab = a->getPiecePos(0) - on0;
		mouse.to(on0);
		mouse.click();
		tick1();
		const Common::String up = Common::String::format("%s, carried %d, cursor hidden %d", piece(0).c_str(), a->getDragging(),
		                                                 a->hidesCursor() ? 1 : 0);
		mouse.to(a->getPieceTarget(0) - grab + Common::Point(15, 0));
		tick1();
		const Common::String far = piece(0);
		mouse.to(a->getPieceTarget(0) - grab + Common::Point(5, 5));
		tick1();
		const Common::String close = piece(0);
		tick1();
		debugC(3, kDebugScript, "artifact check carry: cursor over a loose piece %s, away %s; picked up: %s; 15 off: %s; 5 and 5 off: %s; next tick: %s, carried %d, %s, ArtifactsBuilt %d",
		       hoverCursor.c_str(), awayCursor.c_str(), up.c_str(), far.c_str(), close.c_str(), piece(0).c_str(), a->getDragging(),
		       a->getLastSound().hasPrefix("Artifact.Clink") ? "a clink" : a->getLastSound().c_str(), getGameInt("ArtifactsBuilt"));
		// Over a piece in its place: the arrow, and a click does nothing.
		const Common::Point on0b = mouse.on(a->getPieceSprite(0));
		mouse.to(on0b);
		tick1();
		mouse.click();
		tick1();
		const Common::String placedLine = Common::String::format("cursor %s, carried %d", _inventory->getCursor().c_str(), a->getDragging());
		// Dropped anywhere else: back to its start.
		const Common::Point on1 = mouse.on(a->getPieceSprite(1));
		mouse.to(on1);
		mouse.click();
		tick1();
		mouse.to(Common::Point(300, 400));
		tick1();
		mouse.click();
		tick1();
		debugC(3, kDebugScript, "artifact check placed and dropped: over the placed piece %s; piece 1 dropped: %s, carried %d, %s",
		       placedLine.c_str(), piece(1).c_str(), a->getDragging(), a->getLastSound().c_str());
		// Carried exactly onto its place: it stays carried, and a click then
		// drops it back to its start.
		const Common::Point on2 = mouse.on(a->getPieceSprite(2));
		mouse.to(on2);
		mouse.click();
		tick1();
		const Common::Point grab2 = a->getPiecePos(2) - on2;
		mouse.to(a->getPieceTarget(2) - grab2);
		tick1();
		tick1();
		tick1();
		const Common::String exact = Common::String::format("%s, carried %d", piece(2).c_str(), a->getDragging());
		mouse.click();
		tick1();
		debugC(3, kDebugScript, "artifact check exactly on its place: %s; clicked: %s, %s", exact.c_str(), piece(2).c_str(), a->getLastSound().c_str());
		// The rest, each 3 and 3 off: the last builds the artifact; it
		// morphs, and with its sound over the gang goes back to the close-up.
		int built = -1;
		for (uint i = 1; i < a->getPieceCount(); i++) {
			const Common::Point on = mouse.on(a->getPieceSprite(i));
			mouse.to(on);
			mouse.click();
			tick1();
			const Common::Point g = a->getPiecePos(i) - on;
			mouse.to(a->getPieceTarget(i) - g + Common::Point(3, 3));
			tick1();
			tick1();
			if (a->isSolved() && built < 0)
				built = i;
		}
		const int m = a->getMorphSprite();
		const Common::String morph = Common::String::format("built on piece %d, ArtifactsBuilt %d, %s, artifact hidden %d mode %d", built, getGameInt("ArtifactsBuilt"),
		                                                    a->getLastSound().c_str(), m >= 0 && _currentRoom->isHidden(m) ? 1 : 0, m >= 0 ? _currentRoom->getMode(m) : -1);
		Common::INIFile ini;
		_roomClass->saveKeys(ini);
		Common::String keys;
		for (uint i = 0; i < ArtifactRoom::kMaxPieces; i++) {
			Common::String v;
			ini.getKey(Common::String::format("PiecePlaced%u", i), "ROOM_Artifact_Deer_09", v);
			keys += v == "Yes" ? "Y" : v == "No" ? "N" : "-";
		}
		_sound->stopClip("Artifact.Morph");
		int over = -1, t = -1;
		for (uint k = 1; k <= 200 && t < 0; k++) {
			const bool changed = tick1();
			if (over < 0 && m >= 0 && _currentRoom->getMode(m) == 0)
				over = k;
			if (changed)
				t = k;
		}
		debugC(3, kDebugScript, "artifact check built: %s; PiecePlaced %s; artifact over at tick %d; next %s at tick %d", morph.c_str(), keys.c_str(), over,
		       run.next.c_str(), t);
		// Back again: every piece in its place.
		showRoom(kCloseupRoom);
		showRoom("ROOM_Artifact_Deer_09");
		a = art();
		uint in = 0;
		for (uint i = 0; a && i < a->getPieceCount(); i++)
			in += a->getPiecePos(i) == a->getPieceTarget(i) && _currentRoom->getDepth(a->getPieceSprite(i)) == 1 ? 1 : 0;
		debugC(3, kDebugScript, "artifact check back: %u of %u in place", in, a ? a->getPieceCount() : 0);
		// The keys read back, and a new game's reset.
		ArtifactRoom *c = dynamic_cast<ArtifactRoom *>(createRoomClass(*this, "ROOM_Artifact_Deer_09"));
		c->loadKeys(ini);
		uint read = 0;
		for (uint i = 0; i < ArtifactRoom::kMaxPieces; i++)
			read += c->isPlaced(i) ? 1 : 0;
		c->reset();
		uint after = 0;
		for (uint i = 0; i < ArtifactRoom::kMaxPieces; i++)
			after += c->isPlaced(i) ? 1 : 0;
		debugC(3, kDebugScript, "artifact check keys: %u placed read back, %u after a reset", read, after);
		delete c;
	}
	newGameForCheck();

	// The vault and the open safe (WP-038).
	{
		const char *const kBank = "ROOM_P13_Bank";
		const char *const kVault = "ROOM_P16_Bank_Vault";
		const char *const kSafe = "ROOM_Open_Safe";
		{
			RoomClass *a = createRoomClass(*this, kVault);
			RoomClass *b = createRoomClass(*this, kSafe);
			debugC(3, kDebugScript, "vault check classes: P16_Bank_Vault %s; Open_Safe %s", dynamic_cast<BankVaultRoom *>(a) ? "vault" : a ? "other" : "-",
			       dynamic_cast<OpenSafeRoom *>(b) ? "open safe" : b ? "other" : "-");
			delete a;
			delete b;
		}
		auto vault = [&]() { return dynamic_cast<BankVaultRoom *>(_roomClass); };
		auto placed = [&](const char *object) {
			const int s = Find::sprite(*this, object);
			if (s < 0)
				return Common::String::format("%s none", object + 4);
			// The depth only where the class claims it (the door).
			const Common::String depth = !strcmp(object, "OBJ_P16_DOOR") ? Common::String::format(" depth %d", _currentRoom->getDepth(s)) : Common::String();
			return Common::String::format("%s %s mode %d step %u%s hidden %d%s%s", object + 4, _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
			                              _currentRoom->getStep(s), depth.c_str(), _currentRoom->isHidden(s) ? 1 : 0,
			                              _currentRoom->isRequiresObject(s) ? " requires" : "", _currentRoom->isClickable(s) ? " clickable" : "");
		};
		auto boxes = [&]() {
			Common::String out;
			for (char k = '1'; k <= '5'; k++)
				out += placed(Common::String::format("OBJ_P16_BANK_VAULT_HOTSPOT%c", k).c_str()) + "; ";
			return out;
		};
		auto useItem = [&](const char *item) {
			_inventory->addItem(item);
			_inventory->removeItem(item);
		};
		// The placements: a new game; the combination used; the key used,
		// and the dentures it brings, once.
		newGameForCheck();
		fresh.room(kBank);
		fresh.room(kVault);
		debugC(3, kDebugScript, "vault check placed: %s; %s%s", placed("OBJ_P16_DOOR").c_str(), boxes().c_str(), placed("OBJ_P16_BANK_VAULT_HOTSPOT6").c_str());
		{
			// Where the live run clicks (not compared).
			auto at = [&](const char *object) {
				const int s = Find::sprite(*this, object);
				for (int y = 4; y < 400 && s >= 0; y += 4) {
					for (int x = 4; x < 640; x += 4) {
						if (_currentRoom->hitTest(x, y) == s)
							return Common::String::format("%d %d", x, y);
					}
				}
				return Common::String("-");
			};
			debugC(3, kDebugScript, "vault points: DOOR=%s BOX1=%s DUST=%s", at("OBJ_P16_DOOR").c_str(), at("OBJ_P16_BANK_VAULT_HOTSPOT1").c_str(),
			       at("OBJ_P16_BANK_VAULT_HOTSPOT6").c_str());
		}
		useItem("OBJ_INV_SAFE_COMBO");
		useItem("OBJ_INV_STRONGBOXKEY");
		fresh.room(kBank);
		fresh.room(kVault);
		const bool dentures = _inventory->hasItem("OBJ_INV_DENTURES");
		_inventory->removeItem("OBJ_INV_DENTURES");
		showRoom(kBank);
		showRoom(kVault);
		debugC(3, kDebugScript, "vault check both used: %s; %sdentures %d, again %d", placed("OBJ_P16_DOOR").c_str(), boxes().c_str(), dentures ? 1 : 0,
		       _inventory->hasItem("OBJ_INV_DENTURES") && _inventory->getSlot(0) == "OBJ_INV_DENTURES" ? 1 : 0);

		// The safe: Daphne tries it, with the arrow and with the combination.
		struct Daphne {
			static bool trying(ScoobyEngine &e) {
				Character *d = e._script->getCharacter("DAPHNE");
				return d && d->getState() == Character::kStateSpecial;
			}
		};
		for (int withCombo = 0; withCombo < 2; withCombo++) {
			newGameForCheck();
			if (withCombo)
				_inventory->addItem("OBJ_INV_SAFE_COMBO", "ANIM_TOOLBARSAFE_COMBO", "ANIM_CURSORSAFE_COMBO");
			fresh.room(kBank);
			fresh.room(kVault);
			_script->sendEvent("OBJ_P16_DOOR", withCombo != 0);
			const bool held = vault()->isBusy();
			const bool usedNow = withCombo && _inventory->hasItem("OBJ_INV_SAFE_COMBO") && _inventory->getSlot(0) != "OBJ_INV_SAFE_COMBO";
			int tries = -1, after = -1, t = -1;
			Common::String anim, doorThen, afterLine;
			const int door = vault()->getDoorSprite();
			for (uint k = 1; k <= kMax && t < 0 && after < 0; k++) {
				const bool changed = tick1();
				if (tries < 0 && Daphne::trying(*this)) {
					tries = k;
					Character *d = _script->getCharacter("DAPHNE");
					anim = Common::String::format("%s steps %u, %s, door hidden %d, held %d", _currentRoom->getAnim(d->getSprite()).c_str(),
					                              _currentRoom->getStepCount(d->getSprite()), vault()->getLastSound().c_str(),
					                              door >= 0 && _currentRoom->isHidden(door) ? 1 : 0, vault()->isBusy() ? 1 : 0);
				}
				if (changed) {
					t = k;
				} else if (tries > 0 && !vault()->isDoorClicked()) {
					after = k;
					afterLine = Common::String::format("%s, held %d, scene held %d", _script->isScenePlaying() ? _script->getScene().c_str() : "no scene",
					                                   vault()->isBusy() ? 1 : 0, _script->isBusy() ? 1 : 0);
					doorThen = placed("OBJ_P16_DOOR");
				}
			}
			if (after > 0 && withCombo) {
				for (uint k = after + 1; k <= kMax && t < 0; k++) {
					if (tick1())
						t = k;
				}
			}
			debugC(3, kDebugScript, "vault check the safe %s: held %d, combination used %d; Daphne at tick %d (%s); done at tick %d: %s, %s; next %s at tick %d",
			       withCombo ? "with the combination" : "with the arrow", held ? 1 : 0, usedNow ? 1 : 0, tries, anim.c_str(), after, afterLine.c_str(),
			       doorThen.c_str(), run.next.c_str(), t);
		}

		// The strong boxes: the arrow's two lines; the key in a wrong box
		// (three lines, rand() % 3); the key in the right one: its movie.
		newGameForCheck();
		fresh.room(kBank);
		fresh.room(kVault);
		Common::String arrowLines;
		for (uint k = 0; k < 3; k++) {
			_script->sendEvent("OBJ_P16_BANK_VAULT_HOTSPOT1", false);
			arrowLines += vault()->getLastScene() + Common::String::format(" held %d; ", _script->isBusy() ? 1 : 0);
			_script->skip();
		}
		setGameInt("StrongBox", 2);
		Common::Array<Common::String> wrong;
		_inventory->addItem("OBJ_INV_STRONGBOXKEY", "ANIM_TOOLBARSTRONGBOXKEY", "ANIM_CURSORSTRONG_BOX_KEY");
		for (uint k = 0; k < 40; k++) {
			_script->sendEvent("OBJ_P16_BANK_VAULT_HOTSPOT1", true);
			const Common::String s = vault()->getLastScene();
			bool known = false;
			for (uint i = 0; i < wrong.size(); i++)
				known = known || wrong[i] == s;
			if (!known)
				wrong.push_back(s);
			_script->skip();
		}
		Common::sort(wrong.begin(), wrong.end());
		Common::String wrongLines;
		for (uint i = 0; i < wrong.size(); i++)
			wrongLines += "'" + wrong[i] + "' ";
		_script->sendEvent("OBJ_P16_BANK_VAULT_HOTSPOT3", true);
		const bool keyUsed = _inventory->hasItem("OBJ_INV_STRONGBOXKEY") && _inventory->getSlot(0) != "OBJ_INV_STRONGBOXKEY" &&
		                     _inventory->getSlot(1) != "OBJ_INV_STRONGBOXKEY";
		const int bt = run.change(5);
		const Common::String boxNext = run.next;
		const int boxMovie = run.movie;
		if (!boxNext.empty())
			showRoom(boxNext);
		debugC(3, kDebugScript, "vault check strong boxes: with the arrow %swith the key in box 1 (StrongBox 2): %sin box 3: key used %d, next %s movie %d at tick %d; there, dentures %d; %s",
		       arrowLines.c_str(), wrongLines.c_str(), keyUsed ? 1 : 0, boxNext.c_str(), boxMovie, bt, _inventory->hasItem("OBJ_INV_DENTURES") ? 1 : 0, boxes().c_str());

		// The dust: clicked until its next state is C, then once more.
		newGameForCheck();
		fresh.room(kBank);
		fresh.room(kVault);
		{
			// No ghost in place of a click (1 in 30), for the check.
			_script->setGhostChance(nullptr, 0);
			const int dust = vault()->getDustSprite();
			Common::String clicks;
			for (uint k = 0; k < 3 && dust >= 0; k++) {
				_script->clickClickable(dust);
				tick1();
				clicks += Common::String::format("%s next %s visible %d scene '%s'; ", _currentRoom->getAnim(dust).c_str(), _script->getClickableState(dust).c_str(),
				                                 vault()->isDustVisible() ? 1 : 0, _script->isScenePlaying() ? _script->getScene().c_str() : "");
				_script->skip();
				for (uint w = 0; w < 200 && _script->isClickablePlaying(dust); w++)
					tick1();
			}
			Common::INIFile ini;
			_roomClass->saveKeys(ini);
			Common::String v;
			ini.getKey("DustVisible", kVault, v);
			showRoom(kBank);
			showRoom(kVault);
			debugC(3, kDebugScript, "vault check dust: %sDustVisible=%s; back: %s, next %s", clicks.c_str(), v.c_str(), placed("OBJ_P16_BANK_VAULT_HOTSPOT6").c_str(),
			       _script->getClickableState(vault()->getDustSprite()).c_str());
			BankVaultRoom *c = dynamic_cast<BankVaultRoom *>(createRoomClass(*this, kVault));
			c->loadKeys(ini);
			const bool yes = c->isDustVisible();
			c->reset();
			debugC(3, kDebugScript, "vault check keys: DustVisible=Yes reads %d, reset %d", yes ? 1 : 0, c->isDustVisible() ? 1 : 0);
			delete c;
			_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
		}

		// The open safe: the gang placed; the scene's lines, each speaker's
		// mouth; then the back alley. Seen, it goes straight there. Skipped,
		// likewise.
		newGameForCheck();
		fresh.room(kVault);
		fresh.room(kSafe);
		{
			OpenSafeRoom *o = dynamic_cast<OpenSafeRoom *>(_roomClass);
			Common::String gang;
			static const char *const kNames[] = { "DAPHNE", "FRED", "VELMA", "SHAGGY", "SCOOBY" };
			for (uint i = 0; i < OpenSafeRoom::kGang; i++)
				gang += placed(Common::String::format("OBJ_GANG_VAULT_%s", kNames[i]).c_str()) + Common::String::format(" hit %d; ", o->getGangSprite(i) >= 0 ? _currentRoom->getHitType(o->getGangSprite(i)) : -1);
			const bool hides = o->hidesCursor(), swallows = o->click(-1);
			Common::String lines;
			int first = -1, t = -1;
			for (uint k = 1; k <= 4000 && t < 0; k++) {
				const Common::String before = o->getClip();
				if (tick1()) {
					t = k;
					break;
				}
				if (o->getClip() != before && !o->getClip().empty()) {
					if (first < 0)
						first = k;
					int who = -1;
					for (uint i = 0; i < OpenSafeRoom::kGang; i++)
						who = o->getGangSprite(i) == o->getMouth() ? (int)i : who;
					lines += Common::String::format("%s (%s) ", o->getClip().c_str() + strlen("Open_Safe.340F_"), who >= 0 ? kNames[who] : "-");
					_sound->stopClip(o->getClip());
				}
			}
			debugC(3, kDebugScript, "open safe check: %scursor hidden %d, clicks taken %d; first line at tick %d: %snext %s at tick %d, played %d", gang.c_str(),
			       hides ? 1 : 0, swallows ? 1 : 0, first, lines.c_str(), run.next.c_str(), t, _script->getMemory(kSafe).openingPlayed ? 1 : 0);
			showRoom(kVault);
			showRoom(kSafe);
			const int again = run.change(5);
			debugC(3, kDebugScript, "open safe check seen: next %s at tick %d", run.next.c_str(), again);
			newGameForCheck();
			fresh.room(kVault);
			fresh.room(kSafe);
			o = dynamic_cast<OpenSafeRoom *>(_roomClass);
			for (uint k = 0; k < 100; k++)
				tick1();
			const Common::String playing = o->getClip();
			const bool taken = o->skip();
			const int skipped = run.change(5);
			debugC(3, kDebugScript, "open safe check skipped on tick 100 during %s: taken %d, next %s at tick %d", playing.c_str() + strlen("Open_Safe.340F_"),
			       taken ? 1 : 0, run.next.c_str(), skipped);
		}
	}
	newGameForCheck();

	// The bank and the vault door's clock (WP-039).
	{
		const char *const kBank = "ROOM_P13_Bank";
		const char *const kClockRoom = "ROOM_Vault_Clock";
		const char *const kStreet3 = "ROOM_P11_Street3";
		{
			RoomClass *a = createRoomClass(*this, kBank);
			RoomClass *b = createRoomClass(*this, kClockRoom);
			debugC(3, kDebugScript, "bank check classes: P13_Bank %s; Vault_Clock %s", dynamic_cast<BankRoom *>(a) ? "bank" : a ? "other" : "-",
			       dynamic_cast<VaultClockRoom *>(b) ? "clock" : b ? "other" : "-");
			delete a;
			delete b;
		}
		auto bank = [&]() { return dynamic_cast<BankRoom *>(_roomClass); };
		auto door = [&]() {
			const int s = Find::sprite(*this, "OBJ_P13_DOOR");
			if (s < 0)
				return Common::String("door none");
			return Common::String::format("door %s mode %d step %u of %u depth %d%s", _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
			                              _currentRoom->getStep(s), _currentRoom->getStepCount(s), _currentRoom->getDepth(s),
			                              _currentRoom->isRequiresObject(s) ? " requires" : "");
		};
		auto clockUsedNow = [&]() {
			_inventory->addItem("OBJ_INV_CLOCK");
			_inventory->removeItem("OBJ_INV_CLOCK");
		};
		// The door and the opening: a new game (with moans), without moans,
		// the clock on the door, the vault open.
		newGameForCheck();
		fresh.room(kStreet3);
		fresh.room(kBank);
		const Common::String withMoans = door() + ", opening '" + _script->getOpening() + "'";
		setGameFlag("MoaningActive", false);
		fresh.room(kStreet3);
		fresh.room(kBank);
		const Common::String noMoans = "opening '" + _script->getOpening() + "'";
		clockUsedNow();
		fresh.room(kStreet3);
		fresh.room(kBank);
		const Common::String clockOn = door();
		setGameFlag("VaultOpen", true);
		fresh.room(kStreet3);
		fresh.room(kBank);
		debugC(3, kDebugScript, "bank check placed: %s; without moans %s; the clock on it: %s; the vault open: %s", withMoans.c_str(), noMoans.c_str(),
		       clockOn.c_str(), door().c_str());

		// The door with the arrow: three lines in turn; the third 330C_ with
		// the clock in the toolbar. With the clock on it: to the dial.
		newGameForCheck();
		fresh.room(kStreet3);
		fresh.room(kBank);
		_script->skip();
		Common::String arrow;
		for (uint k = 0; k < 4; k++) {
			_script->sendEvent("OBJ_P13_DOOR", false);
			arrow += bank()->getLastScene() + Common::String::format(" held %d; ", _script->isBusy() ? 1 : 0);
			_script->skip();
		}
		_inventory->addItem("OBJ_INV_CLOCK");
		for (uint k = 0; k < 3; k++) {
			_script->sendEvent("OBJ_P13_DOOR", false);
			arrow += bank()->getLastScene() + "; ";
			_script->skip();
		}
		newGameForCheck();
		clockUsedNow();
		fresh.room(kStreet3);
		fresh.room(kBank);
		_script->skip();
		_script->sendEvent("OBJ_P13_DOOR", false);
		const int toDial = run.change(5);
		debugC(3, kDebugScript, "bank check door with the arrow: %sthe clock on it: next %s at tick %d", arrow.c_str(), run.next.c_str(), toDial);
		{
			fresh.room(kStreet3);
			fresh.room(kBank);
			const int s = Find::sprite(*this, "OBJ_P13_DOOR");
			Common::String at = "-";
			for (int y = 100; y < 400 && s >= 0 && at == "-"; y += 4) {
				for (int x = 4; x < 640 && at == "-"; x += 4) {
					if (_currentRoom->hitTest(x, y) == s)
						at = Common::String::format("%d %d", x + 8, y + 8);
				}
			}
			debugC(3, kDebugScript, "bank points: DOOR=%s", at.c_str());
		}

		// The clock put on the door, by level: the times drawn (rand() % 10)
		// over 80 tries each; the scene names the time stored; the clock
		// used; on to the dial.
		for (int level = 0; level < 3; level++) {
			Common::Array<Common::String> seen;
			bool agree = true, used = true;
			Common::String next;
			for (uint k = 0; k < 80; k++) {
				newGameForCheck();
				setDifficulty(level);
				_inventory->addItem("OBJ_INV_CLOCK", "ANIM_TOOLBARBROKEN_CLOCK", "ANIM_CURSORBROKEN_CLOCK");
				fresh.room(kStreet3);
				fresh.room(kBank);
				_script->skip();
				_script->sendEvent("OBJ_P13_DOOR", true);
				const Common::String scene = bank()->getLastScene();
				const int h = getGameInt("ClockHour"), m = getGameInt("ClockMinute");
				agree = agree && scene.hasSuffix(Common::String::format("_%.2d%.2d_", h, m)) && _script->getScene() == scene;
				used = used && _inventory->hasItem("OBJ_INV_CLOCK") && _inventory->getSlot(0) != "OBJ_INV_CLOCK";
				const Common::String time = Common::String::format("%d:%.2d", h, m);
				bool known = false;
				for (uint i = 0; i < seen.size(); i++)
					known = known || seen[i] == time;
				if (!known)
					seen.push_back(time);
				if (k == 0) {
					run.change(kMax);
					next = run.next;
				}
			}
			Common::sort(seen.begin(), seen.end(), [](const Common::String &a, const Common::String &b) {
				return atoi(a.c_str()) != atoi(b.c_str()) ? atoi(a.c_str()) < atoi(b.c_str()) : a < b;
			});
			Common::String times;
			for (uint i = 0; i < seen.size(); i++)
				times += seen[i] + " ";
			debugC(3, kDebugScript, "bank check the clock on the door, level %d: times %sscene agrees %d, clock used %d; next %s", level, times.c_str(), agree ? 1 : 0,
			       used ? 1 : 0, next.c_str());
		}
		setDifficulty(0);

		// Back from the dial with the vault open: held, and a second later the
		// door swings open with its scene and sound.
		newGameForCheck();
		setGameFlag("MoaningActive", false);
		setGameFlag("VaultOpen", true);
		fresh.room(kClockRoom);
		fresh.room(kBank);
		{
			const Common::String before = door() + Common::String::format(", held %d", bank()->isBusy() ? 1 : 0);
			int opens = -1, over = -1, freed = -1;
			Common::String at;
			const int d = Find::sprite(*this, "OBJ_P13_DOOR");
			for (uint k = 1; k <= 400 && over < 0; k++) {
				tick1();
				if (opens < 0 && !bank()->getLastSound().empty()) {
					opens = k;
					at = Common::String::format("%s, %s, %s", _script->getScene().c_str(), bank()->getLastSound().c_str(), door().c_str());
				}
				if (freed < 0 && !bank()->isBusy())
					freed = k;
				if (opens > 0 && d >= 0 && _currentRoom->getMode(d) == 0)
					over = k;
				if (_script->isScenePlaying())
					_script->skip();
			}
			debugC(3, kDebugScript, "bank check the vault opens: %s; on tick %d: %s; class hold off on tick %d; the door open on tick %d", before.c_str(), opens,
			       at.c_str(), freed, over);
		}

		// The moans: the first a second in, then every 5-14 seconds; four of
		// them. None without moans.
		newGameForCheck();
		fresh.room(kStreet3);
		fresh.room(kBank);
		{
			Common::Array<Common::String> names;
			int first = -1, last = -1, minGap = 100000, maxGap = 0, count = 0;
			for (uint k = 1; k <= 20000 && count < 30; k++) {
				const Common::String before = bank()->getLastSound();
				const int delay = bank()->getMoanDelay();
				tick1();
				if (_script->isScenePlaying())
					_script->skip();
				if (bank()->getMoanDelay() > delay) {
					count++;
					if (first < 0)
						first = k;
					else {
						minGap = MIN<int>(minGap, k - last);
						maxGap = MAX<int>(maxGap, k - last);
					}
					last = k;
					bool known = false;
					for (uint i = 0; i < names.size(); i++)
						known = known || names[i] == bank()->getLastSound();
					if (!known)
						names.push_back(bank()->getLastSound());
				}
			}
			Common::sort(names.begin(), names.end());
			Common::String list;
			for (uint i = 0; i < names.size(); i++)
				list += names[i] + " ";
			setGameFlag("MoaningActive", false);
			fresh.room(kStreet3);
			fresh.room(kBank);
			int none = 0;
			for (uint k = 0; k < 600; k++) {
				tick1();
				none += bank()->getLastSound().hasPrefix("Town_Center.Moan") ? 1 : 0;
			}
			debugC(3, kDebugScript, "bank check moans: first on tick %d; %s; gaps within %d-%d over %d; without moans %d in 600 ticks", first, list.c_str(),
			       minGap >= 181 && maxGap <= 505 ? 181 : minGap, minGap >= 181 && maxGap <= 505 ? 505 : maxGap, count, none);
		}

		// Walking in: Fred from Street 3, Daphne from the vault.
		{
			newGameForCheck();
			auto walks = [&](const char *who) {
				Character *c = _script->getCharacter(who);
				return c ? Common::String::format("%s state %d", who, c->getState()) : Common::String(who) + " none";
			};
			fresh.room(kStreet3);
			fresh.room(kBank);
			const Common::String fromStreet = walks("FRED");
			fresh.room("ROOM_P16_Bank_Vault");
			fresh.room(kBank);
			debugC(3, kDebugScript, "bank check walking in: from Street 3 %s; from the vault %s", fromStreet.c_str(), walks("DAPHNE").c_str());
		}

		// The dial: the hands and the time; where a hand points for the mouse;
		// both hands dragged to the time, a step a tick, then the vault open
		// and back to the bank.
		newGameForCheck();
		setGameInt("ClockHour", 10);
		setGameInt("ClockMinute", 23);
		fresh.room(kBank);
		fresh.room(kClockRoom);
		{
			VaultClockRoom *c = dynamic_cast<VaultClockRoom *>(_roomClass);
			auto hand = [&](const char *object) {
				const int s = Find::sprite(*this, object);
				return s < 0 ? Common::String::format("%s none", object + 16)
				             : Common::String::format("%s %s mode %d step %u of %u hit %d", object + 16, _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
				                                      _currentRoom->getStep(s), _currentRoom->getStepCount(s), _currentRoom->getHitType(s));
			};
			debugC(3, kDebugScript, "clock check placed: %s; %s; %s", hand("OBJ_VAULT_CLOCK_HOUR").c_str(), hand("OBJ_VAULT_CLOCK_MIN").c_str(),
			       hand("OBJ_VAULT_CLOCK_TIME").c_str());
			static const int kPoints[][2] = { { 330, 120 }, { 470, 250 }, { 450, 400 }, { 300, 420 }, { 150, 330 }, { 180, 150 }, { 325, 280 } };
			Common::String targets;
			for (uint i = 0; i < ARRAYSIZE(kPoints); i++) {
				const Common::Point p(kPoints[i][0], kPoints[i][1]);
				targets += Common::String::format("(%d,%d) hour %u minute %u; ", p.x, p.y, VaultClockRoom::getTarget(p, 12), VaultClockRoom::getTarget(p, 60));
			}
			debugC(3, kDebugScript, "clock check pointing: %s", targets.c_str());

			struct Grab {
				static Common::Point on(ScoobyEngine &e, int sprite) {
					for (int y = 100; y < 480; y += 2) {
						for (int x = 100; x < 540; x += 2) {
							if (e._currentRoom->hitTest(x, y) == sprite)
								return Common::Point(x, y);
						}
					}
					return Common::Point(-1, -1);
				}
			};
			// A point the mouse can hold for @p step of a hand of @p steps:
			// 100 pixels out from the dial's center.
			auto toward = [](uint step, uint steps) {
				const double a = 2 * 3.14159265358979323846 * step / steps;
				return Common::Point((int)(319 + 100 * sin(a) + 0.5), (int)(285 - 100 * cos(a) + 0.5));
			};
			auto drag = [&](int sprite, uint target, uint steps) {
				const Common::Point p = Grab::on(*this, sprite);
				// Where the live run grabs it and holds the mouse (not compared).
				debugC(3, kDebugScript, "clock points: %s grab %d %d, hold %d %d", _currentRoom->getObjectName(sprite).c_str(), p.x, p.y, toward(target, steps).x,
				       toward(target, steps).y);
				_mousePos = p;
				updateHover(p.x, p.y);
				_mouseDown = true;
				_roomClass->click(_hover);
				tick1();
				const int caught = c->getDrag();
				_mousePos = toward(target, steps);
				int reached = -1;
				for (uint k = 2; k <= 100 && reached < 0; k++) {
					tick1();
					if (_currentRoom->getStep(sprite) == target && _currentRoom->getMode(sprite) == 0)
						reached = k;
				}
				_mouseDown = false;
				tick1();
				return Common::String::format("caught %d, on step %u on tick %d, let go: drag %d", caught, _currentRoom->getStep(sprite), reached, c->getDrag());
			};
			const int minute = Find::sprite(*this, "OBJ_VAULT_CLOCK_MIN");
			const int hourHand = Find::sprite(*this, "OBJ_VAULT_CLOCK_HOUR");
			const Common::String m = drag(minute, 23, 60);
			const int clicksMinute = c->getClicks();
			const bool openAfterMinute = getGameFlag("VaultOpen");
			run.next.clear();
			const Common::String h = drag(hourHand, 10, 12);
			const int clicks = c->getClicks();
			tick1();
			const Common::String next = run.next;
			debugC(3, kDebugScript, "clock check set: minute %s, %d clicks, vault open %d; hour %s, %d clicks; vault open %d, next %s", m.c_str(), clicksMinute,
			       openAfterMinute ? 1 : 0, h.c_str(), clicks - clicksMinute, getGameFlag("VaultOpen") ? 1 : 0, next.c_str());
		}
	}
	newGameForCheck();

	// Boot Hill (WP-040).
	{
		static const char *const kHills[] = { "ROOM_P21_Boot_Hill1", "ROOM_P22_Boot_Hill2", "ROOM_P23_Boot_Hill3" };
		const char *const kTunnel = "ROOM_P18_Tunnel_Lit";
		auto hill = [&]() { return dynamic_cast<BootHillRoom *>(_roomClass); };
		{
			Common::String hillClasses, graves;
			for (uint i = 0; i < 3; i++) {
				RoomClass *c = createRoomClass(*this, kHills[i]);
				hillClasses += Common::String::format("%s %s; ", kHills[i] + 5, dynamic_cast<BootHillRoom *>(c) ? "boot hill" : c ? "other" : "-");
				delete c;
			}
			for (int g = 0; g < 9; g++)
				graves += Common::String::format("%d %s; ", g, BootHillRoom::getGraveObject(g).c_str() + 4);
			debugC(3, kDebugScript, "boot hill check classes: %sgraves: %s", hillClasses.c_str(), graves.c_str());
		}
		// The graves, the openings and the snacks needed, on a new game; the
		// snacks needed counted by snacks eaten, per level.
		auto graves = [&]() {
			Common::String out;
			for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
				if (_currentRoom->getObjectName(s).contains("_GRAVE"))
					out += Common::String::format("%s%s ", _currentRoom->getObjectName(s).c_str() + 4, _currentRoom->isClickable(s) ? " clickable" : "");
			}
			return out.empty() ? Common::String("no graves ") : out;
		};
		auto needed = [&]() {
			uint n = 1;
			while (n < 10 && !_script->snackEaten())
				n++;
			return n;
		};
		{
			Common::String out;
			for (int level = 0; level < 3; level++) {
				newGameForCheck();
				setDifficulty(level);
				for (uint i = 0; i < 3; i++) {
					fresh.room(kTunnel);
					fresh.room(kHills[i]);
					const Common::String opening = _script->getOpening();
					if (level == 0)
						out += Common::String::format("%s: %sopening '%s'; ", kHills[i] + 5, graves().c_str(), opening.c_str());
					out += Common::String::format("%s level %d needs %u; ", kHills[i] + 5, level, needed());
				}
			}
			setDifficulty(0);
			debugC(3, kDebugScript, "boot hill check rooms: %s", out.c_str());
		}
		{
			// Where the live run clicks (not compared).
			auto at = [&](const char *object) {
				const int s = Find::sprite(*this, object);
				for (int y = 100; y < 420 && s >= 0; y += 4) {
					for (int x = 4; x < 640; x += 4) {
						if (_currentRoom->hitTest(x, y) == s)
							return Common::String::format("%d %d", x + 4, y + 4);
					}
				}
				return Common::String("-");
			};
			newGameForCheck();
			fresh.room(kTunnel);
			fresh.room(kHills[0]);
			const Common::String g1 = at("OBJ_P21_BOOT_HILL1_GRAVE1"), g2 = at("OBJ_P21_BOOT_HILL1_GRAVE2");
			fresh.room(kTunnel);
			fresh.room(kHills[1]);
			debugC(3, kDebugScript, "boot hill points: P21 GRAVE1=%s GRAVE2=%s; P22 TO_P21=%s", g1.c_str(), g2.c_str(), at("OBJ_P22_TO_P21").c_str());
		}

		// A wrong grave: the ghost. The brick's grave: a dig scene
		// (rand() % 3), held; over, the room again through movie 3; there, its
		// opening, the brick, no graves; again, no opening.
		for (int location = 0; location < 9; location += 4) {
			const char *room = kHills[location / 3];
			const Common::String grave = BootHillRoom::getGraveObject(location);
			const Common::String wrong = BootHillRoom::getGraveObject(location / 3 * 3 + (location % 3 + 1) % 3);
			newGameForCheck();
			setGameInt("GraveLocation", location);
			_script->setGhostChance(nullptr, 0);
			fresh.room(kTunnel);
			fresh.room(room);
			_script->skip();
			_script->sendEvent(wrong, true);
			const int ghost = _currentRoom->findGhost();
			const Common::String ghostLine = Common::String::format("%s: ghost shown %d, held %d", wrong.c_str() + 4,
			                                                        ghost >= 0 && !_currentRoom->isHidden(ghost) ? 1 : 0, _script->isBusy() ? 1 : 0);
			Common::Array<Common::String> seen;
			for (uint k = 0; k < 40; k++) {
				newGameForCheck();
				setGameInt("GraveLocation", location);
				fresh.room(kTunnel);
				fresh.room(room);
				_script->skip();
				_script->sendEvent(grave, true);
				const Common::String s = hill()->getLastScene();
				bool known = false;
				for (uint i = 0; i < seen.size(); i++)
					known = known || seen[i] == s;
				if (!known)
					seen.push_back(s);
			}
			Common::sort(seen.begin(), seen.end());
			Common::String digs;
			for (uint i = 0; i < seen.size(); i++)
				digs += "'" + seen[i] + "' ";
			const bool held = _script->isBusy();
			_script->skip();
			const int t = run.change(5);
			const Common::String next = Common::String::format("next %s movie %d at tick %d", run.next.c_str(), run.movie, t);
			showRoom(room);
			const Common::String back = Common::String::format("opening '%s'%s, brick %d, %s", _script->getOpening().c_str(),
			                                                   _script->getMemory(room).openingPlayed ? " played" : "",
			                                                   _inventory->hasItem("OBJ_INV_BRICK") && _inventory->getSlot(0) == "OBJ_INV_BRICK" ? 1 : 0,
			                                                   graves().c_str());
			const bool dsp = hill()->hasDigScenePlayed();
			_script->skip();
			showRoom(kTunnel);
			showRoom(room);
			const Common::String again = Common::String::format("opening '%s'", _script->getOpening().c_str());
			// With the brick: the grave does nothing.
			_script->sendEvent(grave, true);
			debugC(3, kDebugScript, "boot hill check grave %d: %s; digs %sheld %d, %s; back: %s, DigScenePlayed %d; again %s; the grave again: scene '%s'",
			       location, ghostLine.c_str(), digs.c_str(), held ? 1 : 0, next.c_str(), back.c_str(), dsp ? 1 : 0, again.c_str(),
			       _script->isScenePlaying() ? _script->getScene().c_str() : "");
			_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
		}

		// Boot Hill 2's way out waits for the snacks, by level; the tunnel's
		// doesn't.
		{
			Common::String out;
			for (int level = 0; level < 3; level++) {
				newGameForCheck();
				setDifficulty(level);
				fresh.room(kTunnel);
				fresh.room(kHills[1]);
				_script->skip();
				const bool kept = _roomClass->leaveBy("OBJ_P22_TO_P21");
				const Common::String line = hill()->getLastScene();
				_script->skip();
				for (uint k = 0; k < 5; k++)
					tick1();
				const bool opens = _inventory->getState() != Inventory::kStateParked;
				out += Common::String::format("level %d: kept %d, %s, toolbar opening %d; ", level, kept ? 1 : 0, line.c_str(), opens ? 1 : 0);
			}
			setDifficulty(0);
			newGameForCheck();
			fresh.room(kTunnel);
			fresh.room(kHills[1]);
			const bool tunnel = _roomClass->leaveBy("OBJ_P22_TO_P18");
			for (uint k = 0; k < 2; k++)
				_script->snackEaten();
			const bool fed = _roomClass->leaveBy("OBJ_P22_TO_P21");
			debugC(3, kDebugScript, "boot hill check snacks: %sto the tunnel kept %d; with 2 snacks kept %d", out.c_str(), tunnel ? 1 : 0, fed ? 1 : 0);
			setDifficulty(0);
		}

		// Walking in.
		{
			static const struct {
				const char *from, *to;
			} kWalks[] = {
				{ "ROOM_P21_Boot_Hill1", "ROOM_P22_Boot_Hill2" }, { "ROOM_P21_Boot_Hill1", "ROOM_P23_Boot_Hill3" },
				{ "ROOM_P22_Boot_Hill2", "ROOM_P21_Boot_Hill1" }, { "ROOM_P23_Boot_Hill3", "ROOM_P21_Boot_Hill1" },
				{ "ROOM_P18_Tunnel_Lit", "ROOM_P22_Boot_Hill2" }
			};
			Common::String out;
			for (uint i = 0; i < ARRAYSIZE(kWalks); i++) {
				newGameForCheck();
				fresh.room(kWalks[i].from);
				fresh.room(kWalks[i].to);
				Common::String moving;
				for (uint c = 0; c < _script->getCharacters().size(); c++) {
					if (_script->getCharacters()[c].getState() == Character::kStateEndWalk)
						moving += _script->getCharacters()[c].getName() + " ";
				}
				out += Common::String::format("%s to %s: %s; ", kWalks[i].from + 5, kWalks[i].to + 5, moving.c_str());
			}
			debugC(3, kDebugScript, "boot hill check walking in: %s", out.c_str());
		}

		// The keys, and a new game's reset.
		{
			BootHillRoom *c = dynamic_cast<BootHillRoom *>(createRoomClass(*this, kHills[0]));
			Common::INIFile ini;
			ini.setKey("DigScenePlayed", kHills[0], "Yes");
			c->loadKeys(ini);
			const bool yes = c->hasDigScenePlayed();
			Common::INIFile out;
			c->saveKeys(out);
			Common::String v;
			out.getKey("DigScenePlayed", kHills[0], v);
			c->reset();
			debugC(3, kDebugScript, "boot hill check keys: DigScenePlayed=Yes reads %d, writes %s, reset %d", yes ? 1 : 0, v.c_str(), c->hasDigScenePlayed() ? 1 : 0);
			delete c;
		}
	}
	newGameForCheck();

	// The tunnel (WP-041).
	{
		const char *const kDark = "ROOM_P14_Tunnel_Dark";
		const char *const kLit = "ROOM_P18_Tunnel_Lit";
		const char *const kPost = "ROOM_P08_Post_Office";
		{
			RoomClass *a = createRoomClass(*this, kDark);
			RoomClass *b = createRoomClass(*this, kLit);
			debugC(3, kDebugScript, "tunnel check classes: P14_Tunnel_Dark %s; P18_Tunnel_Lit %s", dynamic_cast<TunnelDarkRoom *>(a) ? "dark" : a ? "other" : "-",
			       dynamic_cast<TunnelLitRoom *>(b) ? "lit" : b ? "other" : "-");
			delete a;
			delete b;
		}
		auto dark = [&]() { return dynamic_cast<TunnelDarkRoom *>(_roomClass); };
		// The crate and the opening; the way out's lines, the gang staying.
		newGameForCheck();
		fresh.room(kPost);
		fresh.room(kDark);
		{
			const int s = dark()->getCrateSprite();
			const Common::String crate = s < 0 ? Common::String("crate none")
			                                   : Common::String::format("crate %s mode %d step %u hidden %d%s", _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
			                                                            _currentRoom->getStep(s), _currentRoom->isHidden(s) ? 1 : 0, _currentRoom->isRequiresObject(s) ? " requires" : "");
			const Common::String opening = _script->getOpening();
			_script->skip();
			Common::String lines;
			for (uint k = 0; k < 3; k++) {
				_script->takeExit(_script->readExit("OBJ_P14_TO_P08"));
				lines += Common::String::format("%s held %d; ", dark()->getLastScene().c_str(), _script->isBusy() ? 1 : 0);
				_script->skip();
			}
			const int t = run.change(30);
			debugC(3, kDebugScript, "tunnel check dark: %s, opening '%s'; the way out: %sroom change at tick %d", crate.c_str(), opening.c_str(), lines.c_str(), t);
			// Where the live run clicks (not compared).
			auto at = [&](const char *object) {
				const int sp = Find::sprite(*this, object);
				for (int y = 60; y < 420 && sp >= 0; y += 4) {
					for (int x = 4; x < 640; x += 4) {
						if (_currentRoom->hitTest(x, y) == sp)
							return Common::String::format("%d %d", x + 4, y + 4);
					}
				}
				return Common::String("-");
			};
			const Common::String crateAt = at("OBJ_P14_TUNNEL_DARK_CRATE"), wayOut = at("OBJ_P14_TO_P08");
			fresh.room(kPost);
			fresh.room(kLit);
			debugC(3, kDebugScript, "tunnel points: CRATE=%s DARK_OUT=%s LIT_TO_P22=%s", crateAt.c_str(), wayOut.c_str(), at("OBJ_P18_TO_P22").c_str());
			fresh.room(kPost);
			fresh.room(kDark);
			_script->skip();
		}
		// The crate: the first try, Shaggy alone can't; the next, he and
		// Scooby push it, and the post office. A new visit starts again.
		{
			Common::String out;
			for (uint attempt = 0; attempt < 2; attempt++) {
				_script->sendEvent("OBJ_P14_TUNNEL_DARK_CRATE", true);
				const bool held = dark()->isBusy();
				Character *shaggy = _script->getCharacter("SHAGGY");
				Character *scooby = _script->getCharacter("SCOOBY");
				int tries = -1, done = -1, t = -1;
				Common::String at;
				for (uint k = 1; k <= kMax && t < 0 && done < 0; k++) {
					const bool changed = tick1();
					if (tries < 0 && shaggy && shaggy->getState() == Character::kStateSpecial) {
						tries = k;
						const int s = dark()->getCrateSprite();
						at = Common::String::format("%s steps %u, %s, crate hidden %d, Scooby hidden %d, held %d", _currentRoom->getAnim(shaggy->getSprite()).c_str(),
						                            _currentRoom->getStepCount(shaggy->getSprite()), dark()->getLastSound().c_str(),
						                            s >= 0 && _currentRoom->isHidden(s) ? 1 : 0, scooby && _currentRoom->isHidden(scooby->getSprite()) ? 1 : 0,
						                            dark()->isBusy() ? 1 : 0);
					}
					if (changed)
						t = k;
					else if (tries > 0 && !dark()->isCrateClicked())
						done = k;
				}
				if (t > 0)
					out += Common::String::format("try %u: held %d; Shaggy at tick %d (%s); %s next at tick %d; ", attempt + 1, held ? 1 : 0, tries, at.c_str(),
					                              run.next.c_str(), t);
				else
					out += Common::String::format("try %u: held %d; Shaggy at tick %d (%s); held %d at tick %d; ", attempt + 1, held ? 1 : 0, tries, at.c_str(),
					                              dark()->isBusy() ? 1 : 0, done);
			}
			fresh.room(kPost);
			fresh.room(kDark);
			_script->skip();
			_script->sendEvent("OBJ_P14_TUNNEL_DARK_CRATE", true);
			for (uint k = 0; k < 3; k++)
				tick1();
			debugC(3, kDebugScript, "tunnel check crate: %sa new visit: %s", out.c_str(), dark()->getLastSound().c_str());
		}

		// The lit tunnel: the first way out keeps its scene, later ones lose
		// it; going out first by a way with no scene uses the first up too.
		{
			auto leave = [&](const char *object) {
				_script->takeExit(_script->readExit(object));
				const Common::String s = _script->isScenePlaying() ? _script->getScene() : Common::String("none");
				_script->skip();
				return s;
			};
			newGameForCheck();
			fresh.room(kPost);
			fresh.room(kLit);
			const Common::String first = leave("OBJ_P18_TO_P22");
			const bool played = dynamic_cast<TunnelLitRoom *>(_roomClass)->isCommentPlayed();
			showRoom(kPost);
			showRoom(kLit);
			const Common::String second = leave("OBJ_P18_TO_P22");
			newGameForCheck();
			fresh.room(kPost);
			fresh.room(kLit);
			const Common::String viaPost = leave("OBJ_P18_TO_P08");
			showRoom(kPost);
			showRoom(kLit);
			const Common::String thenHill = leave("OBJ_P18_TO_P22");
			Common::INIFile ini;
			_roomClass->saveKeys(ini);
			Common::String v;
			ini.getKey("BootHillCommentPlayed", kLit, v);
			TunnelLitRoom *c = dynamic_cast<TunnelLitRoom *>(createRoomClass(*this, kLit));
			c->loadKeys(ini);
			const bool reads = c->isCommentPlayed();
			c->reset();
			debugC(3, kDebugScript, "tunnel check lit: to Boot Hill first %s, played %d; again %s; to the post office first %s, then to Boot Hill %s; "
			       "BootHillCommentPlayed=%s reads %d, reset %d", first.c_str(), played ? 1 : 0, second.c_str(), viaPost.c_str(), thenHill.c_str(), v.c_str(),
			       reads ? 1 : 0, c->isCommentPlayed() ? 1 : 0);
			delete c;
		}
	}
	newGameForCheck();

	// The hotel (WP-042).
	{
		const char *const kHall = "ROOM_P17_Hotel_Hall";
		const char *const kHRoom = "ROOM_P20_Hotel_Room";
		const char *const kNickelItem = "OBJ_INV_NICKEL";
		{
			RoomClass *a = createRoomClass(*this, kHall);
			RoomClass *b = createRoomClass(*this, kHRoom);
			debugC(3, kDebugScript, "hotel check classes: P17_Hotel_Hall %s; P20_Hotel_Room %s", dynamic_cast<HotelHallRoom *>(a) ? "hall" : a ? "other" : "-",
			       dynamic_cast<HotelRoomRoom *>(b) ? "room" : b ? "other" : "-");
			delete a;
			delete b;
		}
		auto hall = [&]() { return dynamic_cast<HotelHallRoom *>(_roomClass); };
		auto hroom = [&]() { return dynamic_cast<HotelRoomRoom *>(_roomClass); };
		auto placed = [&](const char *object, bool depth) {
			const int s = Find::sprite(*this, object);
			if (s < 0)
				return Common::String::format("%s none", object + 4);
			return Common::String::format("%s %s mode %d step %u%s%s%s%s", object + 4, _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s), _currentRoom->getStep(s),
			                              depth ? Common::String::format(" depth %d", _currentRoom->getDepth(s)).c_str() : "", _currentRoom->isHidden(s) ? " hidden" : "",
			                              _currentRoom->isRequiresObject(s) ? " requires" : "", _currentRoom->isClickable(s) ? " clickable" : "");
		};
		auto opening = [&](const char *room) {
			return Common::String::format("'%s'%s", _script->getOpening().c_str(), _script->getMemory(room).openingPlayed ? " played" : "");
		};

		// The hall: the placements; the openings over three visits; the
		// walk-ins; the music.
		newGameForCheck();
		{
			fresh.room(kHRoom);
			fresh.room(kHall);
			const Common::String places = placed("OBJ_P17_HOTEL_HALL_DOOR", true) + "; " + placed("OBJ_P17_HOTEL_HALL_HOTSPOT2", true) + "; " +
			                              placed("OBJ_P17_HOTEL_HALL_CHAND", true) + "; " + placed("OBJ_P17_HOTEL_HALL_HOTSPOT1", false);
			const Common::String first = opening(kHall);
			const uint music = _sound->getMusicPercent();
			_script->skip();
			showRoom(kHRoom);
			showRoom(kHall);
			const Common::String second = opening(kHall) + Common::String::format(" SecondEntrance %d", hall()->hasSecondEntrance() ? 1 : 0);
			_script->skip();
			showRoom(kHRoom);
			showRoom(kHall);
			const Common::String third = opening(kHall);
			debugC(3, kDebugScript, "hotel check hall: %s; openings %s, %s, %s; music %u%%", places.c_str(), first.c_str(), second.c_str(), third.c_str(), music);
		}
		{
			// Each walk-in against the same character's depth when it doesn't
			// walk in (from the other room).
			Common::String out;
			auto depthOf = [&](const char *from, const char *who) {
				newGameForCheck();
				fresh.room(from);
				fresh.room(kHall);
				Character *c = _script->getCharacter(who);
				return c ? Common::String::format("%d %d", c->getState(), _currentRoom->getDepth(c->getSprite())) : Common::String("-");
			};
			static const char *const kFrom[] = { "ROOM_P12_Saloon", "ROOM_P20_Hotel_Room" };
			static const char *const kWho[] = { "DAPHNE", "FRED" };
			for (uint i = 0; i < 2; i++) {
				const Common::String walks = depthOf(kFrom[i], kWho[i]), stays = depthOf(kFrom[1 - i], kWho[i]);
				int sw = 0, dw = 0, ss = 0, ds = 0;
				sscanf(walks.c_str(), "%d %d", &sw, &dw);
				sscanf(stays.c_str(), "%d %d", &ss, &ds);
				out += Common::String::format("from %s: %s state %d (else %d), depth %+d; ", kFrom[i] + 5, kWho[i], sw, ss, dw - ds);
			}
			newGameForCheck();
			_inventory->addItem(kNickelItem);
			_inventory->removeItem(kNickelItem);
			fresh.room(kHRoom);
			fresh.room(kHall);
			debugC(3, kDebugScript, "hotel check hall walking in: %sthe nickel used: %s", out.c_str(), placed("OBJ_P17_HOTEL_HALL_CHAND", true).c_str());
		}
		// The door: Fred opens it; back, it stays open. The chandelier's
		// lines. Hotspot 1 on A's step 6, B's step 4, A's step 5.
		newGameForCheck();
		fresh.room(kHRoom);
		fresh.room(kHall);
		_script->skip();
		// One tick: Fred's walk-in from the right has no sequence, so it
		// stops and he goes idle.
		tick1();
		{
			_script->sendEvent("OBJ_P17_HOTEL_HALL_DOOR", false);
			Character *fred = _script->getCharacter("FRED");
			const Common::String door = placed("OBJ_P17_HOTEL_HALL_DOOR", false) + Common::String::format(", HotelHallDoor %d, Fred state %d %s",
			                                                                                              getGameFlag("HotelHallDoor") ? 1 : 0, fred ? fred->getState() : -1,
			                                                                                              fred ? _currentRoom->getAnim(fred->getSprite()).c_str() : "-");
			Common::String chand;
			for (uint k = 0; k < 4; k++) {
				_script->sendEvent("OBJ_P17_HOTEL_HALL_CHAND", true);
				chand += hall()->getLastScene() + Common::String::format(" held %d; ", _script->isBusy() ? 1 : 0);
				_script->skip();
			}
			Common::String hot;
			static const struct {
				const char *anim;
				uint step;
			} kSteps[] = { { "ANIM_P17_HOTEL_HALL_HOTSPOT1A", 6 }, { "ANIM_P17_HOTEL_HALL_HOTSPOT1B", 4 }, { "ANIM_P17_HOTEL_HALL_HOTSPOT1A", 5 } };
			for (uint k = 0; k < ARRAYSIZE(kSteps); k++) {
				fresh.room(kHRoom);
				fresh.room(kHall);
				_script->skip();
				const int h = hall()->getHotspot1();
				if (h >= 0)
					_currentRoom->startAnim(h, kSteps[k].anim, 0, kSteps[k].step);
				tick1();
				Character *d = _script->getCharacter("DAPHNE");
				hot += Common::String::format("%s step %u: Daphne state %d, scene '%s'; ", kSteps[k].anim + 20, kSteps[k].step, d ? d->getState() : -1,
				                              _script->isScenePlaying() ? _script->getScene().c_str() : "");
			}
			showRoom(kHRoom);
			showRoom(kHall);
			debugC(3, kDebugScript, "hotel check hall door: %s; back: %s; chandelier: %shotspot 1: %s", door.c_str(), placed("OBJ_P17_HOTEL_HALL_DOOR", false).c_str(),
			       chand.c_str(), hot.c_str());
		}

		// The room: the first visit's opening, then the window by itself
		// (movie 15 + GraveLocation); back, the comments in turn; with the
		// brick, none.
		newGameForCheck();
		setGameInt("GraveLocation", 4);
		fresh.room(kHall);
		fresh.room(kHRoom);
		{
			const Common::String first = opening(kHRoom) + Common::String::format(" LookedOutAuto %d, music %u%%", hroom()->hasLookedOutAuto() ? 1 : 0,
			                                                                      _sound->getMusicPercent());
			_script->skip();
			const int t = run.change(kMax);
			const Common::String autoLook = Common::String::format("%s movie %d scene '%s'; next %s movie %d at tick %d", hroom()->getLastExit().destination.c_str(),
			                                                       hroom()->getLastExit().movie, hroom()->getLastExit().scene.c_str(), run.next.c_str(), run.movie, t);
			Common::String back;
			for (uint k = 0; k < 3; k++) {
				showRoom(kHRoom);
				back += opening(kHRoom) + Common::String::format(" (window count %d); ", hroom()->getWindowCount());
				_script->skip();
				_script->takeExit(_script->readExit("OBJ_P20_HOTEL_WINDOW"));
				back += Common::String::format("window: movie %d scene '%s'; ", hroom()->getLastExit().movie, hroom()->getLastExit().scene.c_str());
				_script->skip();
			}
			_inventory->addItem("OBJ_INV_BRICK");
			showRoom(kHRoom);
			debugC(3, kDebugScript, "hotel check room: first %s; the window by itself: %s; back: %swith the brick: %s", first.c_str(), autoLook.c_str(), back.c_str(),
			       opening(kHRoom).c_str());
		}
		// The nickel: Daphne picks it up; the hotspots. Each starts after the
		// first visit's look out of the window.
		auto settled = [&]() {
			newGameForCheck();
			fresh.room(kHall);
			fresh.room(kHRoom);
			_script->skip();
			run.change(kMax);
			showRoom(kHRoom);
			_script->skip();
		};
		settled();
		{
			const Common::String nickel = placed(kNickelItem, false);
			_script->sendEvent(kNickelItem, false);
			const bool held = hroom()->isBusy();
			Character *d = _script->getCharacter("DAPHNE");
			int grabs = -1, gone = -1, got = -1;
			Common::String anim, after;
			for (uint k = 1; k <= kMax && got < 0; k++) {
				tick1();
				if (grabs < 0 && d && d->getState() == Character::kStateSpecial) {
					grabs = k;
					anim = Common::String::format("%s steps %u", _currentRoom->getAnim(d->getSprite()).c_str(), _currentRoom->getStepCount(d->getSprite()));
				}
				if (gone < 0 && hroom()->getNickelSprite() >= 0 && _currentRoom->isHidden(hroom()->getNickelSprite()))
					gone = k;
				if (got < 0 && _inventory->hasItem(kNickelItem)) {
					got = k;
					after = Common::String::format("%s held %d, class held %d", hroom()->getLastScene().c_str(), _script->isBusy() ? 1 : 0, hroom()->isBusy() ? 1 : 0);
				}
			}
			_script->skip();
			showRoom(kHall);
			showRoom(kHRoom);
			const Common::String again = placed(kNickelItem, false);
			// Left during the pick-up: it goes in the toolbar.
			settled();
			_script->sendEvent(kNickelItem, false);
			tick1();
			showRoom(kHall);
			const bool leftWith = _inventory->hasItem(kNickelItem);
			debugC(3, kDebugScript, "hotel check nickel: %s; clicked: held %d; Daphne at tick %d (%s); gone at tick %d; in the toolbar at tick %d, %s; back: %s; left during it: %d",
			       nickel.c_str(), held ? 1 : 0, grabs, anim.c_str(), gone, got, after.c_str(), again.c_str(), leftWith ? 1 : 0);
		}
		{
			settled();
			_script->setGhostChance(nullptr, 0);
			const int h4 = Find::sprite(*this, "OBJ_P20_HOTEL_HOTSPOT4");
			const int h2 = Find::sprite(*this, "OBJ_P20_HOTEL_HOTSPOT2");
			Common::String out = Common::String::format("hotspot 2 depth %d, hotspot 4 depth %d; ", h2 >= 0 ? _currentRoom->getDepth(h2) : -9999,
			                                            h4 >= 0 ? _currentRoom->getDepth(h4) : -9999);
			if (h4 >= 0) {
				_currentRoom->startAnim(h4, _currentRoom->getAnim(h4), 0, 11);
				tick1();
				out += Common::String::format("hotspot 4 on step 11: scene '%s'; ", _script->isScenePlaying() ? _script->getScene().c_str() : "");
				_script->skip();
				_currentRoom->startAnim(h4, _currentRoom->getAnim(h4), 0, 0);
			}
			if (h2 >= 0) {
				_currentRoom->startAnim(h2, "ANIM_P20_HOTEL_HOTSPOT2C", 0, 18);
				tick1();
				Character *d = _script->getCharacter("DAPHNE");
				out += Common::String::format("hotspot 2 on C's step 18: Daphne state %d; ", d ? d->getState() : -1);
				_script->setClickable(h2, "ANIM_P20_HOTEL_HOTSPOT2C");
				_script->sendEvent("OBJ_P20_HOTEL_HOTSPOT2", true);
				out += Common::String::format("clicked with its state C: scene '%s'", hroom()->getLastScene().c_str());
			}
			_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
			// The keys.
			HotelHallRoom *a = dynamic_cast<HotelHallRoom *>(createRoomClass(*this, kHall));
			HotelRoomRoom *b = dynamic_cast<HotelRoomRoom *>(createRoomClass(*this, kHRoom));
			Common::INIFile ini;
			ini.setKey("SecondEntrance", kHall, "Yes");
			ini.setKey("LookedOutAuto", kHRoom, "Yes");
			a->loadKeys(ini);
			b->loadKeys(ini);
			const bool ra = a->hasSecondEntrance(), rb = b->hasLookedOutAuto();
			a->reset();
			b->reset();
			debugC(3, kDebugScript, "hotel check hotspots: %s; keys: SecondEntrance reads %d, LookedOutAuto reads %d, reset %d %d", out.c_str(), ra ? 1 : 0, rb ? 1 : 0,
			       a->hasSecondEntrance() ? 1 : 0, b->hasLookedOutAuto() ? 1 : 0);
			delete a;
			delete b;
		}
	}

	// The stage (WP-043).
	{
		const char *const kStage = "ROOM_P05_Stage";
		const char *const kTrap = "ROOM_P06_Stage_Trapdoor";
		const char *const kSaloon = "ROOM_P12_Saloon";
		const int levelBefore = getDifficulty();
		_script->setGhostChance(nullptr, 0);
		{
			RoomClass *a = createRoomClass(*this, kStage);
			RoomClass *b = createRoomClass(*this, kTrap);
			debugC(3, kDebugScript, "stage check classes: P05_Stage %s; P06_Stage_Trapdoor %s", dynamic_cast<StageRoom *>(a) ? "stage" : a ? "other" : "-",
			       dynamic_cast<StageTrapdoorRoom *>(b) ? "trapdoor" : b ? "other" : "-");
			delete a;
			delete b;
		}
		auto stage = [&]() { return dynamic_cast<StageRoom *>(_roomClass); };
		auto trap = [&]() { return dynamic_cast<StageTrapdoorRoom *>(_roomClass); };
		auto bags = [&]() {
			Common::String b;
			for (uint i = 0; i < 4; i++)
				b += getGameFlag(Common::String::format("BagDown%u", i)) ? "1" : "0";
			return b;
		};
		auto setBags = [&](const char *b) {
			for (uint i = 0; i < 4; i++)
				setGameFlag(Common::String::format("BagDown%u", i), b[i] == '1');
			setGameFlag("BagsRandomized", true);
		};
		auto setAliases = [&](const char *a) {
			for (uint i = 0; i < 4; i++)
				setGameInt(Common::String::format("LeverAlias%u", i), a[i] - '0');
		};
		auto sprite = [&](int s) {
			if (s < 0)
				return Common::String("none");
			return Common::String::format("%s mode %d step %u of %u depth %d hit %d%s%s", _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
			                              _currentRoom->getStep(s), _currentRoom->getStepCount(s), _currentRoom->getDepth(s), _currentRoom->getHitType(s),
			                              _currentRoom->isHidden(s) ? " hidden" : "", _currentRoom->isClickable(s) ? " clickable" : "");
		};
		auto hits = [&]() {
			Common::String h;
			for (uint i = 0; i < 4; i++)
				h += trap() && trap()->getLever(i) >= 0 ? Common::String::format("%d", _currentRoom->getHitType(trap()->getLever(i))) : Common::String("-");
			return h;
		};
		auto opening = [&](const char *room) {
			return Common::String::format("'%s'%s", _script->getOpening().c_str(), _script->getMemory(room).openingPlayed ? " played" : "");
		};

		// The stage: the sandbags hung by the level, once; Jenny and the way
		// down by whether she has been met; Shaggy's walk-ins.
		{
			Common::String out;
			for (int level = 0; level < 3; level++) {
				newGameForCheck();
				setDifficulty(level);
				fresh.room(kSaloon);
				fresh.room(kStage);
				out += Common::String::format("level %d: bags down %s, BagsRandomized %d; ", level, bags().c_str(), getGameFlag("BagsRandomized") ? 1 : 0);
				if (level == 0) {
					for (uint i = 0; i < 4; i++)
						out += Common::String::format("SANDBAG%u %s; ", i + 1, sprite(stage()->getSandbag(i)).c_str());
					Character *shaggy = _script->getCharacter("SHAGGY");
					out += Common::String::format("JENNY %s, way down %s; Shaggy from the saloon state %d %s; ", Find::sprite(*this, "OBJ_JENNY") >= 0 ? "placed" : "none",
					                              Find::sprite(*this, "OBJ_P05_T0_P06") >= 0 ? "placed" : "none", shaggy ? shaggy->getState() : -1,
					                              shaggy ? _currentRoom->getAnim(shaggy->getSprite()).c_str() : "-");
					setBags("1111");
					showRoom(kSaloon);
					showRoom(kStage);
					out += Common::String::format("again with 1111: %s; ", bags().c_str());
					showRoom(kTrap);
					showRoom(kStage);
					shaggy = _script->getCharacter("SHAGGY");
					out += Common::String::format("Shaggy from under the stage state %d %s; ", shaggy ? shaggy->getState() : -1,
					                              shaggy ? _currentRoom->getAnim(shaggy->getSprite()).c_str() : "-");
				}
			}
			debugC(3, kDebugScript, "stage check stage: %s", out.c_str());
		}

		// Under the stage: the placements, the openings by level, the knocks.
		{
			newGameForCheck();
			setDifficulty(0);
			setBags("0111");
			fresh.room(kStage);
			fresh.room(kTrap);
			Common::String out;
			for (uint i = 0; i < 4; i++)
				out += Common::String::format("LEVER%u %s; ", i + 1, sprite(trap()->getLever(i)).c_str());
			for (uint i = 0; i < 4; i++)
				out += Common::String::format("SANDBAG%u %s; ", i + 1, sprite(trap()->getSandbag(i)).c_str());
			out += Common::String::format("DOOR %s; EXIT %s; knock countdown %d", sprite(trap()->getDoor()).c_str(), sprite(trap()->getExit()).c_str(),
			                              trap()->getKnockCountdown());
			tick1();
			out += Common::String::format("; after tick 1: %d, levers hit %s", trap()->getKnockCountdown(), hits().c_str());
			for (uint t = 2; t <= 75; t++) {
				tick1();
				if (t == 74 || t == 75)
					out += Common::String::format(", after tick %u: %d", t, trap()->getKnockCountdown());
			}
			debugC(3, kDebugScript, "stage check trapdoor: %s", out.c_str());
			out.clear();
			for (int level = 0; level < 3; level++) {
				newGameForCheck();
				setDifficulty(level);
				setBags("1111");
				fresh.room(kStage);
				out += Common::String::format("level %d:", level);
				for (uint k = 0; k < 4; k++) {
					if (k == 0)
						fresh.room(kTrap);
					else
						showRoom(kTrap);
					out += " " + opening(kTrap);
					_script->skip();
					showRoom(kStage);
				}
				out += "; ";
			}
			debugC(3, kDebugScript, "stage check trapdoor openings: %s", out.c_str());
		}

		// The levers: each from four bags down, by level, with the aliases in
		// order; then other aliases; a lever twice; the levers held while
		// the bags move.
		auto pull = [&](int level, const char *aliases, const char *from, uint lever) {
			newGameForCheck();
			setDifficulty(level);
			setAliases(aliases);
			setBags(from);
			fresh.room(kStage);
			fresh.room(kTrap);
			_script->skip();
			tick1();
			_script->clickClickable(trap()->getLever(lever));
			tick1();
		};
		{
			Common::String out;
			for (int level = 0; level < 3; level++) {
				out += Common::String::format("level %d:", level);
				for (uint n = 0; n < 4; n++) {
					pull(level, "0123", "1111", n);
					out += Common::String::format(" %u %s", n + 1, bags().c_str());
				}
				out += "; ";
			}
			pull(0, "2031", "1111", 0);
			out += Common::String::format("aliases 2031, lever 1: %s; ", bags().c_str());
			pull(3, "0123", "1111", 0);
			out += Common::String::format("level 3, lever 1: %s", bags().c_str());
			debugC(3, kDebugScript, "stage check levers: %s", out.c_str());
		}
		{
			pull(0, "0123", "1111", 0);
			Common::String out = Common::String::format("lever 1: %s, SANDBAG1 %s, sound %s, levers hit %s", bags().c_str(),
			                                            sprite(trap()->getSandbag(0)).c_str(), trap()->getLastSound().c_str(), hits().c_str());
			int back = -1;
			for (uint t = 2; t <= kMax && back < 0; t++) {
				tick1();
				if (hits() == "2222")
					back = t;
			}
			out += Common::String::format("; levers back at tick %d, SANDBAG1 %s", back, sprite(trap()->getSandbag(0)).c_str());
			_script->clickClickable(trap()->getLever(0));
			tick1();
			out += Common::String::format("; again: %s, SANDBAG1 %s, sound %s", bags().c_str(), sprite(trap()->getSandbag(0)).c_str(),
			                              trap()->getLastSound().c_str());
			debugC(3, kDebugScript, "stage check lever moves: %s", out.c_str());
		}

		// The trapdoor opens: the last bag up. Jenny met; up onto the stage
		// through movie 4.
		{
			pull(0, "0123", "1000", 0);
			Common::String out = Common::String::format("bags %s, DOOR %s, opening %d, Jenny met %d, sound %s, EXIT hit %d, levers hit %s",
			                                            bags().c_str(), sprite(trap()->getDoor()).c_str(), trap()->isDoorOpening() ? 1 : 0,
			                                            _inventory->hasMetSuspect("JENNY") ? 1 : 0, trap()->getLastSound().c_str(),
			                                            _currentRoom->getHitType(trap()->getExit()), hits().c_str());
			int out1 = -1, change = -1;
			RoomScript::Exit last;
			Common::String held;
			for (uint t = 2; t <= kMax && change < 0; t++) {
				if (tick1()) {
					change = t;
				} else if (out1 < 0 && trap() && trap()->getLastExit().destination == kStage) {
					out1 = t;
					last = trap()->getLastExit();
					held = hits();
				}
			}
			out += Common::String::format("; out at tick %d (levers hit %s) to %s movie %d; next %s movie %d at tick %d", out1, held.c_str(),
			                              last.destination.c_str(), last.movie, run.next.c_str(), run.movie, change);
			debugC(3, kDebugScript, "stage check trapdoor opens: %s", out.c_str());
		}

		// Jenny on the stage: the hammer, her scene and clue; she gives, the
		// thanks, the suspects. Clicked: a line, with the arrow only.
		{
			newGameForCheck();
			setDifficulty(0);
			setBags("0000");
			_inventory->meetSuspect("JENNY");
			fresh.room(kTrap);
			fresh.room(kStage);
			Character *jenny = _script->getCharacter("JENNY");
			Character *shaggy = _script->getCharacter("SHAGGY");
			Common::String out = Common::String::format("JENNY %s, way down %s, hammer %d, scene '%s' held %d, clue owner %d, gives %d; Shaggy %s; ",
			                                            jenny ? "placed" : "none", Find::sprite(*this, "OBJ_P05_T0_P06") >= 0 ? "placed" : "none",
			                                            _inventory->hasItem("OBJ_INV_HAMMER") ? 1 : 0, stage()->getLastScene().c_str(), _script->isBusy() ? 1 : 0,
			                                            isClueOwner("JENNY") ? 1 : 0, stage()->isGiving() ? 1 : 0,
			                                            shaggy ? _currentRoom->getAnim(shaggy->getSprite()).c_str() : "-");
			_script->skip();
			int gives = -1, thanks = -1, change = -1;
			Common::String anim;
			for (uint t = 1; t <= kMax && change < 0; t++) {
				if (tick1()) {
					change = t;
					break;
				}
				jenny = _script->getCharacter("JENNY");
				if (gives < 0 && jenny && jenny->getState() == Character::kStateAct) {
					gives = t;
					anim = Common::String::format("%s steps %u, held %d", _currentRoom->getAnim(jenny->getSprite()).c_str(),
					                              _currentRoom->getStepCount(jenny->getSprite()), stage()->isBusy() ? 1 : 0);
				}
				if (thanks < 0 && _script->isScenePlaying() && _script->getScene() == "Global.Thank_You_")
					thanks = t;
				if (_script->isScenePlaying())
					_script->skip();
			}
			out += Common::String::format("gives at tick %d (%s); thanks at tick %d; next %s", gives, anim.c_str(), thanks, run.next.c_str());
			// Back with her clue given: no scene; clicked, a line.
			showRoom(kStage);
			out += Common::String::format("; back: scene '%s', gives %d", stage()->getLastScene().c_str(), stage()->isGiving() ? 1 : 0);
			jenny = _script->getCharacter("JENNY");
			if (jenny) {
				_roomClass->click(jenny->getSprite());
				tick1();
				const Common::String line = stage()->getLastScene();
				out += Common::String::format("; clicked: %s", line.hasPrefix("Stage.180") && line.hasSuffix("_") ? "a Stage.180n_ line" : line.c_str());
				out += Common::String::format(", held %d", _script->isBusy() ? 1 : 0);
				_script->skip();
				showRoom(kStage);
				_inventory->setCursor("ANIM_CURSORRUSTY_HAMMER");
				_roomClass->click(jenny->getSprite());
				tick1();
				out += Common::String::format("; with the hammer: '%s'", stage()->getLastScene().c_str());
				_inventory->setCursor("ANIM_CURSORARROW");
			}
			debugC(3, kDebugScript, "stage check jenny: %s", out.c_str());
		}
		_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
		setDifficulty(levelBefore);
	}

	// The jail (WP-044).
	{
		const char *const kJail = "ROOM_P07_Jail";
		const char *const kOffice = "ROOM_P02_Sheriffs_Office";
		const char *const kBlanketItem = "OBJ_INV_BLANKET";
		_script->setGhostChance(nullptr, 0);
		{
			RoomClass *a = createRoomClass(*this, kJail);
			debugC(3, kDebugScript, "jail check classes: P07_Jail %s", dynamic_cast<JailRoom *>(a) ? "jail" : a ? "other" : "-");
			delete a;
		}
		auto jail = [&]() { return dynamic_cast<JailRoom *>(_roomClass); };
		auto placed = [&](const char *object) {
			const int s = Find::sprite(*this, object);
			if (s < 0)
				return Common::String::format("%s none", object + 4);
			return Common::String::format("%s %s mode %d step %u depth %d hit %d%s%s%s", object + 4, _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
			                              _currentRoom->getStep(s), _currentRoom->getDepth(s), _currentRoom->getHitType(s), _currentRoom->isHidden(s) ? " hidden" : "",
			                              _currentRoom->isRequiresObject(s) ? " requires" : "", _currentRoom->isClickable(s) ? " clickable" : "");
		};
		auto special = [&](const char *name) {
			Character *c = _script->getCharacter(name);
			if (!c)
				return Common::String::format("%s none", name);
			return Common::String::format("%s state %d %s steps %u", name, c->getState(), _currentRoom->getAnim(c->getSprite()).c_str(),
			                              _currentRoom->getStepCount(c->getSprite()));
		};
		auto hidden = [&](const char *name) {
			Character *c = _script->getCharacter(name);
			return c && _currentRoom->isHidden(c->getSprite()) ? 1 : 0;
		};
		auto enterJail = [&]() {
			newGameForCheck();
			fresh.room(kOffice);
			fresh.room(kJail);
			_script->skip();
		};
		// A click on a clickable, its state and scene after the tick; then
		// the scene skipped and the clickable at rest.
		auto clickRest = [&](int sprite) {
			_script->clickClickable(sprite);
			tick1();
			const Common::String state = _script->getClickableState(sprite);
			const Common::String r = Common::String::format(" %c '%s' held %d", state.empty() ? '-' : state.lastChar(), _script->getScene().c_str(),
			                                                _script->isBusy() ? 1 : 0);
			for (uint t = 0; t < kMax && (_script->isScenePlaying() || _script->isClickablePlaying(sprite)); t++) {
				if (_script->isScenePlaying())
					_script->skip();
				tick1();
			}
			return r;
		};

		// The placements, and the blanket once taken.
		{
			enterJail();
			Common::String out = placed(kBlanketItem) + "; " + placed("OBJ_P07_JAIL_HOTSPOT2") +
			                     Common::String::format(" rollover '%s'; ", _currentRoom->getRollover(jail()->getBed()).c_str()) +
			                     placed("OBJ_P07_JAIL_HOTSPOT3") + "; " + placed("OBJ_P07_JAIL_HOTSPOT4") + "; " + placed("OBJ_P07_JAIL_OVERLAY_HOLE") + "; " +
			                     placed("OBJ_P07_JAIL_OVERLAY_STOOL");
			out += Common::String::format("; BedBroken %d, opening '%s'", jail()->isBedBroken() ? 1 : 0, _script->getOpening().c_str());
			_inventory->addItem(kBlanketItem);
			showRoom(kOffice);
			showRoom(kJail);
			out += "; taken: " + placed(kBlanketItem);
			debugC(3, kDebugScript, "jail check placements: %s", out.c_str());
		}

		// Hotspots 3 and 4, and the bed's first click: their lines.
		{
			enterJail();
			const int h3 = Find::sprite(*this, "OBJ_P07_JAIL_HOTSPOT3");
			Common::String out = "hotspot 3:";
			for (uint k = 0; k < 3; k++)
				out += clickRest(h3);
			out += "; hotspot 4:";
			for (uint k = 0; k < 5; k++)
				out += clickRest(jail()->getHotspot4());
			out += "; bed:" + clickRest(jail()->getBed());
			out += Common::String::format(", BedBroken %d", jail()->isBedBroken() ? 1 : 0);
			debugC(3, kDebugScript, "jail check hotspots: %s", out.c_str());
		}

		// The blanket: not with an item; with the arrow Shaggy takes it.
		{
			enterJail();
			_inventory->setCursor("ANIM_CURSORRUSTY_HAMMER");
			_script->sendEvent(kBlanketItem, false);
			Common::String out = Common::String::format("another cursor: busy %d", jail()->isBusy() ? 1 : 0);
			_inventory->setCursor("ANIM_CURSORARROW");
			_script->sendEvent(kBlanketItem, false);
			out += Common::String::format("; the arrow: busy %d", jail()->isBusy() ? 1 : 0);
			int got = -1;
			for (uint t = 1; t <= kMax && got < 0; t++) {
				tick1();
				if (t == 1)
					out += Common::String::format("; tick 1: %s, %s", special("SHAGGY").c_str(), placed(kBlanketItem).c_str());
				if (_inventory->hasItem(kBlanketItem)) {
					got = t;
					out += Common::String::format("; in the toolbar at tick %d, scene '%s' held %d, busy %d", t, _script->getScene().c_str(),
					                              _script->isBusy() ? 1 : 0, jail()->isBusy() ? 1 : 0);
				}
			}
			_script->skip();
			showRoom(kOffice);
			showRoom(kJail);
			out += "; back: " + placed(kBlanketItem);
			debugC(3, kDebugScript, "jail check blanket: %s", out.c_str());
		}

		// The bed: the second click, Scooby breaks it; then 150D_ and the
		// two down the hole; and the jail again.
		{
			enterJail();
			const int bed = jail()->getBed();
			clickRest(bed);
			_script->clickClickable(bed);
			tick1();
			Common::String out = Common::String::format("bed hit %d %s mode %d, busy %d; %s; sound %s", _currentRoom->getHitType(bed), _currentRoom->getAnim(bed).c_str(),
			                                            _currentRoom->getMode(bed), jail()->isBusy() ? 1 : 0, special("SCOOBY").c_str(), jail()->getLastSound().c_str());
			int broken = -1;
			for (uint t = 2; t <= kMax && broken < 0; t++) {
				tick1();
				if (jail()->isBedBroken())
					broken = t;
			}
			out += Common::String::format("; broken at tick %d: bed hidden %d rollover '%s', %s, %s, scene '%s' held %d, busy %d", broken,
			                              _currentRoom->isHidden(bed) ? 1 : 0, _currentRoom->getRollover(bed).c_str(), placed("OBJ_P07_JAIL_OVERLAY_HOLE").c_str(),
			                              placed("OBJ_P07_JAIL_OVERLAY_STOOL").c_str(), _script->getScene().c_str(), _script->isBusy() ? 1 : 0, jail()->isBusy() ? 1 : 0);
			debugC(3, kDebugScript, "jail check bed: %s", out.c_str());

			const int stool = _currentRoom->getDepth(jail()->getStool());
			int start = -1, step6 = -9999, change = -1;
			Common::String at;
			int shaggySprite = -1;
			for (uint t = 1; t <= kMax && change < 0; t++) {
				if (_script->isScenePlaying())
					_script->skip();
				if (tick1()) {
					change = t;
					break;
				}
				Character *sh = _script->getCharacter("SHAGGY");
				if (start < 0 && sh && sh->getState() == Character::kStateSpecial) {
					start = t;
					shaggySprite = sh->getSprite();
					at = Common::String::format("%s depth %d, stool %d, Scooby hidden %d, sound %s, busy %d", special("SHAGGY").c_str(), _currentRoom->getDepth(sh->getSprite()),
					                            stool, hidden("SCOOBY"), jail()->getLastSound().c_str(), jail()->isBusy() ? 1 : 0);
				}
				if (start >= 0 && step6 == -9999 && sh && _currentRoom->getStep(sh->getSprite()) == 6)
					step6 = _currentRoom->getDepth(sh->getSprite());
			}
			out = Common::String::format("after 150D_: %s; at step 6 depth %d; next %s %d ticks after, Shaggy hidden %d", at.c_str(), step6, run.next.c_str(),
			                             change - start, shaggySprite >= 0 && _currentRoom->isHidden(shaggySprite) ? 1 : 0);
			showRoom(kJail);
			const bool again = tick1();
			out += Common::String::format("; again: next %s at tick 1 (%d), hole done %d", run.next.c_str(), again ? 1 : 0, jail() && jail()->isHoleDone() ? 1 : 0);
			debugC(3, kDebugScript, "jail check hole: %s", out.c_str());
		}

		// The keys: BedBroken saved and read back; a new game's reset.
		{
			enterJail();
			Common::INIFile before;
			jail()->saveKeys(before);
			Common::String no;
			before.getKey("BedBroken", kJail, no);
			Common::INIFile yes;
			yes.setKey("BedBroken", kJail, "Yes");
			jail()->loadKeys(yes);
			showRoom(kOffice);
			showRoom(kJail);
			const int bed = jail()->getBed();
			Common::String out = Common::String::format("BedBroken=%s; Yes read: bed hidden %d rollover '%s', hole hidden %d, stool hidden %d", no.c_str(),
			                                            _currentRoom->isHidden(bed) ? 1 : 0, _currentRoom->getRollover(bed).c_str(),
			                                            _currentRoom->isHidden(jail()->getHole()) ? 1 : 0, _currentRoom->isHidden(jail()->getStool()) ? 1 : 0);
			tick1();
			out += Common::String::format("; tick 1: scene '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
			Common::INIFile after;
			jail()->saveKeys(after);
			Common::String v;
			after.getKey("BedBroken", kJail, v);
			jail()->reset();
			const bool reset = jail()->isBedBroken();
			Common::INIFile none;
			jail()->loadKeys(yes);
			jail()->loadKeys(none);
			out += Common::String::format("; saved BedBroken=%s; reset %d, missing %d", v.c_str(), reset ? 1 : 0, jail()->isBedBroken() ? 1 : 0);
			_script->skip();
			debugC(3, kDebugScript, "jail check keys: %s", out.c_str());
		}

		// Out of the jail with the blanket or the bed under way: finished.
		{
			enterJail();
			_script->sendEvent(kBlanketItem, false);
			showRoom(kOffice);
			Common::String out = Common::String::format("the blanket clicked, then out: blanket %d", _inventory->hasItem(kBlanketItem) ? 1 : 0);
			enterJail();
			const int bed = jail()->getBed();
			clickRest(bed);
			_script->clickClickable(bed);
			tick1();
			JailRoom *j = jail();
			showRoom(kOffice);
			out += Common::String::format("; the bed clicked, then out: BedBroken %d", j->isBedBroken() ? 1 : 0);
			debugC(3, kDebugScript, "jail check left: %s", out.c_str());
		}

		// The exit handler: Shaggy leaves only with the blanket in a slot and
		// no scene; never the default handler.
		{
			enterJail();
			Common::String out = Common::String::format("no blanket: %d, leaving %d", jail()->leaveBy("OBJ_P02_TO_P07") ? 1 : 0, jail()->isLeaving() ? 1 : 0);
			_inventory->addItem(kBlanketItem);
			_script->startScene("Jail_Cell.150X2_", true);
			jail()->leaveBy("OBJ_P02_TO_P07");
			out += Common::String::format("; the blanket in a slot, a scene: leaving %d", jail()->isLeaving() ? 1 : 0);
			_script->skip();
			jail()->leaveBy("OBJ_P02_TO_P07");
			out += Common::String::format("; no scene: leaving %d, %s", jail()->isLeaving() ? 1 : 0, special("SHAGGY").c_str());
			Character *sh = _script->getCharacter("SHAGGY");
			const int shaggySprite = sh ? sh->getSprite() : -1;
			int change = -1;
			for (uint t = 1; t <= kMax && change < 0; t++) {
				if (tick1())
					change = t;
			}
			out += Common::String::format("; next %s at tick %d, Shaggy hidden %d", run.next.c_str(), change,
			                              shaggySprite >= 0 && _currentRoom->isHidden(shaggySprite) ? 1 : 0);
			debugC(3, kDebugScript, "jail check exit handler: %s", out.c_str());
		}
		_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	}

	// The General Store (WP-045).
	{
		const char *const kFront = "ROOM_P10_Front_Of_General_Store";
		const char *const kStore = "ROOM_P15_General_Store_Interior";
		const char *const kStreet2 = "ROOM_P04_Street2";
		const char *const kAlley = "ROOM_P19_Back_Alley";
		const char *const kIdle = "General_Store_Interior.Snake_Idle";
		_script->setGhostChance(nullptr, 0);
		{
			RoomClass *a = createRoomClass(*this, kFront);
			RoomClass *b = createRoomClass(*this, kStore);
			debugC(3, kDebugScript, "store check classes: P10_Front_Of_General_Store %s; P15_General_Store_Interior %s",
			       dynamic_cast<StoreFrontRoom *>(a) ? "front" : a ? "other" : "-", dynamic_cast<GeneralStoreRoom *>(b) ? "store" : b ? "other" : "-");
			delete a;
			delete b;
		}
		auto front = [&]() { return dynamic_cast<StoreFrontRoom *>(_roomClass); };
		auto store = [&]() { return dynamic_cast<GeneralStoreRoom *>(_roomClass); };
		auto placed = [&](const char *object) {
			const int s = Find::sprite(*this, object);
			if (s < 0)
				return Common::String::format("%s none", object + 4);
			return Common::String::format("%s %s mode %d step %u depth %d hit %d%s%s", object + 4, _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
			                              _currentRoom->getStep(s), _currentRoom->getDepth(s), _currentRoom->getHitType(s), _currentRoom->isHidden(s) ? " hidden" : "",
			                              _currentRoom->isRequiresObject(s) ? " requires" : "");
		};
		auto who = [&](const char *name) {
			Character *c = _script->getCharacter(name);
			if (!c)
				return Common::String::format("%s none", name);
			return Common::String::format("%s state %d %s steps %u", name, c->getState(), _currentRoom->getAnim(c->getSprite()).c_str(),
			                              _currentRoom->getStepCount(c->getSprite()));
		};
		auto inSlot = [&](const char *item) {
			for (uint i = 0; i < Inventory::kSlotCount; i++) {
				if (_inventory->getSlot(i) == item)
					return 1;
			}
			return 0;
		};
		auto use = [&](const char *item) {
			_inventory->addItem(item);
			_inventory->removeItem(item);
		};
		auto opening = [&](const char *room) {
			return Common::String::format("'%s'%s", _script->getOpening().c_str(), _script->getMemory(room).openingPlayed ? " played" : "");
		};
		auto enterFront = [&]() {
			fresh.room(kStreet2);
			fresh.room(kFront);
		};
		auto enterStore = [&]() {
			fresh.room(kAlley);
			fresh.room(kStore);
		};

		// The front: the clock, the faceless clock, the opening, Fred.
		{
			newGameForCheck();
			enterFront();
			Common::String out = placed("OBJ_INV_CLOCK") + "; " + placed("OBJ_P10_FRONT_OF_GS_CLOCK_NOFACE") + "; opening " + opening(kFront) + "; " + who("FRED");
			newGameForCheck();
			setGameFlag("MoaningActive", false);
			enterFront();
			out += "; not moaning: opening " + opening(kFront);
			newGameForCheck();
			use("OBJ_INV_HAMMER");
			enterFront();
			out += Common::String::format("; the hammer used: clock %d, ", _inventory->hasItem("OBJ_INV_CLOCK") ? 1 : 0) + placed("OBJ_INV_CLOCK") + "; " +
			       placed("OBJ_P10_FRONT_OF_GS_CLOCK_NOFACE");
			debugC(3, kDebugScript, "store check front: %s", out.c_str());
		}

		// The front's lines: the clock without the hammer, in turn; the
		// door's two; hotspot 3; hotspot 1 scares Scooby.
		{
			newGameForCheck();
			enterFront();
			_script->skip();
			Common::String out = "clock:";
			auto line = [&](const char *object) {
				_script->sendEvent(object, false);
				Common::String s = Common::String::format(" '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
				_script->skip();
				return s;
			};
			for (uint k = 0; k < 4; k++)
				out += line("OBJ_INV_CLOCK");
			_script->sendEvent("OBJ_P10_FRONT_OF_GS_HOTSPOT2", false);
			const Common::String door = _script->getScene();
			out += Common::String::format("; door: %s", door == "Front_of_General_Store.Door_Locked_" || door == "Front_of_General_Store.Door_Lockedb_" ?
			                                                "Front_of_General_Store.Door_Locked[b]_" : door.c_str());
			_script->skip();
			out += "; hotspot 3:" + line("OBJ_P10_FRONT_OF_GS_HOTSPOT3");
			_script->sendEvent("OBJ_P10_FRONT_OF_GS_HOTSPOT1", false);
			out += Common::String::format("; hotspot 1: waiting %d", front()->isScoobyWaiting() ? 1 : 0);
			tick1();
			out += Common::String::format(", no sound playing, tick 1: %s, waiting %d", who("SCOOBY").c_str(), front()->isScoobyWaiting() ? 1 : 0);
			debugC(3, kDebugScript, "store check front lines: %s", out.c_str());
		}

		// The clock and the hammer: Take_Clock_, the hammer used up; out
		// through movie 2, and back with the broken clock.
		{
			newGameForCheck();
			_inventory->addItem("OBJ_INV_HAMMER");
			enterFront();
			_script->skip();
			_inventory->setCursor("ANIM_CURSORRUSTY_HAMMER");
			_script->sendEvent("OBJ_INV_CLOCK", true);
			Common::String out = Common::String::format("scene '%s' held %d, hammer %d in a slot %d, cursor %s, taken %d", _script->getScene().c_str(),
			                                            _script->isBusy() ? 1 : 0, _inventory->hasItem("OBJ_INV_HAMMER") ? 1 : 0, inSlot("OBJ_INV_HAMMER"),
			                                            _inventory->isDefaultCursor() ? "arrow" : "other", front()->isClockTaken() ? 1 : 0);
			tick1();
			out += Common::String::format("; tick 1, the scene on: changed %d", run.next.empty() ? 0 : 1);
			_script->skip();
			const bool changed = tick1();
			out += Common::String::format("; over, next tick: changed %d, next %s movie %d", changed ? 1 : 0, run.next.c_str(), run.movie);
			showRoom(kFront);
			out += Common::String::format("; back: clock %d, ", _inventory->hasItem("OBJ_INV_CLOCK") ? 1 : 0) + placed("OBJ_INV_CLOCK") + "; " +
			       placed("OBJ_P10_FRONT_OF_GS_CLOCK_NOFACE");
			debugC(3, kDebugScript, "store check clock: %s", out.c_str());
		}

		// The store: the snake and the lantern; the snake's idle on tick 1;
		// Shaggy; the hotspots; with the lantern taken; with the blanket used.
		{
			newGameForCheck();
			enterStore();
			Common::String out = placed("OBJ_P15_GSI_SNAKE") + "; " + placed("OBJ_INV_LANTERN") + "; opening " + opening(kStore) + "; " + who("SHAGGY");
			tick1();
			const int snake = store()->getSnake();
			out += Common::String::format("; tick 1: snake %s mode %d, sound %s", _currentRoom->getAnim(snake).c_str(), _currentRoom->getMode(snake),
			                              store()->getLastSound().c_str());
			out += "; hotspots:";
			static const char *const kHot[] = { "OBJ_P15_GSI_HOTSPOT1", "OBJ_P15_GSI_HOTSPOT1", "OBJ_P15_GSI_HOTSPOT2" };
			for (uint k = 0; k < ARRAYSIZE(kHot); k++) {
				_script->sendEvent(kHot[k], false);
				out += Common::String::format(" '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
				_script->skip();
			}
			newGameForCheck();
			_inventory->addItem("OBJ_INV_LANTERN");
			enterStore();
			out += "; lantern taken: " + placed("OBJ_INV_LANTERN");
			newGameForCheck();
			use("OBJ_INV_BLANKET");
			enterStore();
			out += "; blanket used: " + placed("OBJ_P15_GSI_SNAKE") + Common::String::format(", sound %s, sfx %u%%", store()->getLastSound().c_str(),
			                                                                                 _sound->getFilePercent(Sound::kSfx));
			_sound->setFilePercent(kIdle, 100);
			debugC(3, kDebugScript, "store check store: %s", out.c_str());
		}

		// The snake: its three lines in turn; it strikes on its step 0, and
		// idles again after.
		{
			newGameForCheck();
			enterStore();
			tick1();
			Common::String out = "clicks:";
			for (uint k = 0; k < 4; k++) {
				_script->sendEvent("OBJ_P15_GSI_SNAKE", false);
				out += Common::String::format(" '%s' held %d strike %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0, store()->isStrikePending() ? 1 : 0);
				_script->skip();
			}
			const int snake = store()->getSnake();
			int strike = -1, again = -1;
			Common::String at;
			for (uint t = 2; t <= kMax && again < 0; t++) {
				tick1();
				if (strike < 0 && _currentRoom->getAnim(snake) == "ANIM_P15_GSI_SNAKEANIM") {
					strike = t;
					at = Common::String::format("%s mode %d steps %u, sound %s", _currentRoom->getAnim(snake).c_str(), _currentRoom->getMode(snake),
					                            _currentRoom->getStepCount(snake), store()->getLastSound().c_str());
				} else if (strike > 0 && _currentRoom->getAnim(snake) == "ANIM_P15_GSI_SNAKEIDLE") {
					again = t;
					at += Common::String::format("; idle again at tick %d: mode %d, sound %s", t, _currentRoom->getMode(snake), store()->getLastSound().c_str());
				}
			}
			out += Common::String::format("; strike at tick %d: %s", strike, at.c_str());
			newGameForCheck();
			enterStore();
			_script->sendEvent("OBJ_INV_LANTERN", false);
			out += Common::String::format("; the lantern, no blanket: '%s' strike %d, busy %d", _script->getScene().c_str(), store()->isStrikePending() ? 1 : 0,
			                              store()->isBusy() ? 1 : 0);
			_script->skip();
			debugC(3, kDebugScript, "store check snake: %s", out.c_str());
		}

		// The blanket over the snake: its scene, then Fred tosses it.
		{
			newGameForCheck();
			_inventory->addItem("OBJ_INV_BLANKET");
			enterStore();
			_inventory->setCursor("ANIM_CURSORBLANKET");
			_script->sendEvent("OBJ_P15_GSI_SNAKE", true);
			Common::String out = Common::String::format("scene '%s' held %d, blanket %d in a slot %d, cursor %s", _script->getScene().c_str(), _script->isBusy() ? 1 : 0,
			                                            _inventory->hasItem("OBJ_INV_BLANKET") ? 1 : 0, inSlot("OBJ_INV_BLANKET"),
			                                            _inventory->isDefaultCursor() ? "arrow" : "other");
			_script->skip();
			tick1();
			out += Common::String::format("; tick 1: %s, busy %d", who("FRED").c_str(), store()->isBusy() ? 1 : 0);
			int covered = -1;
			for (uint t = 2; t <= kMax && covered < 0; t++) {
				tick1();
				if (store()->isCovered())
					covered = t;
			}
			out += Common::String::format("; covered at tick %d: %s, sfx %u%%, busy %d", covered, placed("OBJ_P15_GSI_SNAKE").c_str(),
			                              _sound->getFilePercent(Sound::kSfx), store()->isBusy() ? 1 : 0);
			_sound->setFilePercent(kIdle, 100);
			debugC(3, kDebugScript, "store check blanket: %s", out.c_str());
		}

		// The lantern with the snake covered: Fred takes it; on his step 62
		// it is in the toolbar; then Click_On_Lantern_. Left halfway, it is
		// taken anyway.
		{
			newGameForCheck();
			use("OBJ_INV_BLANKET");
			enterStore();
			_sound->setFilePercent(kIdle, 100);
			_script->sendEvent("OBJ_INV_LANTERN", false);
			Common::String out = Common::String::format("busy %d", store()->isBusy() ? 1 : 0);
			tick1();
			out += Common::String::format("; tick 1: %s, %s, sound %s", who("FRED").c_str(), placed("OBJ_INV_LANTERN").c_str(), store()->getLastSound().c_str());
			int got = -1, done = -1;
			for (uint t = 2; t <= kMax && done < 0; t++) {
				tick1();
				if (got < 0 && _inventory->hasItem("OBJ_INV_LANTERN")) {
					Character *fred = _script->getCharacter("FRED");
					got = t;
					out += Common::String::format("; in the toolbar at tick %d (Fred's step %u)", t, fred ? _currentRoom->getStep(fred->getSprite()) : 0);
				}
				if (_script->isScenePlaying()) {
					done = t;
					out += Common::String::format("; scene '%s' at tick %d held %d, busy %d", _script->getScene().c_str(), t, _script->isBusy() ? 1 : 0,
					                              store()->isBusy() ? 1 : 0);
				}
			}
			_script->skip();
			newGameForCheck();
			use("OBJ_INV_BLANKET");
			enterStore();
			_sound->setFilePercent(kIdle, 100);
			_script->sendEvent("OBJ_INV_LANTERN", false);
			showRoom(kAlley);
			out += Common::String::format("; clicked, then out: lantern %d", _inventory->hasItem("OBJ_INV_LANTERN") ? 1 : 0);
			debugC(3, kDebugScript, "store check lantern: %s", out.c_str());
		}
		_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	}

	// The Sheriff's office (WP-046).
	{
		const char *const kOffice = "ROOM_P02_Sheriffs_Office";
		const char *const kStreet1 = "ROOM_P03_Street1";
		_script->setGhostChance(nullptr, 0);
		{
			RoomClass *a = createRoomClass(*this, kOffice);
			debugC(3, kDebugScript, "office check classes: P02_Sheriffs_Office %s", dynamic_cast<SheriffsOfficeRoom *>(a) ? "office" : a ? "other" : "-");
			delete a;
		}
		auto office = [&]() { return dynamic_cast<SheriffsOfficeRoom *>(_roomClass); };
		auto placed = [&](const char *object) {
			const int s = Find::sprite(*this, object);
			if (s < 0)
				return Common::String::format("%s none", object + 4);
			return Common::String::format("%s %s mode %d step %u depth %d hit %d%s%s%s", object + 4, _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
			                              _currentRoom->getStep(s), _currentRoom->getDepth(s), _currentRoom->getHitType(s), _currentRoom->isHidden(s) ? " hidden" : "",
			                              _currentRoom->isRequiresObject(s) ? " requires" : "", _currentRoom->isClickable(s) ? " clickable" : "");
		};
		auto who = [&](const char *name) {
			Character *c = _script->getCharacter(name);
			if (!c)
				return Common::String::format("%s none", name);
			return Common::String::format("%s state %d %s steps %u", name, c->getState(), _currentRoom->getAnim(c->getSprite()).c_str(),
			                              _currentRoom->getStepCount(c->getSprite()));
		};
		auto use = [&](const char *item) {
			_inventory->addItem(item);
			_inventory->removeItem(item);
		};
		auto opening = [&]() {
			return Common::String::format("'%s'%s", _script->getOpening().c_str(), _script->getMemory(kOffice).openingPlayed ? " played" : "");
		};
		auto enterOffice = [&]() {
			fresh.room(kStreet1);
			fresh.room(kOffice);
		};
		// Clicked, then the scene skipped and the clickable at rest; the state
		// it moved to and the scene.
		auto clickRest = [&](int sprite) {
			_script->clickClickable(sprite);
			tick1();
			const Common::String state = _script->getClickableState(sprite);
			const Common::String r = Common::String::format(" %c '%s' held %d", state.empty() ? '-' : state.lastChar(), _script->getScene().c_str(),
			                                                _script->isBusy() ? 1 : 0);
			for (uint t = 0; t < kMax && (_script->isScenePlaying() || _script->isClickablePlaying(sprite)); t++) {
				if (_script->isScenePlaying())
					_script->skip();
				tick1();
			}
			return r;
		};

		// The placements; the opening; with the oil can used; with the blanket
		// taken too.
		{
			newGameForCheck();
			enterOffice();
			Common::String out = placed("OBJ_P02_DOOR") + "; " + placed("OBJ_P02_TO_P07") + "; " + placed("OBJ_P02_SHERIFF_HOTSPOT1") + "; " +
			                     placed("OBJ_P02_SHERIFF_HOTSPOT4") + "; opening " + opening();
			newGameForCheck();
			setGameFlag("MoaningActive", false);
			enterOffice();
			out += "; not moaning: opening " + opening();
			newGameForCheck();
			use("OBJ_INV_OILCAN");
			enterOffice();
			out += "; the oil can used: " + placed("OBJ_P02_DOOR") + "; " + placed("OBJ_P02_TO_P07");
			newGameForCheck();
			use("OBJ_INV_OILCAN");
			_inventory->addItem("OBJ_INV_BLANKET");
			enterOffice();
			out += "; and the blanket taken: " + placed("OBJ_P02_DOOR") + "; " + placed("OBJ_P02_TO_P07");
			debugC(3, kDebugScript, "office check placements: %s", out.c_str());
		}

		// Hotspot 1's lines on its B and E; hotspot 4's on A and B, and the
		// sandwich after its C.
		{
			newGameForCheck();
			enterOffice();
			_script->skip();
			Common::String out = "hotspot 1:";
			const int h1 = Find::sprite(*this, "OBJ_P02_SHERIFF_HOTSPOT1");
			for (uint k = 0; k < 6; k++)
				out += clickRest(h1);
			out += "; hotspot 4:";
			const int h4 = office()->getHotspot4();
			out += clickRest(h4);
			out += clickRest(h4);
			_script->clickClickable(h4);
			tick1();
			out += Common::String::format(" C waiting %d", office()->isSandwichWaiting() ? 1 : 0);
			int eat = -1, done = -1;
			Common::String at;
			for (uint t = 2; t <= kMax && done < 0; t++) {
				tick1();
				Character *sh = _script->getCharacter("SHAGGY");
				if (eat < 0 && sh && sh->getState() == Character::kStateSpecial) {
					eat = t;
					at = Common::String::format("%s, sound %s, busy %d", who("SHAGGY").c_str(), office()->getLastSound().c_str(), office()->isBusy() ? 1 : 0);
				} else if (eat > 0 && !office()->isBusy()) {
					done = t;
				}
			}
			out += Common::String::format("; the sandwich at tick %d: %s; busy 0 at tick %d", eat, at.c_str(), done);
			debugC(3, kDebugScript, "office check hotspots: %s", out.c_str());
		}

		// The door without the oil can: Shaggy shakes the bars, then a line in
		// turn, not held. With it: he oils it, and it opens.
		{
			newGameForCheck();
			enterOffice();
			_script->skip();
			Common::String out;
			for (uint k = 0; k < 4; k++) {
				_script->sendEvent("OBJ_P02_DOOR", false);
				const bool busy = office()->isBusy();
				tick1();
				Common::String at = Common::String::format("%s, sound %s", who("SHAGGY").c_str(), office()->getLastSound().c_str());
				int line = -1;
				for (uint t = 2; t <= kMax && line < 0; t++) {
					tick1();
					if (!office()->isDoorClicked())
						line = t;
				}
				out += Common::String::format("%sbusy %d, tick 1: %s; at tick %d '%s' held %d, busy %d", k ? "; " : "", busy ? 1 : 0, at.c_str(), line,
				                              _script->getScene().c_str(), _script->isBusy() ? 1 : 0, office()->isBusy() ? 1 : 0);
				_script->skip();
			}
			debugC(3, kDebugScript, "office check door: %s", out.c_str());

			newGameForCheck();
			_inventory->addItem("OBJ_INV_OILCAN");
			enterOffice();
			_script->skip();
			_inventory->setCursor("ANIM_CURSOROILCAN");
			_script->sendEvent("OBJ_P02_DOOR", true);
			int inSlot = 0;
			for (uint i = 0; i < Inventory::kSlotCount; i++)
				inSlot |= _inventory->getSlot(i) == "OBJ_INV_OILCAN" ? 1 : 0;
			out = Common::String::format("oil can %d in a slot %d, cursor %s, busy %d", _inventory->hasItem("OBJ_INV_OILCAN") ? 1 : 0, inSlot,
			                             _inventory->isDefaultCursor() ? "arrow" : "other", office()->isBusy() ? 1 : 0);
			tick1();
			out += Common::String::format("; tick 1: %s, sound %s, %s", who("SHAGGY").c_str(), office()->getLastSound().c_str(), placed("OBJ_P02_DOOR").c_str());
			int open = -1;
			for (uint t = 2; t <= kMax && open < 0; t++) {
				tick1();
				if (!office()->isDoorClicked())
					open = t;
			}
			out += Common::String::format("; open at tick %d: %s, scene '%s', busy %d", open, placed("OBJ_P02_DOOR").c_str(), _script->getScene().c_str(),
			                              office()->isBusy() ? 1 : 0);
			showRoom(kStreet1);
			showRoom(kOffice);
			out += "; back: " + placed("OBJ_P02_DOOR");
			debugC(3, kDebugScript, "office check oil: %s", out.c_str());
		}

		// The way into the jail waits for the level's snacks; the way back to
		// Street 1 doesn't.
		{
			Common::String out;
			for (int level = 0; level < 3; level++) {
				newGameForCheck();
				setDifficulty(level);
				enterOffice();
				_script->skip();
				const bool kept = _roomClass->leaveBy("OBJ_P02_TO_P07");
				const Common::String line = office()->getLastScene();
				const bool held = _script->isBusy();
				_script->skip();
				for (uint k = 0; k < 5; k++)
					tick1();
				const bool opens = _inventory->getState() != Inventory::kStateParked;
				out += Common::String::format("level %d: kept %d, '%s' held %d, toolbar opening %d; ", level, kept ? 1 : 0, line.c_str(), held ? 1 : 0, opens ? 1 : 0);
			}
			newGameForCheck();
			setDifficulty(1);
			enterOffice();
			_script->skip();
			const bool street = _roomClass->leaveBy("OBJ_P02_TO_P03");
			bool fed[3];
			for (uint k = 0; k < 3; k++) {
				fed[k] = _roomClass->leaveBy("OBJ_P02_TO_P07");
				_script->skip();
				_script->snackEaten();
			}
			out += Common::String::format("to Street 1 kept %d; level 1 after 0, 1, 2 snacks kept %d %d %d", street ? 1 : 0, fed[0] ? 1 : 0, fed[1] ? 1 : 0, fed[2] ? 1 : 0);
			const bool three = _roomClass->leaveBy("OBJ_P02_TO_P07");
			out += Common::String::format(", after 3 kept %d", three ? 1 : 0);
			setDifficulty(0);
			debugC(3, kDebugScript, "office check jail way: %s", out.c_str());
		}
		_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	}

	// The back alley (WP-047).
	{
		const char *const kAlley = "ROOM_P19_Back_Alley";
		const char *const kStore = "ROOM_P15_General_Store_Interior";
		const char *const kVault = "ROOM_P16_Bank_Vault";
		_script->setGhostChance(nullptr, 0);
		{
			RoomClass *a = createRoomClass(*this, kAlley);
			debugC(3, kDebugScript, "alley check classes: P19_Back_Alley %s", dynamic_cast<BackAlleyRoom *>(a) ? "alley" : a ? "other" : "-");
			delete a;
		}
		auto alley = [&]() { return dynamic_cast<BackAlleyRoom *>(_roomClass); };
		auto who = [&](const char *name) {
			Character *c = _script->getCharacter(name);
			if (!c)
				return Common::String::format("%s none", name);
			return Common::String::format("%s state %d %s", name, c->getState(), _currentRoom->getAnim(c->getSprite()).c_str());
		};
		auto placed = [&](int s) {
			if (s < 0)
				return Common::String("none");
			return Common::String::format("%s mode %d step %u depth %d hit %d%s", _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s), _currentRoom->getStep(s),
			                              _currentRoom->getDepth(s), _currentRoom->getHitType(s), _currentRoom->isClickable(s) ? " clickable" : "");
		};

		// Entry: the walk-ins by the room left, the snakes' sound, the
		// hotspots, the opening.
		{
			newGameForCheck();
			fresh.room(kStore);
			fresh.room(kAlley);
			Common::String out = Common::String::format("from the store: %s, %s; sound %s; hotspot 3 %s; hotspot 1 %s; opening '%s'%s", who("FRED").c_str(),
			                                            who("SHAGGY").c_str(), alley()->getLastSound().c_str(), placed(alley()->getHotspot3()).c_str(),
			                                            placed(alley()->getHotspot1()).c_str(), _script->getOpening().c_str(),
			                                            _script->getMemory(kAlley).openingPlayed ? " played" : "");
			newGameForCheck();
			fresh.room(kVault);
			fresh.room(kAlley);
			out += Common::String::format("; from the vault: %s, %s", who("FRED").c_str(), who("SHAGGY").c_str());
			newGameForCheck();
			fresh.room("ROOM_Open_Safe");
			fresh.room(kAlley);
			out += Common::String::format("; from the open safe: %s", who("SHAGGY").c_str());
			debugC(3, kDebugScript, "alley check entry: %s", out.c_str());
		}

		// Hotspot 3: its A, no line; its B, a line on the tick it starts.
		// Hotspot 1: Daphne and Scooby scared on its step 8.
		{
			newGameForCheck();
			fresh.room(kStore);
			fresh.room(kAlley);
			_script->skip();
			const int h3 = alley()->getHotspot3();
			_script->clickClickable(h3);
			tick1();
			Common::String out = Common::String::format("hotspot 3 A: scene '%s', next %s", _script->getScene().c_str(), _script->getClickableState(h3).c_str());
			for (uint t = 0; t < kMax && _script->isClickablePlaying(h3); t++)
				tick1();
			_script->clickClickable(h3);
			tick1();
			const Common::String line = _script->getScene();
			out += Common::String::format("; B on tick 1: %s held %d, %s", line == "Back_Alley.HOTSPOT3B_" || line == "Back_Alley.HOTSPOT3B2_" ? "Back_Alley.HOTSPOT3B[2]_" : line.c_str(),
			                              _script->isBusy() ? 1 : 0, placed(h3).c_str());
			for (uint t = 0; t < kMax && (_script->isScenePlaying() || _script->isClickablePlaying(h3)); t++) {
				if (_script->isScenePlaying())
					_script->skip();
				tick1();
			}
			const int h1 = alley()->getHotspot1();
			_script->clickClickable(h1);
			tick1();
			int scared = -1;
			for (uint t = 2; t <= kMax && scared < 0; t++) {
				tick1();
				Character *d = _script->getCharacter("DAPHNE");
				if (d && d->getState() == Character::kStateScared)
					scared = t;
			}
			out += Common::String::format("; hotspot 1: scared at tick %d (step %u): %s, %s", scared, _currentRoom->getStep(h1), who("DAPHNE").c_str(), who("SCOOBY").c_str());
			debugC(3, kDebugScript, "alley check hotspots: %s", out.c_str());
		}

		// The way into the store waits for the level's snacks; the way to the
		// vault doesn't.
		{
			Common::String out;
			for (int level = 0; level < 3; level++) {
				newGameForCheck();
				setDifficulty(level);
				fresh.room(kStore);
				fresh.room(kAlley);
				_script->skip();
				const bool kept = _roomClass->leaveBy("OBJ_P19_TO_P15");
				const Common::String line = alley()->getLastScene();
				const bool held = _script->isBusy();
				_script->skip();
				for (uint k = 0; k < 5; k++)
					tick1();
				const bool opens = _inventory->getState() != Inventory::kStateParked;
				out += Common::String::format("level %d: kept %d, '%s' held %d, toolbar opening %d; ", level, kept ? 1 : 0, line.c_str(), held ? 1 : 0, opens ? 1 : 0);
			}
			newGameForCheck();
			setDifficulty(1);
			fresh.room(kStore);
			fresh.room(kAlley);
			_script->skip();
			const bool vault = _roomClass->leaveBy("OBJ_P19_TO_P16");
			bool fed[3];
			for (uint k = 0; k < 3; k++) {
				fed[k] = _roomClass->leaveBy("OBJ_P19_TO_P15");
				_script->skip();
				_script->snackEaten();
			}
			const bool three = _roomClass->leaveBy("OBJ_P19_TO_P15");
			out += Common::String::format("to the vault kept %d; level 1 after 0, 1, 2 snacks kept %d %d %d, after 3 kept %d", vault ? 1 : 0, fed[0] ? 1 : 0,
			                              fed[1] ? 1 : 0, fed[2] ? 1 : 0, three ? 1 : 0);
			setDifficulty(0);
			debugC(3, kDebugScript, "alley check store way: %s", out.c_str());
		}
		_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	}
	newGameForCheck();
	_checking = false;
	debugC(3, kDebugScript, "chase check: done");
}

} // End of namespace Scooby
