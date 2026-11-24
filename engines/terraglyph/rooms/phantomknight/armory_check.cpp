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

#include "terraglyph/rooms/phantomknight/armory.h"
#include "terraglyph/formats/art_archive.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkArmory() {
	// Phantom's king's armory and its slide (WP-068).
	const char *const kArmory = "ROOM_P20_Kings_Armory";
	const char *const kSlide = "ROOM_P20_Kings_Armory_Slide";
	const char *const kDoors = "ROOM_P11_Room_Of_Doors";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kArmory;
	if (!found) {
		debugC(3, kDebugScript, "armory check: no king's armory");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *a = createRoomClass(*this, kArmory);
		RoomClass *b = createRoomClass(*this, kSlide);
		debugC(3, kDebugScript, "armory check classes: King's Armory %s; Slide %s", dynamic_cast<KingsArmoryRoom *>(a) ? "armory" : "-",
		       dynamic_cast<KingsArmoryRoom *>(b) ? "armory" : "-");
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
	auto armory = [&](const char *room) { return dynamic_cast<KingsArmoryRoom *>(getRoomClass(room)); };
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
		return Common::String::format("%s %s depth %d hit %d required '%s'", r.isRequiresObject(s) ? "requires" : "other", r.getAnim(s).c_str(), r.getDepth(s),
		                              r.getHitType(s), r.getRequired(s).c_str());
	};
	// A fresh story, Jane screaming or not.
	auto story = [&](bool screaming) {
		newGameForCheck();
		setGameFlag("Screaming", screaming);
		_sound->stop();
	};
	auto useBottle = [&]() {
		_inventory->addItem("OBJ_INV_BOTTLE_FULL");
		_inventory->removeItem("OBJ_INV_BOTTLE_FULL");
	};
	auto inSlot = [&](const char *item) {
		for (uint i = 0; i < Inventory::kSlotCount; i++) {
			if (_inventory->getSlot(i) == item)
				return 1;
		}
		return 0;
	};

	// The placements.
	{
		story(false);
		showRoom(kDoors);
		showRoom(kArmory);
		Common::String out = Common::String::format("a new game: duster %s, armour %s, scratch %s", sprite(find("OBJ_INV_DUSTER")).c_str(),
		                                            sprite(find("OBJ_P20_KINGS_ARMORY_MAN")).c_str(), sprite(find("OBJ_P20_KINGS_ARMORY_SCRATCH")).c_str());
		story(false);
		_inventory->addItem("OBJ_INV_DUSTER");
		showRoom(kDoors);
		showRoom(kArmory);
		out += Common::String::format("; the duster had: duster %s", sprite(find("OBJ_INV_DUSTER")).c_str());
		debugC(3, kDebugScript, "armory check placements: %s", out.c_str());
	}

	// Entry: the intro once; Jane screaming; the bottle used.
	{
		Common::String out;
		story(false);
		const char *const kRooms[] = { kArmory, kSlide };
		for (uint r = 0; r < 2; r++) {
			showRoom(kDoors);
			showRoom(kRooms[r]);
			const Common::String first = _script->getOpening();
			const int held = _script->isBusy() ? 1 : 0;
			showRoom(kDoors);
			showRoom(kRooms[r]);
			out += Common::String::format("%s%s: first '%s' held %d, IntroDone %d, again '%s'", r ? "; " : "", kRooms[r] + 5, first.c_str(), held,
			                              armory(kRooms[r])->isIntroDone() ? 1 : 0, _script->getOpening().c_str());
		}
		uint screams = 0, intros = 0;
		for (uint k = 0; k < 200; k++) {
			story(true);
			showRoom(kDoors);
			showRoom(kArmory);
			const Common::String s = armory(kArmory)->getScream();
			for (int n = 1; n <= 5; n++) {
				if (s == Common::String::format("Kings_Armory.Jane_Screaming%d", n))
					screams |= 1 << (n - 1);
			}
			const Common::String o = _script->getOpening();
			intros |= o == "Kings_Armory.Intro_" ? 1 : o == "Kings_Armory.Intro2_" ? 2 : 4;
		}
		out += Common::String::format("; screaming, 200 visits: screams 1-5");
		for (int n = 0; n < 5; n++)
			out += Common::String::format(" %d", (screams >> n) & 1);
		out += Common::String::format(", Intro_ %d, Intro2_ %d, other %d", intros & 1 ? 1 : 0, intros & 2 ? 1 : 0, intros & 4 ? 1 : 0);
		for (uint r = 0; r < 2; r++) {
			story(false);
			useBottle();
			showRoom(kDoors);
			showRoom(kRooms[r]);
			tick1();
			out += Common::String::format("; the bottle used, %s: to '%s' movie %d", kRooms[r] + 5, next.c_str(), movie);
		}
		debugC(3, kDebugScript, "armory check entry: %s", out.c_str());
	}

	// Scooby takes the duster.
	{
		story(false);
		showRoom(kDoors);
		showRoom(kArmory);
		_script->skip();
		KingsArmoryRoom *c = armory(kArmory);
		const int duster = find("OBJ_INV_DUSTER");
		_script->sendEvent("OBJ_INV_DUSTER", false);
		Common::String out = Common::String::format("taking %d, busy %d", c->isTakingDuster() ? 1 : 0, c->isBusy() ? 1 : 0);
		Character *scooby = _script->getCharacter("SCOOBY");
		int start = -1, done = -1;
		Common::String at;
		for (int t = 1; t <= 600 && done < 0; t++) {
			tick1();
			if (start < 0 && scooby->getState() == Character::kStateSpecial) {
				start = t;
				at = Common::String::format("%s, Scooby_Takes_Duster %d, hidden %d", _currentRoom->getAnim(scooby->getSprite()).c_str(),
				                            _sound->isClipPlaying("Kings_Armory.Scooby_Takes_Duster") ? 1 : 0, duster >= 0 && _currentRoom->isHidden(duster) ? 1 : 0);
			}
			if (start > 0 && !c->isTakingDuster())
				done = t;
		}
		out += Common::String::format("; Scooby at tick %d (%s); at tick %d the duster %d, scene '%s' held %d, busy %d", start, at.c_str(), done,
		                              _inventory->hasItem("OBJ_INV_DUSTER") ? 1 : 0, _script->getScene().c_str(), _script->isBusy() ? 1 : 0, c->isBusy() ? 1 : 0);
		story(false);
		showRoom(kDoors);
		showRoom(kArmory);
		_script->sendEvent("OBJ_INV_DUSTER", false);
		showRoom(kDoors);
		out += Common::String::format("; left at once: the duster %d", _inventory->hasItem("OBJ_INV_DUSTER") ? 1 : 0);
		debugC(3, kDebugScript, "armory check duster: %s", out.c_str());
	}

	// The scratch and the armour: the arrow, then the full bottle.
	{
		story(false);
		showRoom(kDoors);
		showRoom(kArmory);
		_script->skip();
		Common::String out = "the arrow on the scratch:";
		for (int k = 0; k < 4; k++) {
			_script->sendEvent("OBJ_P20_KINGS_ARMORY_SCRATCH", false);
			out += Common::String::format(" '%s'", _script->getScene().c_str());
			_script->skip();
			tick1();
		}
		_script->sendEvent("OBJ_P20_KINGS_ARMORY_MAN", false);
		tick1();
		out += Common::String::format("; on the armour: to %s movie %d", next.c_str(), movie);
		static const char *const kTargets[] = { "OBJ_P20_KINGS_ARMORY_SCRATCH", "OBJ_P20_KINGS_ARMORY_MAN" };
		for (uint k = 0; k < 2; k++) {
			story(false);
			_inventory->addItem("OBJ_INV_BOTTLE_FULL");
			showRoom(kDoors);
			showRoom(kArmory);
			_script->skip();
			_inventory->setCursor("ANIM_CURSORSPORTS_BOTTLE_FILLED");
			_script->sendEvent(kTargets[k], true);
			const int slot = inSlot("OBJ_INV_BOTTLE_FULL");
			const int arrow = _inventory->isDefaultCursor() ? 1 : 0;
			tick1();
			out += Common::String::format("; the bottle on the %s: to %s movie %d, in a slot %d, the arrow %d", k ? "armour" : "scratch", next.c_str(), movie, slot, arrow);
		}
		debugC(3, kDebugScript, "armory check use: %s", out.c_str());
	}

	// The profile: each room's IntroDone in its own section.
	{
		story(false);
		Common::INIFile ini;
		armory(kArmory)->saveKeys(ini);
		armory(kSlide)->saveKeys(ini);
		Common::String a, b;
		ini.getKey("IntroDone", kArmory, a);
		ini.getKey("IntroDone", kSlide, b);
		Common::String out = Common::String::format("a new game: armory IntroDone=%s, slide IntroDone=%s", a.c_str(), b.c_str());
		showRoom(kDoors);
		showRoom(kSlide);
		Common::INIFile ini2;
		armory(kArmory)->saveKeys(ini2);
		armory(kSlide)->saveKeys(ini2);
		ini2.getKey("IntroDone", kArmory, a);
		ini2.getKey("IntroDone", kSlide, b);
		out += Common::String::format("; after the slide: armory %s, slide %s", a.c_str(), b.c_str());
		armory(kSlide)->reset();
		armory(kSlide)->loadKeys(ini2);
		out += Common::String::format("; read back %d", armory(kSlide)->isIntroDone() ? 1 : 0);
		debugC(3, kDebugScript, "armory check profile: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Terraglyph
