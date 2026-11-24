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
#include "terraglyph/rooms/phantomknight/storage.h"

namespace Terraglyph {

void TerraglyphEngine::checkStorage() {
	// Phantom's kitchen storage room (WP-067).
	const char *const kStorage = "ROOM_P17_Kitchen_Storage";
	const char *const kKitchen = "ROOM_P13_Kitchen";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kStorage;
	if (!found) {
		debugC(3, kDebugScript, "storage check: no kitchen storage room");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kStorage);
		debugC(3, kDebugScript, "storage check classes: Kitchen Storage %s", dynamic_cast<KitchenStorageRoom *>(c) ? "storage room" : "-");
		delete c;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto storage = [&]() { return dynamic_cast<KitchenStorageRoom *>(getRoomClass(kStorage)); };
	auto find = [&](const char *name) {
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == name)
				return (int)s;
		}
		return -1;
	};
	auto sprite = [&](int s) {
		if (s < 0)
			return Common::String("none");
		Room &r = *_currentRoom;
		const char *kind = r.isRequiresObject(s) ? "requires" : r.isClickable(s) ? "clickable" : "other";
		return Common::String::format("%s %s depth %d hit %d rollover '%s'%s", kind, r.getAnim(s).c_str(), r.getDepth(s), r.getHitType(s), r.getRollover(s).c_str(),
		                              r.isHidden(s) ? " hidden" : "");
	};
	auto fredAt = [&]() {
		Character *f = _script->getCharacter("FRED");
		if (!f || f->getSprite() < 0)
			return Common::String("none");
		return Common::String::format("(%d, %d)", _currentRoom->getOffsetX(f->getSprite()), _currentRoom->getOffsetY(f->getSprite()));
	};
	auto story = [&](int doorClicked) {
		newGameForCheck();
		storage()->setDoorClicked(doorClicked);
		_sound->stop();
	};
	auto inSlot = [&](const char *item) {
		for (uint i = 0; i < Inventory::kSlotCount; i++) {
			if (_inventory->getSlot(i) == item)
				return 1;
		}
		return 0;
	};
	// Ticks until the room changes, each scene skipped as it starts.
	auto untilNext = [&]() {
		next.clear();
		movie = -1;
		for (uint t = 0; t < 3000 && next.empty(); t++) {
			tick1();
			if (_script->isScenePlaying())
				_script->skip();
		}
		return Common::String::format("to %s movie %d", next.empty() ? "-" : next.c_str(), movie);
	};

	// The placements: a new game; the door begun with both had; the door
	// open.
	{
		Common::String out;
		for (int k = 0; k < 3; k++) {
			story(k == 0 ? 0 : k == 1 ? 1 : 3);
			if (k >= 1) {
				_inventory->addItem("OBJ_INV_SCREWDRIVER");
				_inventory->removeItem("OBJ_INV_SCREWDRIVER");
				_inventory->addItem("OBJ_INV_SHIELD");
			}
			showRoom(kKitchen);
			showRoom(kStorage);
			static const char *const kCase[] = { "a new game", "begun, both had", "open" };
			const Common::String door = sprite(find("OBJ_P17_KITCHEN_STORAGE_DOOR"));
			const Common::String screw = sprite(find("OBJ_INV_SCREWDRIVER"));
			const Common::String shield = sprite(find("OBJ_INV_SHIELD"));
			out += Common::String::format("%s%s: door %s, screwdriver %s, shield %s, Fred at %s", k ? "; " : "", kCase[k], door.c_str(), screw.c_str(), shield.c_str(),
			                              fredAt().c_str());
		}
		out += Common::String::format("; hotspot 3 %s", sprite(find("OBJ_P17_KITCHEN_STORAGE_HOTSPOT3")).c_str());
		debugC(3, kDebugScript, "storage check placements: %s", out.c_str());
	}

	// Entry's scenes.
	{
		Common::String out;
		static const struct {
			int door;
			bool fromHere;
		} kCases[] = { { 3, false }, { 3, true }, { 1, true }, { 1, false }, { 0, true } };
		for (uint k = 0; k < ARRAYSIZE(kCases); k++) {
			story(kCases[k].door);
			showRoom(kCases[k].fromHere ? kStorage : kKitchen);
			showRoom(kStorage);
			out += Common::String::format("%sDoorClicked %d from %s: '%s' held %d", k ? "; " : "", kCases[k].door, kCases[k].fromHere ? "here" : "the kitchen",
			                              _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		}
		debugC(3, kDebugScript, "storage check entry: %s", out.c_str());
	}

	// The door: the arrow; the screwdriver; then twice more.
	{
		story(0);
		showRoom(kKitchen);
		showRoom(kStorage);
		_script->sendEvent("OBJ_P17_KITCHEN_STORAGE_DOOR", false);
		Common::String out = Common::String::format("the arrow: %s", untilNext().c_str());
		for (int k = 0; k < 3; k++) {
			showRoom(kStorage);
			_script->sendEvent("OBJ_P17_KITCHEN_STORAGE_DOOR", false);
			const Common::String scene = _script->getScene();
			out += Common::String::format(", '%s'", scene.c_str());
			if (k == 2) {
				out += Common::String::format(" (%s)", untilNext().c_str());
			} else {
				_script->skip();
				tick1();
			}
		}
		story(0);
		_inventory->addItem("OBJ_INV_SCREWDRIVER");
		showRoom(kKitchen);
		showRoom(kStorage);
		_inventory->setCursor("ANIM_CURSORSCREWDRIVER");
		_script->sendEvent("OBJ_P17_KITCHEN_STORAGE_DOOR", true);
		const int door = find("OBJ_P17_KITCHEN_STORAGE_DOOR");
		out += Common::String::format("; the screwdriver: DoorClicked %d, in a slot %d, the arrow %d, door rollover '%s', %s", storage()->getDoorClicked(),
		                              inSlot("OBJ_INV_SCREWDRIVER"), _inventory->isDefaultCursor() ? 1 : 0, door >= 0 ? _currentRoom->getRollover(door).c_str() : "-",
		                              untilNext().c_str());
		for (int k = 0; k < 2; k++) {
			showRoom(kStorage);
			_script->skip();
			_script->sendEvent("OBJ_P17_KITCHEN_STORAGE_DOOR", false);
			out += Common::String::format("; again: DoorClicked %d, %s", storage()->getDoorClicked(), untilNext().c_str());
		}
		debugC(3, kDebugScript, "storage check door: %s", out.c_str());
	}

	// Fred takes the screwdriver; Daphne the shield.
	{
		story(0);
		showRoom(kKitchen);
		showRoom(kStorage);
		KitchenStorageRoom *c = storage();
		const int screw = find("OBJ_INV_SCREWDRIVER");
		_script->sendEvent("OBJ_INV_SCREWDRIVER", false);
		Common::String out = Common::String::format("screwdriver: taking %d, busy %d", c->isTakingScrewdriver() ? 1 : 0, c->isBusy() ? 1 : 0);
		Character *fred = _script->getCharacter("FRED");
		int start = -1, had = -1, done = -1;
		Common::String at;
		for (int t = 1; t <= 600 && done < 0; t++) {
			tick1();
			if (start < 0 && fred->getState() == Character::kStateSpecial) {
				start = t;
				at = Common::String::format("%s, Fred_Takes_Screwdriver %d, hidden %d", _currentRoom->getAnim(fred->getSprite()).c_str(),
				                            _sound->isClipPlaying("Kitchen_Storage.Fred_Takes_Screwdriver") ? 1 : 0, screw >= 0 && _currentRoom->isHidden(screw) ? 1 : 0);
			}
			if (had < 0 && _inventory->hasItem("OBJ_INV_SCREWDRIVER"))
				had = t;
			if (start > 0 && !c->isTakingScrewdriver())
				done = t;
		}
		out += Common::String::format("; Fred at tick %d (%s); in the toolbar at tick %d; at tick %d scene '%s' held %d, busy %d", start, at.c_str(), had, done,
		                              _script->getScene().c_str(), _script->isBusy() ? 1 : 0, c->isBusy() ? 1 : 0);
		for (int met = 0; met < 2; met++) {
			story(0);
			if (met)
				_inventory->meetSuspect("BRAD");
			showRoom(kKitchen);
			showRoom(kStorage);
			c = storage();
			const int shield = find("OBJ_INV_SHIELD");
			_script->sendEvent("OBJ_INV_SHIELD", false);
			Character *daphne = _script->getCharacter("DAPHNE");
			start = done = -1;
			for (int t = 1; t <= 600 && done < 0; t++) {
				tick1();
				if (start < 0 && daphne->getState() == Character::kStateSpecial) {
					start = t;
					at = Common::String::format("%s, Daphne_Takes_Shield %d, hidden %d", _currentRoom->getAnim(daphne->getSprite()).c_str(),
					                            _sound->isClipPlaying("Kitchen_Storage.Daphne_Takes_Shield") ? 1 : 0, shield >= 0 && _currentRoom->isHidden(shield) ? 1 : 0);
				}
				if (start > 0 && !c->isTakingShield())
					done = t;
			}
			out += Common::String::format("; shield%s: Daphne at tick %d (%s); at tick %d the shield %d, scene '%s' held %d", met ? ", Brad met" : "", start, at.c_str(), done,
			                              _inventory->hasItem("OBJ_INV_SHIELD") ? 1 : 0, _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		}
		// Left while taking: each in the toolbar all the same.
		story(0);
		showRoom(kKitchen);
		showRoom(kStorage);
		_script->sendEvent("OBJ_INV_SCREWDRIVER", false);
		_script->sendEvent("OBJ_INV_SHIELD", false);
		showRoom(kKitchen);
		out += Common::String::format("; left at once: screwdriver %d, shield %d", _inventory->hasItem("OBJ_INV_SCREWDRIVER") ? 1 : 0,
		                              _inventory->hasItem("OBJ_INV_SHIELD") ? 1 : 0);
		debugC(3, kDebugScript, "storage check take: %s", out.c_str());
	}

	// The hotspots, and the key.
	{
		story(0);
		showRoom(kKitchen);
		showRoom(kStorage);
		Common::String out;
		static const char *const kSpots[] = { "OBJ_P17_KITCHEN_STORAGE_HOTSPOT1", "OBJ_P17_KITCHEN_STORAGE_HOTSPOT2", "OBJ_P17_KITCHEN_STORAGE_HOTSPOT3" };
		for (uint h = 0; h < ARRAYSIZE(kSpots); h++) {
			out += Common::String::format("%shotspot %u:", h ? "; " : "", h + 1);
			for (int k = 0; k < 4; k++) {
				_script->sendEvent(kSpots[h], true);
				out += Common::String::format(" '%s'", _script->getScene().c_str());
				_script->skip();
				tick1();
			}
		}
		story(0);
		KitchenStorageRoom *c = storage();
		c->reset();
		Common::INIFile ini;
		c->saveKeys(ini);
		Common::String a;
		ini.getKey("DoorClicked", kStorage, a);
		out += Common::String::format("; a new game: DoorClicked=%s", a.c_str());
		c->setDoorClicked(2);
		Common::INIFile ini2;
		c->saveKeys(ini2);
		c->reset();
		c->loadKeys(ini2);
		out += Common::String::format("; read back %d", c->getDoorClicked());
		debugC(3, kDebugScript, "storage check other: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Terraglyph
