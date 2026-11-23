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
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"
#include "terraglyph/station_rooms.h"

namespace Terraglyph {

static const char *const kStation = "ROOM_P09_Railroad_Station";
static const char *const kGummyRoom = "ROOM_Gummy";
static const char *const kDentures = "OBJ_INV_DENTURES";

static int findSprite(const Room &room, const char *object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void RailroadStationRoom::reset() {
	// 0x420820 (no base reset in the original's slot).
	_gives = false;
	_thanks = false;
	_hint = 0;
}

const char *RailroadStationRoom::nextHint() {
	if (_engine.getInventory()->hasItem(kDentures) && _hint == 2)
		_hint = 0;
	const int old = _hint++;
	if (old == 0)
		return "Railroad_Station.220C_";
	if (old == 1)
		return "Railroad_Station.220C2_";
	if (old == 2) {
		_hint = 0;
		return "Railroad_Station.220C3_";
	}
	return "";
}

void RailroadStationRoom::arrive(const Common::String &from) {
	// 0x4212C0 (+0x2C): Intro_, or 110A_ while MoaningActive; with the
	// whistle puzzle solved (GummyPuzzle, game +0x274) and his clue not yet
	// given, Gummy's next hint; back here from the dentures, 220DP_. Unplayed.
	RoomScript &script = *_engine.getScript();
	script.setOpening(_engine.getGameFlag("MoaningActive") ? "Railroad_Station.110A_" : "Railroad_Station.Intro_");
	if (_engine.getGameFlag("GummyPuzzle") && !_engine.isClueOwner("GUMMY"))
		script.setOpening(nextHint());
	if (from == kStation)
		script.setOpening("Railroad_Station.220DP_");
}

void RailroadStationRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x4213C0. Gummy and his donkey (OBJ_GUMMY*, OBJ_DONKEY*) not built
	// until Artie has been met; the way to the chase not built until Gummy's
	// clue has been given. Hotspots 2 and 3 are built as the default path
	// builds a clickable (0x408DC0 with 1, -1), and found by name.
	const bool artie = _engine.getInventory()->hasMetSuspect("ARTIE");
	if ((object.hasPrefix("OBJ_GUMMY") || object.hasPrefix("OBJ_DONKEY")) && !artie)
		claim.build = Room::Claim::kSkip;
	else if (object == "OBJ_P09_TO_P40" && !_engine.isClueOwner("GUMMY"))
		claim.build = Room::Claim::kSkip;
}

void RailroadStationRoom::enter(const Common::String &from) {
	// 0x4210F0, after the base entry.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	_lastScene.clear();
	_gummyClicked = 0;
	_donkeyClicked = false;
	_hotspot2 = findSprite(room, "OBJ_P09_RAILROAD_STATION_HOTSPOT2");
	_hotspot3 = findSprite(room, "OBJ_P09_RAILROAD_STATION_HOTSPOT3");
	const bool solved = _engine.getGameFlag("GummyPuzzle");
	Character *gummy = script.getCharacter("GUMMY");
	Character *donkey = script.getCharacter("DONKEY");
	// Before the puzzle, once Artie has been met, Gummy sleeps.
	if (!solved && _engine.getInventory()->hasMetSuspect("ARTIE") && gummy)
		gummy->actLoop(room, *_engine.getSound(), "SLEEPS");
	// After it, the donkey can't be clicked; with his clue given, both stand
	// 100 to the right.
	if (solved && donkey)
		room.setHitType(donkey->getSprite(), 0);
	if (solved && _engine.isClueOwner("GUMMY") && gummy && donkey) {
		room.setOffset(gummy->getSprite(), room.getOffsetX(gummy->getSprite()) + 100, room.getOffsetY(gummy->getSprite()));
		room.setOffset(donkey->getSprite(), room.getOffsetX(donkey->getSprite()) + 100, room.getOffsetY(donkey->getSprite()));
	}
	// Back here from the dentures: his clue (game vtable +0x44), and he
	// gives it when idle.
	if (from == kStation) {
		_engine.giveClue("GUMMY");
		_gives = true;
	}
	if (from == "ROOM_P04_Street2") {
		if (Character *shaggy = script.getCharacter("SHAGGY"))
			shaggy->walkIn(room, *_engine.getSound(), "DR");
	}
}

bool RailroadStationRoom::click(int sprite) {
	// 0x420840 reads Gummy's and the donkey's clicked flags (+8) with the
	// cursor. The class takes the clicks it acts on: Gummy with the arrow,
	// or with the dentures once the puzzle is solved; the donkey with the
	// arrow.
	if (sprite < 0)
		return false;
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	Character *gummy = script.getCharacter("GUMMY");
	Character *donkey = script.getCharacter("DONKEY");
	if (gummy && sprite == gummy->getSprite()) {
		if (inventory.getCursor() == "ANIM_CURSORDENTURES" && _engine.getGameFlag("GummyPuzzle")) {
			_gummyClicked = 2;
			return true;
		}
		if (inventory.isDefaultCursor()) {
			_gummyClicked = 1;
			return true;
		}
	}
	if (donkey && sprite == donkey->getSprite() && inventory.isDefaultCursor()) {
		_donkeyClicked = true;
		return true;
	}
	return false;
}

void RailroadStationRoom::event(const Common::String &object, bool success) {
	// 0x421070: hotspot 2 moved on to its B, HOTSPOT2A_; to its C,
	// HOTSPOT2B_. Held.
	RoomScript &script = *_engine.getScript();
	if (object != "OBJ_P09_RAILROAD_STATION_HOTSPOT2" || _hotspot2 < 0)
		return;
	const Common::String state = script.getClickableState(_hotspot2);
	const char *scene = nullptr;
	if (state == "ANIM_P09_RAILROAD_STATION_HOTSPOT2B")
		scene = "Railroad_Station.HOTSPOT2A_";
	else if (state == "ANIM_P09_RAILROAD_STATION_HOTSPOT2C")
		scene = "Railroad_Station.HOTSPOT2B_";
	if (scene && script.startScene(scene, true))
		_lastScene = scene;
}

void RailroadStationRoom::leave(const char *destination, const char *scene, int movie) {
	// The room's exit handler (+0x24, the base's here) with these keys.
	RoomScript::Exit exit;
	exit.destination = destination;
	exit.scene = scene;
	exit.movie = movie;
	_lastExit = exit;
	_engine.getScript()->takeExit(exit);
}

void RailroadStationRoom::tick() {
	// 0x420840, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	Character *gummy = script.getCharacter("GUMMY");

	// Gummy clicked.
	if (_gummyClicked && gummy) {
		const int how = _gummyClicked;
		_gummyClicked = 0;
		if (how == 2) {
			// The dentures: used up, the arrow back, and the station again
			// through movie 30, with 220D_.
			inventory.removeItem(kDentures);
			inventory.setCursor("ANIM_CURSORARROW");
			leave(kStation, "Railroad_Station.220D_", 30);
		} else if (!_engine.getGameFlag("GummyPuzzle")) {
			leave(kGummyRoom, "", -1);
		} else if (!_engine.isClueOwner("GUMMY")) {
			const char *scene = nextHint();
			if (script.startScene(scene, true))
				_lastScene = scene;
		} else {
			static const char *const kLines[] = { "Global.1000GA_", "Global.1000GB_", "Global.1000GC_" };
			const char *scene = kLines[_engine.getRandomNumber(2)];
			if (script.startScene(scene, false))
				_lastScene = scene;
		}
	}

	// Free, Gummy idle and his clue given: he gives (GIVES), input held.
	const bool free = !script.isScenePlaying() && script.isOpeningDone();
	if (free && gummy && gummy->getState() == Character::kStateIdle && _gives) {
		gummy->act(room, *_engine.getSound(), "GIVES");
		_gives = false;
		_thanks = true;
	}
	// Gummy idle (or looping) again, or not here: the thanks, and the
	// suspects.
	const int state = gummy ? gummy->getState() : Character::kStateIdle;
	if (_thanks && (state == Character::kStateIdle || state == 0xE || state == Character::kStateActLoop)) {
		_thanks = false;
		leave("ROOM_Suspect_Chooser", "Global.Thank_You_", -1);
	}

	// The donkey clicked with the arrow: the whistle puzzle.
	if (_donkeyClicked) {
		_donkeyClicked = false;
		leave(kGummyRoom, "", -1);
	}

	// Hotspot 3 on step 8 of its C: Scooby scared.
	if (_hotspot3 >= 0 && room.getStep(_hotspot3) == 8 && room.getAnim(_hotspot3) == "ANIM_P09_RAILROAD_STATION_HOTSPOT3C") {
		if (Character *scooby = script.getCharacter("SCOOBY"))
			scooby->setState(room, *_engine.getSound(), Character::kStateScared);
	}
}

RoomClass *createStationRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kStation)
		return new RailroadStationRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
