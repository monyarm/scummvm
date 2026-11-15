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

#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/room_setup.h"
#include "scooby/sound.h"
#include "scooby/story_rooms.h"

namespace Scooby {

// Sequence modes (0x430AA0).
static const int kModeOnce = 3;

static const char *const kRoomName = "ROOM_P01_Town_Center";
static const char *const kSign = "OBJ_INV_SIGN";
static const char *const kHangingSign = "OBJ_P01_TC_HOTSPOT4";
static const char *const kHotspot2 = "OBJ_P01_TC_HOTSPOT2";

void TownCenterRoom::reset() {
	// 0x42D030, after the base reset (0x40CD20): one snack needed (+0x24,
	// in the setup table), the first moan after a second.
	_moanDelay = Room::kTicksPerSecond;
	_exitScene = 0;
	_signClicked = false;
	_takeSign = false;
	_taking = false;
}

int TownCenterRoom::findSprite(const Common::String &object) const {
	const Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

bool TownCenterRoom::isMoaning() const {
	// The story's MoaningActive (game +0x254): the new game sets it
	// (0x42DBC0) and only the Saloon clears it (0x414E95).
	return _engine.getGameFlag("MoaningActive");
}

void TownCenterRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42D800
	if (object == kSign) {
		// A requires object in layer 1, unless the sign was ever taken; it
		// shows once the hanging sign has come down.
		if (_engine.getInventory()->hasItem(kSign)) {
			claim.build = Room::Claim::kSkip;
			return;
		}
		claim.build = Room::Claim::kRequires;
		claim.layer = 1;
		claim.depth = 1;
		claim.hidden = !_signClicked;
	} else if (object == kHangingSign) {
		// A clickable in layer 1, gone once it has been clicked.
		claim.build = Room::Claim::kClickable;
		claim.layer = 1;
		claim.depth = 1;
		claim.hidden = _signClicked;
	}
	// OBJ_P01_TC_HOTSPOT2 is built as the default path builds it; the class
	// only keeps it.
}

void TownCenterRoom::enter(const Common::String &from) {
	// 0x42D270, after the base entry: one of the gang walks in from the
	// street the gang came from.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	const char *who = nullptr;
	const char *direction = nullptr;
	if (from == "ROOM_P03_Street1") {
		who = "FRED";
		direction = "UL"; // 0x405EE0
	} else if (from == "ROOM_P04_Street2") {
		who = "VELMA";
		direction = "U"; // 0x405FE0
	} else if (from == "ROOM_P11_Street3") {
		who = "SHAGGY";
		direction = "UR"; // 0x405E60
	}
	if (who) {
		Character *c = script.getCharacter(who);
		if (c) {
			c->walkIn(room, sound, direction);
			debugC(1, kDebugScript, "Town Center: %s walks in %s", who, direction);
		}
	}
	_takeSign = false;
	_taking = false;
	_takes = -1;
	_hangingSign = findSprite(kHangingSign);
	_sign = findSprite(kSign);
	_hotspot2 = findSprite(kHotspot2);
}

void TownCenterRoom::event(const Common::String &object, bool success) {
	// 0x42D7B0
	if (object == kHangingSign) {
		_signClicked = true;
		debugC(1, kDebugScript, "Town Center: the hanging sign was clicked");
	} else if (object == kSign) {
		_takeSign = true;
		debugC(1, kDebugScript, "Town Center: the sign was clicked; Scooby will take it");
	}
}

void TownCenterRoom::tick() {
	// 0x42D3F0, after the base tick (0x40B490).
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();

	// A moan every 5-14 seconds while the story has them.
	if (_moanDelay-- <= 0 && isMoaning()) {
		const Common::String moan = Common::String::format("Town_Center.Moan.%u", _engine.getRandomNumber(3) + 1);
		sound.play(moan);
		_moanDelay = (_engine.getRandomNumber(9) + 5) * Room::kTicksPerSecond;
		debugC(1, kDebugScript, "Town Center: %s at tick %u; the next in %d ticks", moan.c_str(), script.getTick(), _moanDelay);
	}

	// The hanging sign's click played out: the sign is on the ground.
	if (_signClicked && _hangingSign >= 0 && !room.isHidden(_hangingSign) && room.getMode(_hangingSign) == 0) {
		if (_sign >= 0)
			room.setHidden(_sign, false);
		room.setHidden(_hangingSign, true);
		script.startScene("Town_Center.307_", true);
		debugC(1, kDebugScript, "Town Center: the sign is down");
	}

	// Step 7 of the second hotspot's C state frightens three of the gang.
	if (_hotspot2 >= 0 && room.getStep(_hotspot2) == 7 && room.getAnim(_hotspot2) == "ANIM_P01_TC_HOTSPOT2C") {
		static const char *const kFrightened[] = { "DAPHNE", "SCOOBY", "SHAGGY" };
		for (uint i = 0; i < ARRAYSIZE(kFrightened); i++) {
			Character *c = script.getCharacter(kFrightened[i]);
			if (c)
				c->setState(room, sound, Character::kStateScared);
		}
	}

	if (!_takeSign)
		return;
	// Scooby takes the sign once he is idle, no scene plays, and the opening
	// is done. Input is held until he has it.
	Character *scooby = script.getCharacter("SCOOBY");
	if (!_taking && scooby && scooby->getState() == Character::kStateIdle && !script.isScenePlaying() &&
	    script.isOpeningDone()) {
		_taking = true;
		room.setHidden(scooby->getSprite(), true);
		const int s = scooby->getSprite();
		_takes = room.addEngineSprite(room.getLayerSprite(1), "OBJ_SCOOBY_P01_TAKES_SIGN", "ANIM_SCOOBY_P01_TAKES_SIGN1", 10,
		                              room.getX(s), room.getY(s), 1, "OBJ_SCOOBY_P01_TAKES_SIGN");
		room.startAnim(_takes, "ANIM_SCOOBY_P01_TAKES_SIGN1", kModeOnce, -1);
		sound.play("Scooby.Takes.Sign");
		if (_sign >= 0)
			room.setHidden(_sign, true);
		debugC(1, kDebugScript, "Town Center: Scooby takes the sign at tick %u", script.getTick());
	}
	if (_taking && _takes >= 0 && room.getMode(_takes) == 0) {
		_takeSign = false;
		_taking = false;
		if (scooby)
			room.setHidden(scooby->getSprite(), false);
		room.setHidden(_takes, true);
		_takes = -1;
		Inventory &inventory = *_engine.getInventory();
		if (!inventory.hasItem(kSign))
			inventory.addItem(kSign, "ANIM_TOOLBARPOP_SIGN", "ANIM_CURSORPOP_SIGN");
		script.startScene("Town_Center.360C_", true);
		debugC(1, kDebugScript, "Town Center: Scooby has the sign at tick %u", script.getTick());
	}
}

bool TownCenterRoom::leaveBy(const Common::String &exitObject) {
	// 0x42D060: until Scooby has eaten a snack here, an exit plays one of
	// three scenes in turn and flashes the snack box instead.
	RoomScript &script = *_engine.getScript();
	if (script.getMemory(_engine.getRoom()->getName()).snacksGiven != 0)
		return false;
	static const char *const kScenes[] = { "Town_Center.100A_", "Town_Center.100A2_", "Town_Center.100A3_" };
	script.startScene(kScenes[_exitScene], true);
	debugC(1, kDebugScript, "Town Center: no snack yet; %s", kScenes[_exitScene]);
	_exitScene = (_exitScene + 1) % ARRAYSIZE(kScenes);
	_engine.getInventory()->flashSnackBox();
	return true;
}

void TownCenterRoom::idle() {
	// 0x42D340: rand() & 3; the 100B scenes only while the story has moans.
	static const char *const kScenes[] = { "Town_Center.100B_", "Town_Center.100B2_", "Town_Center.100B3_" };
	const uint r = _engine.getRandomNumber(3);
	if (r == 3)
		_engine.getScript()->startScene("Town_Center.1100D_", false);
	else if (isMoaning())
		_engine.getScript()->startScene(kScenes[r], false);
}

void TownCenterRoom::saveKeys(Common::INIFile &profile) {
	// 0x42D970: the base keys, then SignClicked, in the room's section.
	profile.setKey("SignClicked", kRoomName, _signClicked ? "Yes" : "No");
}

void TownCenterRoom::loadKeys(const Common::INIFile &profile) {
	// 0x42D9B0 -> 0x40C560: only Yes reads as true; a missing key reads as
	// empty, so false.
	Common::String v;
	_signClicked = profile.getKey("SignClicked", kRoomName, v) && v == "Yes";
}

// The gang walking in from the room left (0x405E60 UR, 0x405EA0 DR,
// 0x405EE0 UL, 0x405F20 DL, 0x405FE0 U).
static void walkIn(ScoobyEngine &engine, const char *who, const char *direction) {
	Character *c = engine.getScript()->getCharacter(who);
	if (!c)
		return;
	c->walkIn(*engine.getRoom(), *engine.getSound(), direction);
	debugC(1, kDebugScript, "%s: %s walks in %s", engine.getRoom()->getName().c_str(), who, direction);
}

static int findPlaced(ScoobyEngine &engine, const Common::String &object) {
	const Room &room = *engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Street 2

void Street2Room::prepare() {
	// 0x42CB40: Street 1's moaning opening on every visit while the story
	// has moans; none otherwise.
	_engine.getScript()->setOpening(_engine.getGameFlag("MoaningActive") ? "Street_1.130A_" : "");
}

void Street2Room::enter(const Common::String &from) {
	// 0x42C440, after the base entry.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Character *gummy = script.getCharacter("GUMMY");
	Character *donkey = script.getCharacter("DONKEY");
	if (gummy) {
		// Asleep until his puzzle; moved along once he has given his clue.
		if (!_engine.getGameFlag("GummyPuzzle"))
			gummy->actLoop(room, *_engine.getSound(), "SLEEPS");
		else if (_engine.isClueOwner("GUMMY")) {
			room.setOffset(gummy->getSprite(), room.getX(gummy->getSprite()) + 50, room.getY(gummy->getSprite()));
			if (donkey)
				room.setOffset(donkey->getSprite(), room.getX(donkey->getSprite()) + 50, room.getY(donkey->getSprite()));
		}
		// Neither is ever hit (+0x4 = 0).
		room.setHitType(gummy->getSprite(), 0);
		if (donkey)
			room.setHitType(donkey->getSprite(), 0);
	}
	if (from == "ROOM_P10_Front_Of_General_Store") {
		walkIn(_engine, "DAPHNE", "UR");
	} else if (from == "ROOM_P09_Railroad_Station") {
		walkIn(_engine, "FRED", "UL");
	} else if (from == kRoomName) {
		walkIn(_engine, "FRED", "DL");
		walkIn(_engine, "DAPHNE", "DR");
	}
}

void Street2Room::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42C7A0: Gummy and his donkey only once Artie is met.
	if ((object.hasPrefix("OBJ_DONKEY") || object.hasPrefix("OBJ_GUMMY")) && !_engine.getInventory()->hasMetSuspect("ARTIE")) {
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_P04_STREET2_CLOCK") {
		// Its face (A), or none once the clock is taken (B), in layer 0.
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
		claim.anim = _engine.getInventory()->hasItem("OBJ_INV_CLOCK") ? "ANIM_P04_STREET2_CLOCKB" : "ANIM_P04_STREET2_CLOCKA";
	} else if (object == "OBJ_P04_STREET2_WINDOW") {
		// Only once Gummy has given his clue, never hit.
		if (_engine.isClueOwner("GUMMY")) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = -1;
			claim.hitType = 0;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	}
}

void Street2Room::event(const Common::String &object, bool success) {
	// 0x42CAE0: the hotspots' lines, input left on.
	if (object == "OBJ_P04_STREET2_HOTSPOT1")
		_engine.getScript()->startScene("Street_2.100C_", false);
	else if (object == "OBJ_P04_STREET2_HOTSPOT2")
		_engine.getScript()->startScene("Street_2.100D_", false);
}

void Street2Room::leaving(const Common::String &destination, Common::String &scene) {
	// 0x42C600: to the general store, one of two lines (rand() & 1).
	if (destination == "ROOM_P10_Front_Of_General_Store")
		scene = _engine.getRandomNumber(1) == 0 ? "Street_2.General_Store1_" : "Street_2.General_Store2_";
}

// ---------------------------------------------------------------------------
// Street 3

void Street3Room::reset() {
	// 0x42CBE0
	_moanDelay = Room::kTicksPerSecond;
	_musicBack = true;
	_hotspotScene = 0;
}

void Street3Room::enter(const Common::String &from) {
	// 0x42CE50, after the base entry.
	if (from == kRoomName)
		walkIn(_engine, "FRED", "DL");
	else if (from == "ROOM_P12_Saloon")
		walkIn(_engine, "VELMA", "U");
	else if (from == "ROOM_P13_Bank")
		walkIn(_engine, "SHAGGY", "UR");
	_musicBack = true;
	_hotspot2 = findPlaced(_engine, "OBJ_P11_STREET3_HOTSPOT2");
	_hotspot3 = findPlaced(_engine, "OBJ_P11_STREET3_HOTSPOT3");
}

void Street3Room::tick() {
	// 0x42CD10, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	if (_moanDelay-- <= 0 && _engine.getGameFlag("MoaningActive")) {
		const Common::String moan = Common::String::format("Street_3.Moan.%u", _engine.getRandomNumber(3) + 1);
		_engine.getSound()->play(moan);
		_moanDelay = (_engine.getRandomNumber(9) + 5) * Room::kTicksPerSecond;
		debugC(1, kDebugScript, "Street 3: %s at tick %u", moan.c_str(), script.getTick());
	}
	// Hotspot 2 played out: the music group again, its next variant.
	if (_hotspot2 >= 0 && room.getMode(_hotspot2) == 0 && _musicBack) {
		_musicBack = false;
		_engine.getMusic()->setGroup(2);
	}
	// Step 3 of hotspot 3's B state: its scene.
	if (_hotspot3 >= 0 && room.getStep(_hotspot3) == 3 && room.getAnim(_hotspot3) == "ANIM_P11_STREET3_HOTSPOT3B")
		script.startScene("Street_3.HOTSPOT3_", true);
}

void Street3Room::event(const Common::String &object, bool success) {
	// 0x42CC10
	if (object == "OBJ_P11_STREET3_HOTSPOT") {
		static const char *const kScenes[] = { "Street_3.130X1_", "Street_3.130X2_", "Street_3.130X3_" };
		_engine.getScript()->startScene(kScenes[_hotspotScene], true);
		_hotspotScene = (_hotspotScene + 1) % ARRAYSIZE(kScenes);
	} else if (object == "OBJ_P11_STREET3_HOTSPOT2") {
		// The music leaves its group; it comes back when the hotspot is done.
		_engine.getMusic()->setGroup(-1);
		_musicBack = true;
	}
}

void Street3Room::idle() {
	// 0x42CCC0: rand() & 1.
	if (_engine.getRandomNumber(1) == 0) {
		if (_engine.getGameFlag("MoaningActive"))
			_engine.getScript()->startScene("Street_3.100B2_", false);
	} else {
		_engine.getScript()->startScene("Street_3.1100A_", false);
	}
}

RoomClass *createStoryRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kRoomName)
		return new TownCenterRoom(engine);
	if (room == "ROOM_P04_Street2")
		return new Street2Room(engine);
	if (room == "ROOM_P11_Street3")
		return new Street3Room(engine);
	return nullptr;
}
} // End of namespace Scooby
