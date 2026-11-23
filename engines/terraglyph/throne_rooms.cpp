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

// Phantom of the Knight's throne room. The addresses are Phantom's.

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"
#include "terraglyph/throne_rooms.h"

namespace Terraglyph {

static const char *const kThroneRoom = "ROOM_P10_Throne_Room";
static const char *const kThrone = "OBJ_P10_THRONE_ROOM_THRONE";
static const char *const kHotspot3 = "OBJ_P10_THRONE_ROOM_HOTSPOT3";
static const char *const kRuby = "OBJ_INV_RUBY";

static bool inSlot(TerraglyphEngine &engine, const char *item) {
	// 0x4070C0: the item is in one of the toolbar's slots.
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (engine.getInventory()->getSlot(i) == item)
			return true;
	}
	return false;
}

bool ThroneRoomRoom::allBallsFound(TerraglyphEngine &engine) {
	return engine.getGameFlag("RedBallFound") && engine.getGameFlag("BlueBallFound") && engine.getGameFlag("YellowBallFound");
}

bool ThroneRoomRoom::isRubyUsed(TerraglyphEngine &engine) {
	return engine.getInventory()->hasItem(kRuby) && !inSlot(engine, kRuby);
}

int ThroneRoomRoom::findSprite(const Common::String &object) const {
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void ThroneRoomRoom::reset() {
	// 0x417AF0: Jane's movie not played, no entrances.
	_janeMoviePlayed = false;
	_entrances = 0;
}

void ThroneRoomRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x417C10. The way to the secret passage (OBJ_P10_TO_P19) is an exit
	// as the default path builds it, then at depth -1 (enter()). The
	// throne, until the three balls are found, is a clickable without the
	// ghost (0x408DC0 with 0, -1: layer= and priority=, neither set, so the
	// first layer and -1); after, the requires object it is placed as. Once
	// the ruby is used it is hidden. The Jester (OBJ_JESTER*) isn't built
	// if the ruby was never taken, or once the three balls are found. The
	// third hotspot is a clickable as the default path builds it.
	if (object == kThrone) {
		if (!allBallsFound(_engine)) {
			claim.build = Room::Claim::kClickable;
			claim.layer = -1;
			claim.depth = -1;
		} else {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
			claim.depth = 0;
		}
		claim.hidden = isRubyUsed(_engine);
	} else if (object.hasPrefix("OBJ_JESTER") && (!_engine.getInventory()->hasItem(kRuby) || allBallsFound(_engine))) {
		claim.build = Room::Claim::kSkip;
	}
}

void ThroneRoomRoom::enter(const Common::String &from) {
	// 0x417B20. The first time, Jane's movie: movie 18, back to this room
	// (game vtable +0x08), in place of the base entry; the fork's base entry
	// has run already. From the room of doors Velma walks in (D, 0x406020),
	// from the first arena Daphne (UL, 0x405EE0), from the second Shaggy
	// (UR, 0x405E60). Jane's screaming stops (game +0x288, Screaming).
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	_throne = findSprite(kThrone);
	_hotspot3 = findSprite(kHotspot3);
	_clicked = -1;
	const int passage = findSprite("OBJ_P10_TO_P19");
	if (passage >= 0)
		room.setDepth(passage, -1);
	bool movie = false;
	if (!_janeMoviePlayed) {
		_janeMoviePlayed = true;
		_engine.requestRoom(kThroneRoom, 18);
		movie = true;
	}
	const char *who = nullptr;
	const char *direction = nullptr;
	if (from == "ROOM_P11_Room_Of_Doors") {
		who = "VELMA";
		direction = "D";
	} else if (from == "ROOM_P09_Arena1") {
		who = "DAPHNE";
		direction = "UL";
	} else if (from == "ROOM_P14_Arena2") {
		who = "SHAGGY";
		direction = "UR";
	}
	if (who) {
		Character *c = script.getCharacter(who);
		if (c)
			c->walkIn(room, *_engine.getSound(), direction);
	}
	_engine.setGameFlag("Screaming", false);
	debugC(1, kDebugScript, "throne room: from %s%s", from.c_str(), movie ? "; Jane's movie" : "");
}

bool ThroneRoomRoom::click(int sprite) {
	// 0x417E80 reads Jane's clicked flag (+8) with the duster as the cursor,
	// in the room's tick, before the game's update (0x4098B0) could call it
	// a wrong item; the class takes that click, so nothing else does.
	Character *jane = _engine.getScript()->getCharacter("JANE");
	if (sprite >= 0 && jane && sprite == jane->getSprite() && _engine.getInventory()->getCursor() == "ANIM_CURSORDUSTER") {
		_clicked = sprite;
		return true;
	}
	return false;
}

void ThroneRoomRoom::tick() {
	// 0x417E80, after the base tick. Jane clicked with the duster: it
	// leaves the toolbar, the cursor is the arrow again, and
	// Throne_Room.160E_, blocking. While the third hotspot is on step 1 of
	// its A, the gang is scared (state 0x11), each tick.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	const int clicked = _clicked;
	_clicked = -1;
	Character *jane = script.getCharacter("JANE");
	if (clicked >= 0 && jane && clicked == jane->getSprite() && inventory.getCursor() == "ANIM_CURSORDUSTER") {
		inventory.removeItem("OBJ_INV_DUSTER");
		inventory.setCursor("ANIM_CURSORARROW");
		script.startScene("Throne_Room.160E_", true);
		debugC(1, kDebugScript, "throne room: Jane takes the duster");
	}
	if (_hotspot3 >= 0 && room.getStep(_hotspot3) == 1 && room.getAnim(_hotspot3) == "ANIM_P10_THRONE_ROOM_HOTSPOT3A") {
		static const char *const kGang[] = { "DAPHNE", "SCOOBY", "SHAGGY", "VELMA", "FRED" };
		for (uint i = 0; i < ARRAYSIZE(kGang); i++) {
			Character *c = script.getCharacter(kGang[i]);
			if (c)
				c->setState(room, sound, Character::kStateScared);
		}
	}
}

void ThroneRoomRoom::event(const Common::String &object, bool success) {
	// 0x417FB0. The throne with the ruby: movie 9, back to this room (game
	// vtable +0x08); the ruby leaves the toolbar and the cursor is the
	// arrow again. Else (the arrow, on the clickable or the requires
	// object) Throne_Room.160X_, blocking.
	if (object != kThrone)
		return;
	if (success) {
		_engine.requestRoom(kThroneRoom, 9);
		Inventory &inventory = *_engine.getInventory();
		inventory.removeItem(kRuby);
		inventory.setCursor("ANIM_CURSORARROW");
		debugC(1, kDebugScript, "throne room: the ruby in the throne");
		return;
	}
	_engine.getScript()->startScene("Throne_Room.160X_", true);
}

void ThroneRoomRoom::saveKeys(Common::INIFile &profile) {
	// 0x418030: JaneMoviePlayed, Entrances.
	profile.setKey("JaneMoviePlayed", kThroneRoom, _janeMoviePlayed ? "Yes" : "No");
	profile.setKey("Entrances", kThroneRoom, Common::String::format("%d", _entrances));
}

void ThroneRoomRoom::loadKeys(const Common::INIFile &profile) {
	// 0x418090 -> 0x40C560, 0x40C670: only Yes reads as true.
	Common::String v;
	_janeMoviePlayed = profile.getKey("JaneMoviePlayed", kThroneRoom, v) && v == "Yes";
	if (profile.getKey("Entrances", kThroneRoom, v))
		_entrances = atoi(v.c_str());
}

RoomClass *createThroneRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kThroneRoom)
		return new ThroneRoomRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
