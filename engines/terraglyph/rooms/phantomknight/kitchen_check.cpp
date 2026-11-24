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
#include "terraglyph/rooms/phantomknight/kitchen.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/class.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkKitchen() {
	// Phantom's kitchen (WP-065).
	const char *const kKitchen = "ROOM_P13_Kitchen";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kKitchen;
	if (!found) {
		debugC(3, kDebugScript, "kitchen check: no kitchen");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kKitchen);
		debugC(3, kDebugScript, "kitchen check classes: Kitchen %s", dynamic_cast<KitchenRoom *>(c) ? "kitchen" : "-");
		delete c;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto kitchen = [&]() { return dynamic_cast<KitchenRoom *>(getRoomClass(kKitchen)); };
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
		const char *kind = r.isRequiresObject(s) ? "requires" : !r.getDestination(s).empty() ? "exit" : "other";
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
	auto use = [&](const char *item) {
		_inventory->addItem(item);
		_inventory->removeItem(item);
	};
	// A fresh story: Whodunit JESTER, so Ewan's clue is his own.
	auto story = [&]() {
		newGameForCheck();
		setGameString("Whodunit", "JESTER");
		_sound->stop();
	};
	auto ewanState = [&]() {
		Character *e = _script->getCharacter("EWAN");
		if (!e)
			return Common::String("none");
		return Common::String::format("state %d %s depth %d hit %d", e->getState(), _currentRoom->getAnim(e->getSprite()).c_str(), _currentRoom->getDepth(e->getSprite()),
		                              _currentRoom->getHitType(e->getSprite()));
	};

	// The placements: a new game; the quiz won with the screwdriver; and the
	// full bottle had.
	{
		Common::String out;
		for (int k = 0; k < 3; k++) {
			story();
			if (k >= 1) {
				setGameFlag("EwanPuzzle", true);
				_inventory->addItem("OBJ_INV_SCREWDRIVER");
				giveClue("EWAN");
			}
			if (k == 2)
				use("OBJ_INV_BOTTLE_FULL");
			showRoom(kKitchen);
			static const char *const kCase[] = { "a new game", "won with the screwdriver", "and the full bottle had" };
			const Common::String grease = sprite(find("OBJ_P13_KITCHEN_HOTSPOT1"));
			const Common::String way = sprite(find("OBJ_P13_TO_P17"));
			out += Common::String::format("%s%s: grease %s, to the storage room %s", k ? "; " : "", kCase[k], grease.c_str(), way.c_str());
		}
		debugC(3, kDebugScript, "kitchen check placements: %s", out.c_str());
	}

	// The walk-ins.
	{
		Common::String out;
		static const char *const kFrom[] = { "ROOM_P12_Dining_Room", "ROOM_P17_Kitchen_Storage", "ROOM_P04_Town" };
		for (uint i = 0; i < ARRAYSIZE(kFrom); i++) {
			story();
			showRoom(kFrom[i]);
			showRoom(kKitchen);
			out += Common::String::format("%sfrom %s: %s", i ? "; " : "", kFrom[i] + 5, walker().c_str());
		}
		debugC(3, kDebugScript, "kitchen check walk-ins: %s", out.c_str());
	}

	// Ewan on entry: before his quiz; won, his clue; then stirring,
	// sleeping with the screwdriver, stirring with the mirror used.
	{
		story();
		showRoom(kKitchen);
		Common::String out = Common::String::format("not won: %s", ewanState().c_str());
		story();
		setGameFlag("EwanPuzzle", true);
		showRoom(kKitchen);
		const Common::String scene = _script->getScene();
		const int held = _script->isBusy() ? 1 : 0;
		const Common::String clue = getGameString("CluesFound0");
		const bool own = clue == "EWAN_COUPON" || clue == "EWAN_BELLOWS" || clue == "EWAN_K" || clue == "EWAN_FILM" || clue == "EWAN_SPOON";
		out += Common::String::format("; won: scene '%s' held %d, his own clue %d, giving %d", scene.c_str(), held, own ? 1 : 0, kitchen()->isGiving() ? 1 : 0);
		for (int k = 0; k < 3; k++) {
			story();
			setGameFlag("EwanPuzzle", true);
			giveClue("EWAN");
			if (k >= 1)
				_inventory->addItem("OBJ_INV_SCREWDRIVER");
			if (k == 2)
				use("OBJ_INV_MIRROR");
			showRoom(kKitchen);
			static const char *const kCase[] = { "given", "the screwdriver", "the mirror used" };
			const int sleeping = kitchen()->isSleeping() ? 1 : 0;
			const int snore = _sound->isClipPlaying("Kitchen.Ewan_Sleeping") ? 1 : 0;
			out += Common::String::format("; %s: %s, sleeping %d, Ewan_Sleeping %d", kCase[k], ewanState().c_str(), sleeping, snore);
		}
		debugC(3, kDebugScript, "kitchen check Ewan: %s", out.c_str());
	}

	// His clue given: GIVES, then the thanks and the suspects.
	{
		story();
		setGameFlag("EwanPuzzle", true);
		showRoom(kKitchen);
		Common::String out, last;
		bool acted = false;
		for (uint t = 0; t < 4000 && next.empty(); t++) {
			tick1();
			Character *e = _script->getCharacter("EWAN");
			acted = acted || (e && e->getState() == Character::kStateAct);
			const Common::String s = _script->getScene();
			if (!s.empty() && s != last) {
				out += Common::String::format("%s%s", out.empty() ? "" : " ", s.c_str());
				last = s;
				_script->skip();
			} else if (s.empty()) {
				last.clear();
			}
		}
		debugC(3, kDebugScript, "kitchen check clue: scenes %s; Ewan acted %d; then to %s", out.empty() ? "none" : out.c_str(), acted ? 1 : 0, next.c_str());
	}

	// A click on Ewan: before his quiz, Big Head then the quiz; won, his
	// lines; asleep, his scenes.
	{
		story();
		showRoom(kKitchen);
		Character *e = _script->getCharacter("EWAN");
		kitchen()->click(e->getSprite());
		tick1();
		Common::String out = Common::String::format("first: to %s, '%s' on %s back to %s, met %d, EwanIntro %d", next.c_str(), getBigHeadScene().c_str(),
		                                            getBigHeadBackground().c_str(), getBigHeadNext().c_str(), _inventory->hasMetSuspect("EWAN") ? 1 : 0,
		                                            kitchen()->isIntroDone() ? 1 : 0);
		showRoom(kKitchen);
		kitchen()->click(_script->getCharacter("EWAN")->getSprite());
		tick1();
		out += Common::String::format("; again: to %s movie %d", next.c_str(), movie);

		story();
		setGameFlag("EwanPuzzle", true);
		giveClue("EWAN");
		showRoom(kKitchen);
		Common::String lines;
		for (int k = 0; k < 4; k++) {
			kitchen()->click(_script->getCharacter("EWAN")->getSprite());
			tick1();
			const Common::String l = kitchen()->getLine();
			const int locked = _script->isSpeechLocked() ? 1 : 0;
			lines += Common::String::format("%s%s (locked %d)", k ? " " : "", l.c_str(), locked);
			_sound->stop();
			tick1();
		}
		out += Common::String::format("; won, awake: %s; the lock free after %d", lines.c_str(), _script->isSpeechLocked() ? 0 : 1);

		story();
		setGameFlag("EwanPuzzle", true);
		giveClue("EWAN");
		_inventory->addItem("OBJ_INV_SCREWDRIVER");
		showRoom(kKitchen);
		Common::String scenes;
		for (int k = 0; k < 4; k++) {
			kitchen()->click(_script->getCharacter("EWAN")->getSprite());
			tick1();
			scenes += Common::String::format("%s'%s' held %d", k ? " " : "", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
			_script->skip();
			tick1();
		}
		out += Common::String::format("; asleep: %s", scenes.c_str());
		debugC(3, kDebugScript, "kitchen check click: %s", out.c_str());
	}

	// The grease: the arrow; the empty bottle, which Fred fills.
	{
		story();
		setGameFlag("EwanPuzzle", true);
		giveClue("EWAN");
		_inventory->addItem("OBJ_INV_SCREWDRIVER");
		_inventory->addItem("OBJ_INV_BOTTLE_EMPTY");
		showRoom(kKitchen);
		KitchenRoom *c = kitchen();
		_script->sendEvent("OBJ_P13_KITCHEN_HOTSPOT1", false);
		Common::String out = Common::String::format("the arrow: scene '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		_script->skip();
		tick1();
		_inventory->setCursor("ANIM_CURSORSPORTS_BOTTLE_EMPTY");
		_script->sendEvent("OBJ_P13_KITCHEN_HOTSPOT1", true);
		out += Common::String::format("; the bottle: greasing %d, busy %d", c->isGreasing() ? 1 : 0, c->isBusy() ? 1 : 0);
		Character *fred = _script->getCharacter("FRED");
		int fetch = -1, done = -1;
		Common::String at;
		for (int t = 1; t <= 400 && done < 0; t++) {
			tick1();
			if (fetch < 0 && fred->getState() == Character::kStateSpecial) {
				fetch = t;
				const int g = find("OBJ_P13_KITCHEN_HOTSPOT1");
				int slot = 0;
				for (uint i = 0; i < Inventory::kSlotCount; i++)
					slot = slot || _inventory->getSlot(i) == "OBJ_INV_BOTTLE_EMPTY";
				at = Common::String::format("%s, Get_Grease %d, the grease hidden %d, the empty bottle in a slot %d, the arrow %d",
				                            _currentRoom->getAnim(fred->getSprite()).c_str(), _sound->isClipPlaying("Kitchen.Get_Grease") ? 1 : 0,
				                            g >= 0 && _currentRoom->isHidden(g) ? 1 : 0, slot, _inventory->isDefaultCursor() ? 1 : 0);
			}
			if (fetch > 0 && !c->isGreasing())
				done = t;
		}
		out += Common::String::format("; Fred fetches at tick %d (%s); at tick %d the full bottle %d, scene '%s' held %d, busy %d", fetch, at.c_str(), done,
		                              _inventory->hasItem("OBJ_INV_BOTTLE_FULL") ? 1 : 0, _script->getScene().c_str(), _script->isBusy() ? 1 : 0, c->isBusy() ? 1 : 0);

		// Left with the bottle at the grease: the full bottle all the same.
		story();
		setGameFlag("EwanPuzzle", true);
		giveClue("EWAN");
		_inventory->addItem("OBJ_INV_SCREWDRIVER");
		_inventory->addItem("OBJ_INV_BOTTLE_EMPTY");
		showRoom(kKitchen);
		_script->sendEvent("OBJ_P13_KITCHEN_HOTSPOT1", true);
		showRoom("ROOM_P12_Dining_Room");
		out += Common::String::format("; left at once: the full bottle %d", _inventory->hasItem("OBJ_INV_BOTTLE_FULL") ? 1 : 0);
		debugC(3, kDebugScript, "kitchen check grease: %s", out.c_str());
	}

	// Hotspot 3, the way to the storage room's movie, the profile.
	{
		story();
		showRoom(kKitchen);
		_script->skip();
		_script->clickClickable(find("OBJ_P13_KITCHEN_HOTSPOT3"));
		tick1();
		Character *d = _script->getCharacter("DAPHNE");
		Common::String out = Common::String::format("hotspot 3: Daphne %d", d ? d->getState() : -1);
		KitchenRoom *c = kitchen();
		int m1 = 8, m2 = 8, m3 = 8;
		c->exitMovie("ROOM_P17_Kitchen_Storage", m1);
		c->exitMovie("ROOM_P12_Dining_Room", m3);
		_inventory->addItem("OBJ_INV_SCREWDRIVER");
		c->exitMovie("ROOM_P17_Kitchen_Storage", m2);
		out += Common::String::format("; to the storage room movie %d, with the screwdriver %d; to the dining room %d", m1, m2, m3);
		story();
		c = kitchen();
		c->reset();
		Common::INIFile ini;
		c->saveKeys(ini);
		Common::String a;
		ini.getKey("EwanIntro", kKitchen, a);
		out += Common::String::format("; a new game: EwanIntro=%s", a.c_str());
		c->setIntroDone(true);
		Common::INIFile ini2;
		c->saveKeys(ini2);
		c->reset();
		c->loadKeys(ini2);
		out += Common::String::format("; read back %d", c->isIntroDone() ? 1 : 0);
		debugC(3, kDebugScript, "kitchen check other: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Terraglyph
