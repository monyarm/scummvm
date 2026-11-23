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

// Phantom of the Knight's dining room. The addresses are Phantom's.

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/dining_rooms.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const char *const kDining = "ROOM_P12_Dining_Room";
static const char *const kMariaRoom = "ROOM_Maria";

void DiningRoomRoom::reset() {
	// 0x40FD60: nothing pending, Maria's movie not played.
	_giving = false;
	_thanks = false;
	_moviePlayed = false;
}

void DiningRoomRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x40FEE0, everything under the second layer (game +0x5C):
	// - the burger, an inventory object (0x408BD0) at depth 1, hidden until
	//   Maria's game is won (MariaPuzzle, game +0x255);
	// - the mug, an exit (0x401850) at depth 1000;
	// - hotspots 2, 3 and 4, clickables (0x408DC0 with 1) at depths 10000,
	//   1 and 2.
	// Anything else the default path builds.
	if (object == "OBJ_INV_BURGER") {
		claim.build = Room::Claim::kInventory;
		claim.layer = 1;
		claim.depth = 1;
		claim.hidden = !_engine.getGameFlag("MariaPuzzle");
		return;
	}
	if (object == "OBJ_P12_DINING_ROOM_MUG") {
		claim.build = Room::Claim::kExit;
		claim.layer = 1;
		claim.depth = 1000;
		return;
	}
	static const struct {
		const char *object;
		int depth;
	} kHotspots[] = { { "OBJ_P12_DINING_ROOM_HOTSPOT2", 10000 }, { "OBJ_P12_DINING_ROOM_HOTSPOT3", 1 }, { "OBJ_P12_DINING_ROOM_HOTSPOT4", 2 } };
	for (uint i = 0; i < ARRAYSIZE(kHotspots); i++) {
		if (object == kHotspots[i].object) {
			claim.build = Room::Claim::kClickable;
			claim.layer = 1;
			claim.depth = kHotspots[i].depth;
			return;
		}
	}
}

void DiningRoomRoom::enter(const Common::String &from) {
	// 0x40FD80. The first time, Maria's movie: MariaMoviePlayed is set and
	// movie 35 plays into her game (game vtable +0x08, ROOM_Maria), in place
	// of the base entry; the fork's base entry has run already. After, Maria
	// is met (0x40A8B0); with her game won and her clue not yet given
	// (0x40A870), Dining_Room.350A_, blocking, her clue (game vtable +0x44),
	// and she gives it when idle (+0xB8). One snack is needed (+0x24).
	_clicked = -1;
	if (!_moviePlayed) {
		_moviePlayed = true;
		_engine.requestRoom(kMariaRoom, 35);
		debugC(1, kDebugScript, "dining room: from %s; Maria's movie", from.c_str());
		return;
	}
	_engine.getInventory()->meetSuspect("MARIA");
	if (!_engine.isClueOwner("MARIA") && _engine.getGameFlag("MariaPuzzle")) {
		_engine.getScript()->startScene("Dining_Room.350A_", true);
		_engine.giveClue("MARIA");
		_giving = true;
	}
	_engine.getScript()->setSnacksNeeded(1);
	debugC(1, kDebugScript, "dining room: from %s%s", from.c_str(), _giving ? "; Maria's clue" : "");
}

bool DiningRoomRoom::click(int sprite) {
	_clicked = sprite;
	return false;
}

void DiningRoomRoom::tick() {
	// 0x410110, after the base tick. Maria clicked with the arrow (game
	// +0x20 bit 4, her +0x08) before her game is won: to her game, through
	// the room's exit handler (+0x24). Then, Maria idle: with her clue given
	// she GIVES (state 0xF), and input is held with the cursor hidden; idle
	// again, the exit handler takes the gang to ROOM_Suspect_Chooser with
	// Global.Thank_You_.
	RoomScript &script = *_engine.getScript();
	const int clicked = _clicked;
	_clicked = -1;
	Character *maria = script.getCharacter("MARIA");
	if (maria && clicked >= 0 && clicked == maria->getSprite() && _engine.getInventory()->isDefaultCursor() &&
	    !_engine.getGameFlag("MariaPuzzle")) {
		RoomScript::Exit exit;
		exit.destination = kMariaRoom;
		exit.movie = -1;
		script.takeExit(exit);
		debugC(1, kDebugScript, "dining room: Maria clicked, to her game");
	}
	if (maria) {
		if (maria->getState() != Character::kStateIdle)
			return;
		if (_giving) {
			maria->act(*_engine.getRoom(), *_engine.getSound(), "GIVES");
			_giving = false;
			_thanks = true;
		}
		if (maria->getState() != Character::kStateIdle)
			return;
	}
	if (_thanks) {
		_thanks = false;
		RoomScript::Exit exit;
		exit.destination = "ROOM_Suspect_Chooser";
		exit.scene = "Global.Thank_You_";
		exit.movie = -1;
		script.takeExit(exit);
		debugC(1, kDebugScript, "dining room: thanks, then the suspects and clues");
	}
}

void DiningRoomRoom::idle() {
	// 0x4100C0 (+0x34): with Velma idle, her GLASSES and Global.1000A2_,
	// which doesn't hold input.
	RoomScript &script = *_engine.getScript();
	Character *velma = script.getCharacter("VELMA");
	if (velma && velma->getState() == Character::kStateIdle) {
		velma->act(*_engine.getRoom(), *_engine.getSound(), "GLASSES");
		script.startScene("Global.1000A2_", false);
	}
}

void DiningRoomRoom::event(const Common::String &object, bool success) {
	// 0x410610: hotspot 2 scares Daphne, Shaggy, Scooby, Fred and Velma
	// (state 0x11).
	if (object != "OBJ_P12_DINING_ROOM_HOTSPOT2")
		return;
	RoomScript &script = *_engine.getScript();
	static const char *const kGang[] = { "DAPHNE", "SHAGGY", "SCOOBY", "FRED", "VELMA" };
	for (uint i = 0; i < ARRAYSIZE(kGang); i++) {
		Character *c = script.getCharacter(kGang[i]);
		if (c)
			c->setState(*_engine.getRoom(), *_engine.getSound(), Character::kStateScared);
	}
}

Common::String DiningRoomRoom::idleLine(RoomScript &script, const Common::String &name) {
	// 0x410550: Maria's, once her game is won, by rand() % 4: 1010I1_, or
	// 1010I3_ for 1 and 2, or 1010I4_. Else the base line.
	if (name == "MARIA" && _engine.getGameFlag("MariaPuzzle")) {
		static const char *const kLines[] = { "Dining_Room.1010I1_1_MARIA", "Dining_Room.1010I3_1_MARIA", "Dining_Room.1010I3_1_MARIA",
		                                      "Dining_Room.1010I4_1_MARIA" };
		return kLines[_engine.getRandomNumber(3)];
	}
	return script.getBaseIdleLine(name);
}

void DiningRoomRoom::saveKeys(Common::INIFile &profile) {
	// 0x4104D0: MariaMoviePlayed.
	profile.setKey("MariaMoviePlayed", kDining, _moviePlayed ? "Yes" : "No");
}

void DiningRoomRoom::loadKeys(const Common::INIFile &profile) {
	// 0x410510 -> 0x40C560: only Yes reads as true.
	Common::String v;
	_moviePlayed = profile.getKey("MariaMoviePlayed", kDining, v) && v == "Yes";
}

RoomClass *createDiningRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kDining)
		return new DiningRoomRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
