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
#include "terraglyph/garden_rooms.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkGarden() {
	// Phantom's gardener's house, garden and garden plaque (WP-060).
	const char *const kHouse = "ROOM_P05_Gardeners_House";
	const char *const kGarden = "ROOM_P06_Garden";
	const char *const kPlaque = "ROOM_Garden_Plaque";
	const char *const kTown = "ROOM_P04_Town";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kHouse;
	if (!found) {
		debugC(3, kDebugScript, "garden check: no gardener's house");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *a = createRoomClass(*this, kHouse);
		RoomClass *b = createRoomClass(*this, kGarden);
		RoomClass *c = createRoomClass(*this, kPlaque);
		debugC(3, kDebugScript, "garden check classes: Gardener's House %s; Garden %s; Plaque %s", dynamic_cast<GardenersHouseRoom *>(a) ? "house" : "-",
		       dynamic_cast<GardenRoom *>(b) ? "garden" : "-", dynamic_cast<GardenPlaqueRoom *>(c) ? "plaque" : "-");
		delete a;
		delete b;
		delete c;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto find = [&](const char *object) {
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == object)
				return (int)s;
		}
		return -1;
	};
	auto sprite = [&](int s) {
		if (s < 0)
			return Common::String("none");
		Room &r = *_currentRoom;
		const int p = r.getParent(s);
		return Common::String::format("%s %s mode %d step %u depth %d hit %d under %s%s%s%s", r.getObjectName(s).c_str(), r.getAnim(s).c_str(), r.getMode(s),
		                              r.getStep(s), r.getDepth(s), r.getHitType(s), p == 0 ? "root" : r.getLabel(p).c_str(),
		                              r.isRequiresObject(s) ? " requires" : "", r.isClickable(s) ? " clickable" : "",
		                              r.getDestination(s).empty() ? "" : " exit");
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
	auto playing = [&](const char *const *names, uint n) {
		Common::String out;
		for (uint i = 0; i < n; i++) {
			if (_sound->isClipPlaying(names[i]))
				out += Common::String::format("%s%s", out.empty() ? "" : " ", names[i]);
		}
		return out.empty() ? Common::String("none") : out;
	};
	static const char *const kHouseSounds[] = { "Gardeners_House.Puzzle_Right", "Gardeners_House.Door", "Gardeners_House.Puzzle_Wrong" };
	// A fresh story with these flags.
	auto story = [&](bool crest, bool screaming) {
		newGameForCheck();
		setGameFlag("CrestPuzzle", crest);
		setGameFlag("Screaming", screaming);
		_sound->stop();
	};
	auto entry = [&]() {
		return Common::String::format("opening '%s' held %d; walks in: %s", _script->getOpening().c_str(), _script->isBusy() ? 1 : 0, walker().c_str());
	};

	// The house: its placements, without and with CrestPuzzle; the entries.
	{
		story(false, false);
		showRoom(kTown);
		showRoom(kHouse);
		GardenersHouseRoom *h = dynamic_cast<GardenersHouseRoom *>(getRoomClass(kHouse));
		Common::String out = Common::String::format("unsolved: plaque %s; way to the garden %s; door %s; from the town: %s; scream %s",
		                                            sprite(find("OBJ_P05_GARDENERS_HOUSE_PLACQUE")).c_str(), sprite(find("OBJ_P05_TO_P06")).c_str(),
		                                            sprite(h->getDoor()).c_str(), entry().c_str(), h->getScream().empty() ? "none" : "one");
		story(true, false);
		showRoom(kGarden);
		showRoom(kHouse);
		out += Common::String::format("; solved: plaque %s; way to the garden %s; door %s; from the garden: %s", sprite(find("OBJ_P05_GARDENERS_HOUSE_PLACQUE")).c_str(),
		                              sprite(find("OBJ_P05_TO_P06")).c_str(), sprite(h->getDoor()).c_str(), entry().c_str());
		uint intros = 0, screams = 0;
		for (uint k = 0; k < 100; k++) {
			story(false, true);
			showRoom(kTown);
			showRoom(kHouse);
			const Common::String &o = _script->getOpening();
			if (o == "Gardeners_House.Intro_")
				intros |= 1;
			else if (o == "Gardeners_House.Intro2_")
				intros |= 2;
			const Common::String &s = h->getScream();
			if (s.hasPrefix("Gardeners_House.Jane_Screaming") && s.size() == 31)
				screams |= 1 << (s[30] - '0');
		}
		Common::String sc;
		for (int v = 0; v < 10; v++) {
			if (screams & (1 << v))
				sc += Common::String::format(" %d", v);
		}
		out += Common::String::format("; screaming, over 100 visits: Intro_ %d, Intro2_ %d, screams%s", intros & 1 ? 1 : 0, intros & 2 ? 1 : 0, sc.c_str());
		debugC(3, kDebugScript, "garden check house: %s", out.c_str());
	}

	// Back from the plaque: untried, tried and wrong four times, solved.
	{
		Common::String out;
		story(false, false);
		showRoom(kPlaque);
		showRoom(kHouse);
		out = Common::String::format("untried: %s, sounds %s", entry().c_str(), playing(kHouseSounds, 3).c_str());
		for (uint k = 0; k < 4; k++) {
			showRoom(kPlaque);
			_script->sendEvent("OBJ_GARDEN_PLAQUE_LION", false);
			_sound->stop();
			showRoom(kHouse);
			out += Common::String::format("; tried %u: opening '%s', sounds %s", k + 1, _script->getOpening().c_str(), playing(kHouseSounds, 3).c_str());
			_script->skip();
		}
		story(true, false);
		showRoom(kPlaque);
		showRoom(kHouse);
		GardenersHouseRoom *h = dynamic_cast<GardenersHouseRoom *>(getRoomClass(kHouse));
		out += Common::String::format("; solved: opening '%s', sounds %s, door %s", _script->getOpening().c_str(), playing(kHouseSounds, 3).c_str(),
		                              sprite(h->getDoor()).c_str());
		debugC(3, kDebugScript, "garden check return: %s", out.c_str());
	}

	// The house's plaque: Big Head's lines in turn, then, with a ball, the
	// puzzle.
	{
		story(false, false);
		showRoom(kHouse);
		Common::String out;
		for (uint k = 0; k < 4; k++) {
			_script->sendEvent("OBJ_P05_GARDENERS_HOUSE_PLACQUE", false);
			tick1();
			out += Common::String::format("%s%s '%s' on %s back to %s", k ? ", " : "", next.c_str(), getBigHeadScene().c_str(), getBigHeadBackground().c_str(),
			                              getBigHeadNext().c_str());
		}
		out += Common::String::format("; PlaqueSeen %s", getGameString("PlaqueSeen").c_str());
		static const char *const kBalls[] = { "RedBallFound", "YellowBallFound", "BlueBallFound" };
		for (uint b = 0; b < 3; b++) {
			story(false, false);
			setGameFlag(kBalls[b], true);
			showRoom(kHouse);
			_script->sendEvent("OBJ_P05_GARDENERS_HOUSE_PLACQUE", false);
			tick1();
			out += Common::String::format("; %s: to %s", kBalls[b], next.c_str());
		}
		debugC(3, kDebugScript, "garden check plaque: %s", out.c_str());
	}

	// The garden: the tracks without and with MazeEntered, and their lines.
	{
		story(false, false);
		showRoom(kHouse);
		showRoom(kGarden);
		Common::String out = Common::String::format("tracks %s; from the house: %s", sprite(find("OBJ_P06_GARDEN_TRACKS")).c_str(), walker().c_str());
		story(false, false);
		setGameFlag("MazeEntered", true);
		showRoom(kGarden);
		out += Common::String::format("; maze entered: tracks %s; lines", sprite(find("OBJ_P06_GARDEN_TRACKS")).c_str());
		_script->skip();
		for (uint k = 0; k < 4; k++) {
			_script->sendEvent("OBJ_P06_GARDEN_TRACKS", true);
			out += Common::String::format(" '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
			_script->skip();
		}
		debugC(3, kDebugScript, "garden check garden: %s", out.c_str());
	}

	// The garden plaque: its buttons, the mouse over one, the crest pressed
	// right, a button pressed twice, and a wrong order.
	{
		auto plaque = [&]() { return dynamic_cast<GardenPlaqueRoom *>(getRoomClass(kPlaque)); };
		static const int kCrest[4] = { 2, 4, 1, 3 };
		auto setCrest = [&](const int crest[4]) {
			for (uint i = 0; i < 4; i++)
				setGameInt(Common::String::format("CrestSequence%u", i), crest[i]);
		};
		story(false, false);
		setCrest(kCrest);
		showRoom(kPlaque);
		GardenPlaqueRoom *p = plaque();
		Common::String out;
		for (uint i = 0; i < GardenPlaqueRoom::kButtons; i++)
			out += Common::String::format("%s%s", i ? "; " : "", sprite(p->getButton(i)).c_str());
		_hover = p->getButton(2);
		tick1();
		out += Common::String::format("; the mouse on the deer: steps %u %u %u %u", _currentRoom->getStep(p->getButton(0)), _currentRoom->getStep(p->getButton(1)),
		                              _currentRoom->getStep(p->getButton(2)), _currentRoom->getStep(p->getButton(3)));
		_hover = -1;
		tick1();
		debugC(3, kDebugScript, "garden check buttons: %s", out.c_str());

		// Right: deer (2), dragon (4), bush (1), lion (3), the deer twice.
		static const uint kOrder[] = { 2, 3, 3, 1, 0 };
		out.clear();
		for (uint k = 0; k < ARRAYSIZE(kOrder); k++) {
			_script->sendEvent(GardenPlaqueRoom::getButtonObject(kOrder[k]), true);
			tick1();
			out += Common::String::format("%s%s: step %u, presses %d, right %d", k ? "; " : "", GardenPlaqueRoom::getButtonObject(kOrder[k]) + 18,
			                              _currentRoom->getStep(p->getButton(kOrder[k])), p->getPresses(), p->isAllRight() ? 1 : 0);
			if (!next.empty())
				break;
		}
		out += Common::String::format("; then to %s, busy %d, CrestPuzzle %s, tried %d, Button_Click playing %d", next.c_str(), p->isBusy() ? 1 : 0,
		                              getGameString("CrestPuzzle").c_str(), GardenPlaqueRoom::wasTried() ? 1 : 0,
		                              _sound->isClipPlaying("Gardeners_House.Button_Click") ? 1 : 0);
		// Wrong: lion first.
		story(false, false);
		setCrest(kCrest);
		showRoom(kPlaque);
		p = plaque();
		static const uint kWrong[] = { 0, 1, 2, 3 };
		for (uint k = 0; k < ARRAYSIZE(kWrong); k++) {
			_script->sendEvent(GardenPlaqueRoom::getButtonObject(kWrong[k]), true);
			tick1();
		}
		out += Common::String::format("; lion, bush, deer, dragon: right %d, to %s, CrestPuzzle %s", p->isAllRight() ? 1 : 0, next.c_str(),
		                              getGameString("CrestPuzzle").c_str());
		newGameForCheck();
		out += Common::String::format("; a new game: tried %d", GardenPlaqueRoom::wasTried() ? 1 : 0);
		debugC(3, kDebugScript, "garden check crest: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Terraglyph
