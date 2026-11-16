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
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"
#include "scooby/souvenir_rooms.h"

namespace Scooby {

void ScoobyEngine::checkSouvenirShop() {
	// Phantom's souvenir shop (WP-058): the class, the placements, the
	// openings by Screaming and IntroDone, the bottle with the arrow and with
	// the tokens, Fred fetching it, leaving while bought, the hotspot's
	// lines, and the profile.
	const char *const kShop = "ROOM_P03_Souvenir_Shop";
	const char *const kTown = "ROOM_P04_Town";
	const char *const kBottle = "OBJ_INV_BOTTLE_EMPTY";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kShop;
	if (!found) {
		debugC(3, kDebugScript, "souvenir check: no souvenir shop");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kShop);
		debugC(3, kDebugScript, "souvenir check classes: Souvenir Shop %s", dynamic_cast<SouvenirShopRoom *>(c) ? "souvenir shop" : "-");
		delete c;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto shop = [&]() { return dynamic_cast<SouvenirShopRoom *>(getRoomClass(kShop)); };
	auto sprite = [&](int s) {
		if (s < 0)
			return Common::String("none");
		Room &r = *_currentRoom;
		const int p = r.getParent(s);
		return Common::String::format("%s %s mode %d step %u depth %d hit %d under %s%s%s", r.getObjectName(s).c_str(), r.getAnim(s).c_str(), r.getMode(s),
		                              r.getStep(s), r.getDepth(s), r.getHitType(s), p == 0 ? "root" : r.getLabel(p).c_str(),
		                              r.isRequiresObject(s) ? " requires" : "", r.isClickable(s) ? " clickable" : "");
	};
	auto find = [&](const char *object) {
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == object)
				return (int)s;
		}
		return -1;
	};
	// A visit: a new game, IntroDone and Screaming as given, from @p from.
	auto visit = [&](bool introDone, bool screaming, const char *from) {
		newGameForCheck();
		setGameFlag("Screaming", screaming);
		shop()->setIntroDone(introDone);
		if (from)
			showRoom(from);
		showRoom(kShop);
	};
	auto entry = [&]() {
		SouvenirShopRoom *c = shop();
		Character *fred = _script->getCharacter("FRED");
		return Common::String::format("opening '%s' played %d, held %d, IntroDone %d, scream %s; FRED state %d depth %d", _script->getOpening().c_str(),
		                              _script->isOpeningDone() ? 1 : 0, _script->isBusy() ? 1 : 0, c->isIntroDone() ? 1 : 0,
		                              c->getScream().empty() ? "none" : "one", fred ? fred->getState() : -1,
		                              fred && fred->getSprite() >= 0 ? _currentRoom->getDepth(fred->getSprite()) : 0);
	};

	// The placements and the openings.
	{
		visit(false, false, kTown);
		Common::String out = Common::String::format("bottle %s; hotspot %s; first visit from the town: %s", sprite(find(kBottle)).c_str(),
		                                            sprite(find("OBJ_P03_SOUVENIR_STORE_HOTSPOT1")).c_str(), entry().c_str());
		_script->skip();
		showRoom(kTown);
		showRoom(kShop);
		out += Common::String::format("; again: %s", entry().c_str());
		visit(false, true, nullptr);
		out += Common::String::format("; screaming: %s", entry().c_str());
		uint seen = 0;
		for (uint k = 0; k < 100; k++) {
			visit(true, true, nullptr);
			const Common::String &s = shop()->getScream();
			if (s.hasPrefix("Souvenir_Shop.Jane_Screaming") && s.size() == 29)
				seen |= 1 << (s[28] - '0');
		}
		Common::String sc;
		for (int v = 0; v < 10; v++) {
			if (seen & (1 << v))
				sc += Common::String::format(" %d", v);
		}
		out += Common::String::format("; screams seen over 100 visits%s", sc.c_str());
		debugC(3, kDebugScript, "souvenir check entry: %s", out.c_str());
	}

	// The bottle with the arrow, then with the tokens; Fred fetches it.
	{
		visit(true, false, nullptr);
		SouvenirShopRoom *c = shop();
		_script->sendEvent(kBottle, false);
		Common::String out = Common::String::format("the arrow: scene '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		_script->skip();
		_inventory->addItem("OBJ_INV_TOKENS");
		_inventory->setCursor("ANIM_CURSORTOKENS");
		_script->sendEvent(kBottle, true);
		bool tokens = false;
		for (uint i = 0; i < Inventory::kSlotCount; i++)
			tokens = tokens || _inventory->getSlot(i) == "OBJ_INV_TOKENS";
		out += Common::String::format("; the tokens: bought %d, busy %d, tokens in the toolbar %d, cursor %s", c->isBought() ? 1 : 0, c->isBusy() ? 1 : 0,
		                              tokens ? 1 : 0, _inventory->getCursor().c_str());
		Character *fred = _script->getCharacter("FRED");
		const int bottle = c->getBottle();
		int fetchAt = -1, toolbarAt = -1;
		Common::String atFetch;
		for (uint t = 1; t <= 400 && toolbarAt < 0; t++) {
			tick1();
			if (fetchAt < 0 && c->isFetching()) {
				fetchAt = t;
				atFetch = Common::String::format("FRED state %d %s, %s playing %d; bottle %s", fred->getState(), _currentRoom->getAnim(fred->getSprite()).c_str(),
				                                 "Souvenir_Shop.Fred_Takes_Bottle", _sound->isClipPlaying("Souvenir_Shop.Fred_Takes_Bottle") ? 1 : 0,
				                                 sprite(bottle).c_str());
			}
			if (_inventory->hasItem(kBottle))
				toolbarAt = t;
		}
		out += Common::String::format("; fetched at tick %d: %s; in the toolbar at tick %d, bought %d, scene '%s' held %d", fetchAt, atFetch.c_str(), toolbarAt,
		                              c->isBought() ? 1 : 0, _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		debugC(3, kDebugScript, "souvenir check bottle: %s", out.c_str());

		// Taken: the next visit's bottle.
		_script->skip();
		showRoom(kTown);
		showRoom(kShop);
		debugC(3, kDebugScript, "souvenir check taken: bottle %s", sprite(find(kBottle)).c_str());
	}

	// Left while bought: the bottle still goes in the toolbar.
	{
		visit(true, false, nullptr);
		_inventory->addItem("OBJ_INV_TOKENS");
		_script->sendEvent(kBottle, true);
		const bool before = _inventory->hasItem(kBottle);
		showRoom(kTown);
		debugC(3, kDebugScript, "souvenir check left: before %d, after %d", before ? 1 : 0, _inventory->hasItem(kBottle) ? 1 : 0);
	}

	// The hotspot, clicked three times, each played out.
	{
		visit(true, false, nullptr);
		const int h = shop()->getHotspot();
		Common::String out;
		for (uint k = 0; k < 3; k++) {
			_script->clickClickable(h);
			tick1();
			out += Common::String::format("%splays %s, next %s: scene '%s'", k ? "; " : "", _currentRoom->getAnim(h).c_str(), _script->getClickableState(h).c_str(),
			                              _script->getScene().c_str());
			_script->skip();
			for (uint t = 0; t < 400 && _script->isClickablePlaying(h); t++)
				tick1();
		}
		debugC(3, kDebugScript, "souvenir check hotspot: %s", out.c_str());
	}

	// The profile.
	{
		Common::INIFile ini;
		SouvenirShopRoom *c = shop();
		c->setIntroDone(true);
		c->saveKeys(ini);
		Common::String v;
		ini.getKey("IntroDone", kShop, v);
		c->reset();
		const bool reset = c->isIntroDone();
		c->loadKeys(ini);
		debugC(3, kDebugScript, "souvenir check profile: IntroDone=%s; reset %d; loaded %d", v.c_str(), reset ? 1 : 0, c->isIntroDone() ? 1 : 0);
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Scooby
