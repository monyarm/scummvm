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

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/rooms/showdown/hotel.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/common/setup/setup.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kHall = "ROOM_P17_Hotel_Hall";
static const char *const kHotelRoom = "ROOM_P20_Hotel_Room";
static const char *const kNickel = "OBJ_INV_NICKEL";

static int findSprite(const Room &room, const char *object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

static bool used(const Inventory &inventory, const char *item) {
	if (!inventory.hasItem(item))
		return false;
	for (uint i = 0; i < 4; i++) {
		if (inventory.getSlot(i) == item)
			return false;
	}
	return true;
}

// 0x40C6B0: an opening's name alone; whether it has played stays as it was.
static void nameOpening(TerraglyphEngine &engine, const char *room, const char *scene) {
	RoomScript &script = *engine.getScript();
	RoomScript::Memory &memory = script.getMemory(room);
	const bool played = memory.openingPlayed;
	script.setOpening(scene);
	memory.openingPlayed = played;
}

// ---------------------------------------------------------------------------
// The hall

void HotelHallRoom::reset() {
	// 0x410C10, after the base reset.
	_secondEntrance = false;
}

void HotelHallRoom::prepare() {
	// 0x410CC0 (+0x30): 1100BE3_, played or not as before; once played, the
	// second visit has 1100B2_ instead, once.
	nameOpening(_engine, kHall, "Hotel_Hall.1100BE3_");
	if (_engine.getScript()->getMemory(kHall).openingPlayed && !_secondEntrance) {
		_engine.getScript()->setOpening("Hotel_Hall.1100B2_");
		_secondEntrance = true;
	}
}

void HotelHallRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x410DC0: the door a requires object (0x408C80 with -1: layer 0) at
	// depth 1; hotspot 2 a clickable at depth -100; the chandelier a
	// clickable at depth -1, until the nickel is used; hotspot 1 the default
	// clickable.
	if (object == "OBJ_P17_HOTEL_HALL_DOOR") {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.depth = 1;
	} else if (object == "OBJ_P17_HOTEL_HALL_HOTSPOT2") {
		claim.build = Room::Claim::kClickable;
		claim.depth = -100;
	} else if (object == "OBJ_P17_HOTEL_HALL_CHAND") {
		if (used(*_engine.getInventory(), kNickel)) {
			claim.build = Room::Claim::kSkip;
		} else {
			claim.build = Room::Claim::kClickable;
			claim.depth = -1;
		}
	}
}

void HotelHallRoom::enter(const Common::String &from) {
	// 0x410C40, after the base entry: Daphne walks in from the left from the
	// saloon, Fred from the right otherwise; the music at 95%.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	_lastScene.clear();
	_door = findSprite(room, "OBJ_P17_HOTEL_HALL_DOOR");
	_hotspot1 = findSprite(room, "OBJ_P17_HOTEL_HALL_HOTSPOT1");
	if (_door >= 0 && _engine.getGameFlag("HotelHallDoor"))
		room.startAnim(_door, "ANIM_P17_HOTEL_HALL_DOORANIM", 5, -1);
	Character *c = from == "ROOM_P12_Saloon" ? script.getCharacter("DAPHNE") : script.getCharacter("FRED");
	if (c)
		c->walkIn(room, *_engine.getSound(), from == "ROOM_P12_Saloon" ? "L" : "R");
	_engine.getMusic()->setGroup(3, 95);
	_chandLine = 0;
}

void HotelHallRoom::event(const Common::String &object, bool success) {
	// 0x410F80
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	if (object == "OBJ_P17_HOTEL_HALL_DOOR") {
		// Fred, idle, opens it, whatever the cursor.
		Character *fred = script.getCharacter("FRED");
		if (fred && fred->getState() == Character::kStateIdle) {
			if (_door >= 0)
				room.startAnim(_door, "ANIM_P17_HOTEL_HALL_DOORANIM", 5, -1);
			_engine.setGameFlag("HotelHallDoor", true);
			fred->special(room, *_engine.getSound(), "OBJ_FRED_P17_OPEN_DOOR");
			_engine.getSound()->play("Hotel_Hall.Unlocked_Door");
			debugC(1, kDebugScript, "hotel hall: Fred opens the door at tick %u", script.getTick());
			return;
		}
	}
	if (object == "OBJ_P17_HOTEL_HALL_CHAND") {
		// Three lines in turn, not held.
		static const char *const kLines[] = { "Hotel_Hall.400A_", "Hotel_Hall.400A2_", "Hotel_Hall.400A3_" };
		const char *scene = kLines[_chandLine];
		_chandLine = (_chandLine + 1) % 3;
		if (script.startScene(scene, false))
			_lastScene = scene;
	}
}

void HotelHallRoom::tick() {
	// 0x410D00, after the base tick: hotspot 1 on step 6 of its A, or step 4
	// of its B: Daphne scared, and her scream.
	if (_hotspot1 < 0)
		return;
	Room &room = *_engine.getRoom();
	const Common::String &anim = room.getAnim(_hotspot1);
	const char last = anim.empty() ? 0 : anim.lastChar();
	const uint32 step = room.getStep(_hotspot1);
	if ((last == 'A' && step == 6) || (last == 'B' && step == 4)) {
		RoomScript &script = *_engine.getScript();
		if (Character *daphne = script.getCharacter("DAPHNE"))
			daphne->setState(room, *_engine.getSound(), Character::kStateScared);
		if (script.startScene("Hotel_Hall.Scream_", false))
			_lastScene = "Hotel_Hall.Scream_";
	}
}

void HotelHallRoom::saveKeys(Common::INIFile &profile) {
	// 0x411090: the base keys, then SecondEntrance, in the room's section.
	profile.setKey("SecondEntrance", kHall, _secondEntrance ? "Yes" : "No");
}

void HotelHallRoom::loadKeys(const Common::INIFile &profile) {
	// 0x4110D0 -> 0x40C560: a missing key reads as false.
	Common::String v;
	_secondEntrance = profile.getKey("SecondEntrance", kHall, v) && v == "Yes";
}

// ---------------------------------------------------------------------------
// The room

void HotelRoomRoom::reset() {
	// 0x411170, after the base reset.
	_windowCount = 0;
	_lookedOutAuto = false;
	_autoLook = false;
}

void HotelRoomRoom::arrive(const Common::String &from) {
	// 0x411190 (+0x2C). Back from the window, the brick not found yet: a
	// comment, 410C_ and 410C2_ in turn. Else, on the first visit, 280C_
	// and then the window by itself; or no opening.
	RoomScript &script = *_engine.getScript();
	if (from == kHotelRoom && !_engine.getInventory()->hasItem("OBJ_INV_BRICK")) {
		const int old = _windowCount++;
		if (old == 1) {
			script.setOpening("Hotel_Room.410C_");
		} else if (old == 2) {
			script.setOpening("Hotel_Room.410C2_");
			_windowCount = 1;
		} else {
			script.getMemory(kHotelRoom).openingPlayed = false;
		}
		return;
	}
	if (!_lookedOutAuto) {
		script.setOpening("Hotel_Room.280C_");
		_lookedOutAuto = true;
		_autoLook = true;
		return;
	}
	nameOpening(_engine, kHotelRoom, "");
}

void HotelRoomRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x411280: the nickel a requires object (0x408C80 with -1: layer 0),
	// hidden once taken. Hotspots 2 and 4 are built as the default path
	// builds a clickable (0x408DC0 with 1, -1: depth priority=, -1 without
	// one), so they are left to it and found by name on entry.
	if (object == kNickel) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.hidden = _engine.getInventory()->hasItem(kNickel);
	}
}

void HotelRoomRoom::enter(const Common::String &from) {
	// 0x411250, after the base entry: the music at 90%.
	Room &room = *_engine.getRoom();
	_hotspot2 = findSprite(room, "OBJ_P20_HOTEL_HOTSPOT2");
	_hotspot4 = findSprite(room, "OBJ_P20_HOTEL_HOTSPOT4");
	_nickel = findSprite(room, kNickel);
	_engine.getMusic()->setGroup(3, 90);
	_grabbing = false;
	_nickelClicked = false;
	_lastScene.clear();
}

void HotelRoomRoom::roomLeft() {
	// 0x411910, after the base leave: the nickel clicked and not yet taken.
	Inventory &inventory = *_engine.getInventory();
	if (_nickelClicked && !inventory.hasItem(kNickel))
		inventory.addItem(kNickel, "ANIM_TOOLBARBuffalo_Nickel", "ANIM_CURSORBuffalo_Nickel");
}

void HotelRoomRoom::event(const Common::String &object, bool success) {
	// 0x4113B0: hotspot 2 on its C: 410X_, not held. The nickel with the
	// arrow: Daphne picks it up.
	RoomScript &script = *_engine.getScript();
	if (object == "OBJ_P20_HOTEL_HOTSPOT2") {
		if (_hotspot2 >= 0 && script.getClickableState(_hotspot2) == "ANIM_P20_HOTEL_HOTSPOT2C" && script.startScene("Hotel_Room.410X_", false))
			_lastScene = "Hotel_Room.410X_";
	} else if (object == kNickel && _engine.getInventory()->isDefaultCursor()) {
		_nickelClicked = true;
	}
}

bool HotelRoomRoom::leaveBy(const Common::String &exitObject) {
	leave(_engine.getScript()->readExit(exitObject));
	return true;
}

void HotelRoomRoom::leave(RoomScript::Exit exit) {
	// 0x411440 (+0x24): the window (an exit to this room) plays the
	// graveyard from above, movie 15 + GraveLocation. Only the first way out
	// keeps its scene. Then the default handler.
	if (exit.destination == kHotelRoom)
		exit.movie = _engine.getGameInt("GraveLocation") + 15;
	if (_windowCount == 0)
		_windowCount = 1;
	else
		exit.scene.clear();
	_lastExit = exit;
	debugC(1, kDebugScript, "hotel room: out to %s, movie %d, scene '%s'", exit.destination.c_str(), exit.movie, exit.scene.c_str());
	_engine.getScript()->startExit(exit);
}

void HotelRoomRoom::tick() {
	// 0x4115D0, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	const bool free = !script.isScenePlaying() && script.isOpeningDone();
	// The first visit's opening over: the window, by itself.
	if (_autoLook && free) {
		_autoLook = false;
		RoomScript::Exit exit;
		exit.destination = kHotelRoom;
		exit.movie = -1;
		leave(exit);
	}
	// The nickel: Daphne picks it up, input held; on her step 14 it goes
	// from the floor; idle again, it is in the toolbar, with 410A_.
	Character *daphne = script.getCharacter("DAPHNE");
	if (_nickelClicked) {
		if (!_grabbing && daphne && daphne->getState() == Character::kStateIdle && free) {
			_grabbing = true;
			daphne->special(room, sound, "OBJ_DAPHNE_P20_GRAB_COIN");
		}
		if (_grabbing && daphne) {
			if (daphne->getSprite() >= 0 && room.getStep(daphne->getSprite()) == 14) {
				if (_nickel >= 0)
					room.setHidden(_nickel, true);
			} else if (daphne->getState() == Character::kStateIdle) {
				_grabbing = false;
				_nickelClicked = false;
				Inventory &inventory = *_engine.getInventory();
				if (!inventory.hasItem(kNickel))
					inventory.addItem(kNickel, "ANIM_TOOLBARBuffalo_Nickel", "ANIM_CURSORBuffalo_Nickel");
				if (script.startScene("Hotel_Room.410A_", true))
					_lastScene = "Hotel_Room.410A_";
			}
		}
	}
	// Hotspot 4 on step 11: its scene.
	if (_hotspot4 >= 0 && room.getStep(_hotspot4) == 11 && script.startScene("Hotel_Room.HOTSPOT4_", false))
		_lastScene = "Hotel_Room.HOTSPOT4_";
	// Hotspot 2 on step 18 of its C or D: Daphne scared.
	if (_hotspot2 >= 0 && room.getStep(_hotspot2) == 18 &&
	    (room.getAnim(_hotspot2) == "ANIM_P20_HOTEL_HOTSPOT2C" || room.getAnim(_hotspot2) == "ANIM_P20_HOTEL_HOTSPOT2D") && daphne)
		daphne->setState(room, sound, Character::kStateScared);
}

void HotelRoomRoom::saveKeys(Common::INIFile &profile) {
	// 0x411960: the base keys, then LookedOutAuto, in the room's section.
	profile.setKey("LookedOutAuto", kHotelRoom, _lookedOutAuto ? "Yes" : "No");
}

void HotelRoomRoom::loadKeys(const Common::INIFile &profile) {
	// 0x4119A0 -> 0x40C560: a missing key reads as false.
	Common::String v;
	_lookedOutAuto = profile.getKey("LookedOutAuto", kHotelRoom, v) && v == "Yes";
}

RoomClass *createHotelRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kHall)
		return new HotelHallRoom(engine);
	if (room == kHotelRoom)
		return new HotelRoomRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
