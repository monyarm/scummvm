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
#include "scooby/dining_rooms.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

void ScoobyEngine::checkDining() {
	// Phantom's dining room (WP-063).
	const char *const kDining = "ROOM_P12_Dining_Room";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kDining;
	if (!found) {
		debugC(3, kDebugScript, "dining check: no dining room");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kDining);
		debugC(3, kDebugScript, "dining check classes: Dining Room %s", dynamic_cast<DiningRoomRoom *>(c) ? "dining room" : "-");
		delete c;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto dining = [&]() { return dynamic_cast<DiningRoomRoom *>(getRoomClass(kDining)); };
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
		const char *kind = r.isClickable(s) ? "clickable" : r.isInventoryObject(s) ? "inventory" : !r.getDestination(s).empty() ? "exit" : "other";
		return Common::String::format("%s %s %s depth %d hit %d under %s hidden %d", kind, r.getObjectName(s).c_str(), r.getAnim(s).c_str(), r.getDepth(s),
		                              r.getHitType(s), p == 0 ? "root" : r.getLabel(p).c_str(), r.isHidden(s) ? 1 : 0);
	};
	// A fresh story; Maria's movie already played unless @p movieToCome.
	auto story = [&](bool movieToCome) {
		newGameForCheck();
		setGameString("Whodunit", "JESTER");
		dining()->setMoviePlayed(!movieToCome);
		_sound->stop();
	};
	// Run ticks, skipping each scene as it starts; the scenes in order.
	auto scenes = [&](uint maxTicks, bool &acted) {
		Common::String out, last;
		acted = false;
		for (uint t = 0; t < maxTicks; t++) {
			tick1();
			Character *maria = _script->getCharacter("MARIA");
			acted = acted || (maria && maria->getState() == Character::kStateAct);
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

	// The placements, before and after Maria's game is won.
	{
		Common::String out;
		for (int won = 0; won < 2; won++) {
			story(false);
			setGameFlag("MariaPuzzle", won != 0);
			showRoom(kDining);
			const Common::String burger = sprite(find("OBJ_INV_BURGER"));
			out += Common::String::format("%swon %d: burger %s", won ? "; " : "", won, burger.c_str());
		}
		static const char *const kObjects[] = { "OBJ_P12_DINING_ROOM_MUG", "OBJ_P12_DINING_ROOM_HOTSPOT1", "OBJ_P12_DINING_ROOM_HOTSPOT2",
		                                        "OBJ_P12_DINING_ROOM_HOTSPOT3", "OBJ_P12_DINING_ROOM_HOTSPOT4" };
		for (uint i = 0; i < ARRAYSIZE(kObjects); i++) {
			const Common::String one = sprite(find(kObjects[i]));
			out += Common::String::format("; %s", one.c_str());
		}
		const int mug = find("OBJ_P12_DINING_ROOM_MUG");
		out += Common::String::format(" to %s", mug >= 0 ? _currentRoom->getDestination(mug).c_str() : "-");
		debugC(3, kDebugScript, "dining check placements: %s", out.c_str());
	}

	// Entry: the first time Maria's movie into her game; after, she is met
	// and one snack is needed; won, her clue.
	{
		story(true);
		showRoom("ROOM_P11_Room_Of_Doors");
		showRoom(kDining);
		const int played = dining()->isMoviePlayed() ? 1 : 0;
		const int met = _inventory->hasMetSuspect("MARIA") ? 1 : 0;
		tick1();
		Common::String out = Common::String::format("first: to %s movie %d, MariaMoviePlayed %d, met %d", next.c_str(), movie, played, met);
		story(false);
		showRoom(kDining);
		const int met2 = _inventory->hasMetSuspect("MARIA") ? 1 : 0;
		const uint snacks = _script->getSnacksNeeded();
		const Common::String scene = _script->getScene();
		tick1();
		out += Common::String::format("; again: to '%s', met %d, snacks needed %u, scene '%s'", next.c_str(), met2, snacks, scene.c_str());
		story(false);
		setGameFlag("MariaPuzzle", true);
		showRoom(kDining);
		const Common::String clueScene = _script->getScene();
		const int held = _script->isBusy() ? 1 : 0;
		const int owner = isClueOwner("MARIA") ? 1 : 0;
		const Common::String clue = getGameString("CluesFound0");
		const bool own = clue == "MARIA_LICENSE" || clue == "MARIA_COUPON" || clue == "MARIA_KEYS" || clue == "MARIA_BOOTS" || clue == "MARIA_BOW";
		const int giving = dining()->isGiving() ? 1 : 0;
		out += Common::String::format("; won: scene '%s' held %d, owner %d, one of her own clues %d, giving %d", clueScene.c_str(), held, owner, own ? 1 : 0, giving);
		showRoom("ROOM_P11_Room_Of_Doors");
		showRoom(kDining);
		out += Common::String::format("; won again: scene '%s', giving %d", _script->getScene().c_str(), dining()->isGiving() ? 1 : 0);
		debugC(3, kDebugScript, "dining check entry: %s", out.c_str());
	}

	// Her clue given: GIVES, then the thanks and the suspects.
	{
		story(false);
		setGameFlag("MariaPuzzle", true);
		showRoom(kDining);
		bool acted = false;
		const Common::String s = scenes(4000, acted);
		debugC(3, kDebugScript, "dining check clue: scenes %s; Maria acted %d; then to %s", s.c_str(), acted ? 1 : 0, next.c_str());
	}

	// Maria clicked with the arrow: before her game is won, to it; after,
	// nothing.
	{
		Common::String out;
		for (int won = 0; won < 2; won++) {
			story(false);
			setGameFlag("MariaPuzzle", won != 0);
			if (won)
				giveClue("MARIA");
			showRoom(kDining);
			_script->skip();
			Character *maria = _script->getCharacter("MARIA");
			dining()->click(maria ? maria->getSprite() : -1);
			bool acted = false;
			const Common::String s = scenes(won ? 100 : 1000, acted);
			out += Common::String::format("%swon %d: to '%s' movie %d, scenes %s", won ? "; " : "", won, next.c_str(), movie, s.c_str());
		}
		debugC(3, kDebugScript, "dining check Maria: %s", out.c_str());
	}

	// Hotspot 2 scares the gang.
	{
		story(false);
		showRoom(kDining);
		_script->skip();
		_script->clickClickable(find("OBJ_P12_DINING_ROOM_HOTSPOT2"));
		tick1();
		static const char *const kGang[] = { "DAPHNE", "SHAGGY", "SCOOBY", "FRED", "VELMA" };
		Common::String states;
		for (uint g = 0; g < ARRAYSIZE(kGang); g++) {
			Character *ch = _script->getCharacter(kGang[g]);
			states += Common::String::format("%s%s %d", g ? " " : "", kGang[g], ch ? ch->getState() : -1);
		}
		debugC(3, kDebugScript, "dining check hotspot: clicked: %s", states.c_str());
	}

	// The idle chatter and Maria's idle line.
	{
		story(false);
		showRoom(kDining);
		_script->skip();
		DiningRoomRoom *c = dining();
		Character *velma = _script->getCharacter("VELMA");
		if (velma)
			velma->setState(*_currentRoom, *_sound, Character::kStateIdle);
		c->idle();
		const Common::String scene = _script->getScene();
		const int held = _script->isBusy() ? 1 : 0;
		const int acting = velma && velma->getState() == Character::kStateAct ? 1 : 0;
		Common::String out = Common::String::format("idle: scene '%s' held %d, Velma acting %d", scene.c_str(), held, acting);
		const Common::String base = _script->getBaseIdleLine("MARIA");
		const Common::String before = c->idleLine(*_script, "MARIA");
		out += Common::String::format("; Maria's line before her game is won the base one %d", before == base ? 1 : 0);
		setGameFlag("MariaPuzzle", true);
		uint seen = 0;
		for (uint k = 0; k < 200; k++) {
			const Common::String l = c->idleLine(*_script, "MARIA");
			seen |= l == "Dining_Room.1010I1_1_MARIA" ? 1 : l == "Dining_Room.1010I3_1_MARIA" ? 2 : l == "Dining_Room.1010I4_1_MARIA" ? 4 : 8;
		}
		out += Common::String::format("; won: 1010I1_ %d, 1010I3_ %d, 1010I4_ %d, other %d", seen & 1 ? 1 : 0, seen & 2 ? 1 : 0, seen & 4 ? 1 : 0, seen & 8 ? 1 : 0);
		const Common::String fred = c->idleLine(*_script, "FRED");
		const Common::String fredBase = _script->getBaseIdleLine("FRED");
		out += Common::String::format("; Fred's the base one %d", fred == fredBase ? 1 : 0);
		debugC(3, kDebugScript, "dining check idle: %s", out.c_str());
	}

	// The profile.
	{
		story(true);
		DiningRoomRoom *c = dining();
		c->reset();
		Common::INIFile ini;
		c->saveKeys(ini);
		Common::String a;
		ini.getKey("MariaMoviePlayed", kDining, a);
		Common::String out = Common::String::format("a new game: MariaMoviePlayed=%s", a.c_str());
		showRoom(kDining);
		tick1();
		Common::INIFile ini2;
		c->saveKeys(ini2);
		ini2.getKey("MariaMoviePlayed", kDining, a);
		c->reset();
		c->loadKeys(ini2);
		out += Common::String::format("; after a visit: MariaMoviePlayed=%s; loaded: played %d", a.c_str(), c->isMoviePlayed() ? 1 : 0);
		debugC(3, kDebugScript, "dining check profile: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Scooby
