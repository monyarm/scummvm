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

#include "scooby/chase_rooms.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

// Sequence modes (0x430AA0).
static const int kModeOnce = 3;

static const char *const kOilCan = "OBJ_INV_OILCAN";
static const char *const kCorral = "ROOM_P30_Horseshoe_Corral";
static const char *const kCloseup = "ROOM_P25A_Chase_B2_Closeup";

// The movie the corral's run-off plays (game +0x08, push 8), and the one
// before the chase starts again from C5 and C6 (push 0x1F).
static const int kCorralMovie = 8;
static const int kRestartMovie = 31;

static int findPrefixed(const Room &room, const char *prefix) {
	// The placement claim keeps the last sprite it builds.
	int found = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s).hasPrefix(prefix))
			found = s;
	}
	return found;
}

// ---------------------------------------------------------------------------
// Chase B

void ChaseBRoom::reset() {
	// 0x41E780, after the base reset (0x40CD20).
	_countdown = 720;
}

void ChaseBRoom::arrive(const Common::String &from) {
	// 0x41FA80 (room vtable +0x2C): this visit's opening, not played yet.
	RoomScript &script = *_engine.getScript();
	const Common::String &room = _engine.getRoom()->getName();
	if (room == "ROOM_P26_Chase_B3") {
		script.setOpening("Chase_Branch_3.230_");
	} else if (room == "ROOM_P27_Chase_B4" || room == "ROOM_P24_Chase_B1" || from == kCorral) {
		script.setOpening("");
	} else {
		// rand() & 3
		static const char *const kOpenings[] = {
			"Chase_Branch_3.230_", "Chase_Branch_3.230B2_", "Chase_Branch_3.230C_", "Chase_Branch_3.230D_"
		};
		script.setOpening(kOpenings[_engine.getRandomNumber(3)]);
	}
	debugC(1, kDebugScript, "%s: opening '%s'", room.c_str(), script.getOpening().c_str());
}

void ChaseBRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41ED40
	if (object.hasPrefix("OBJ_BRANCH")) {
		// The run-off's sprite, under layer 0 at depth 0, looping.
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
	} else if (object == kOilCan) {
		// A requires object under layer 0 (0x408C80 with -1), unless the
		// oil can was ever taken.
		if (_engine.getInventory()->hasItem(kOilCan)) {
			claim.build = Room::Claim::kSkip;
		} else {
			claim.build = Room::Claim::kRequires;
			claim.layer = 0;
			claim.depth = 0;
		}
	} else if (object == "OBJ_P25_OVERLAY") {
		// Under layer 0 at depth -1, while the safe's combination is still
		// to get and ICDouble has given his clue (0x40A870, game +0xD8).
		if (!_engine.getInventory()->hasItem("OBJ_INV_SAFE_COMBO") && _engine.isClueOwner("ICDOUBLE")) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = -1;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	}
}

void ChaseBRoom::enter(const Common::String &from) {
	// 0x41F650, after the base entry.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	_countdown = 720;
	_pending.clear();
	_lastSound.clear();
	_branch = findPrefixed(room, "OBJ_BRANCH");
	_oilCanSprite = findPrefixed(room, kOilCan);

	// B4 and B1 run straight on.
	if (room.getName() == "ROOM_P27_Chase_B4")
		leave("ROOM_P35_Chase_C6");
	if (room.getName() == "ROOM_P24_Chase_B1")
		leave("ROOM_P39_Chase_C4");

	// Back from the close-up or the corral: the rest of that run-off.
	if (from == kCloseup || from == kCorral) {
		const bool closeup = from == kCloseup;
		_lastSound = closeup ? "Chase.Branch_2" : "Chase.Branch_5_from_HC";
		sound.play(_lastSound);
		if (_branch >= 0)
			room.startAnim(_branch, closeup ? "ANIM_BRANCH2A_FIN" : "ANIM_BRANCH5A_FIN", kModeOnce, -1);
		_pending = closeup ? "ROOM_P38_Chase_C3" : "ROOM_P35_Chase_C6";
		debugC(1, kDebugScript, "%s: back from %s; on to %s", room.getName().c_str(), from.c_str(), _pending.c_str());
	}
	_oilCan = false;

	// ICDouble is never hit here (+0x4 = 0).
	Character *icdouble = _engine.getScript()->getCharacter("ICDOUBLE");
	if (icdouble)
		room.setHitType(icdouble->getSprite(), 0);
}

void ChaseBRoom::leave(const Common::String &destination) {
	// 0x41E7A0: once only. The run-off for the room it leads to, and for B4
	// and C5 and C6, the branch the gang is at; input stops until it has
	// played out. A destination with no run-off still holds the input.
	if (!_pending.empty())
		return;
	_pending = destination;

	struct RunOff {
		const char *destination;
		const char *branch; // the branch sprite's object, if it matters
		int oilCan;         // the oil-can flag, if it matters (-1)
		const char *anim;
		const char *sound;
	};
	static const RunOff kRunOffs[] = {
		{ "ROOM_P25_Chase_B2", nullptr, -1, "ANIM_BRANCH3B", "Chase.Branch_3b" },
		{ "ROOM_P38_Chase_C3", nullptr, -1, "ANIM_BRANCH2B", "Chase.Branch_2" },
		{ kCloseup, nullptr, -1, "ANIM_BRANCH2A", "Chase.Branch_2" },
		{ "ROOM_P37_Chase_C2", nullptr, -1, "ANIM_BRANCH3A", "Chase.Branch_3a" },
		{ "ROOM_P39_Chase_C4", nullptr, -1, "ANIM_BRANCH1A", "Chase.Branch_1a" },
		{ kCorral, nullptr, -1, "ANIM_BRANCH5A", "Chase.Branch_5a" },
		{ "ROOM_P35_Chase_C6", "OBJ_BRANCH5", -1, "ANIM_BRANCH5B", "Chase.Branch_5b" },
		{ "ROOM_P34_Chase_C5", nullptr, 1, "ANIM_BRANCH6C", "Chase.Branch_6c" },
		{ "ROOM_P34_Chase_C5", nullptr, 0, "ANIM_BRANCH6A", "Chase.Branch_6a" },
		{ "ROOM_P27_Chase_B4", "OBJ_BRANCH6", -1, "ANIM_BRANCH6B", "Chase.Branch_6b" },
		{ "ROOM_P35_Chase_C6", "OBJ_BRANCH4", -1, "ANIM_BRANCH4A", "Chase.Branch_4" }
	};
	Room &room = *_engine.getRoom();
	const Common::String branch = _branch >= 0 ? room.getObjectName(_branch) : Common::String();
	for (uint i = 0; i < ARRAYSIZE(kRunOffs); i++) {
		const RunOff &r = kRunOffs[i];
		if (destination != r.destination || (r.branch && branch != r.branch) || (r.oilCan >= 0 && (int)_oilCan != r.oilCan))
			continue;
		if (_branch >= 0)
			room.startAnim(_branch, r.anim, kModeOnce, -1);
		_lastSound = r.sound;
		_engine.getSound()->play(_lastSound);
		debugC(1, kDebugScript, "%s: run-off to %s: %s, %s", room.getName().c_str(), destination.c_str(), r.anim, r.sound);
		return;
	}
	debugC(1, kDebugScript, "%s: to %s, with no run-off for %s", room.getName().c_str(), destination.c_str(), branch.c_str());
}

bool ChaseBRoom::leaveBy(const Common::String &exitObject) {
	// 0x41E7A0 replaces the default handler: the exit's scene and walk-off
	// never play.
	leave(_engine.getScript()->readExit(exitObject).destination);
	return true;
}

void ChaseBRoom::event(const Common::String &object, bool success) {
	// 0x41EBC0: taking the oil can sends the gang on to C5, the long way.
	if (object == kOilCan) {
		_oilCan = true;
		leave("ROOM_P34_Chase_C5");
	}
}

void ChaseBRoom::tick() {
	// 0x41EFC0, after the base tick (0x40B490).
	Room &room = *_engine.getRoom();
	const int old = _countdown--;
	if (old < 0 && _pending.empty()) {
		// The gang runs on by itself, from the rooms that have a way to.
		_countdown = 2160;
		static const char *const kTimeouts[][2] = {
			{ "ROOM_P25_Chase_B2", "ROOM_P38_Chase_C3" },
			{ "ROOM_P26_Chase_B3", "ROOM_P37_Chase_C2" },
			{ "ROOM_P28_Chase_B5", "ROOM_P35_Chase_C6" },
			{ "ROOM_P29_Chase_B6", "ROOM_P34_Chase_C5" }
		};
		for (uint i = 0; i < ARRAYSIZE(kTimeouts); i++) {
			if (room.getName() == kTimeouts[i][0]) {
				debugC(1, kDebugScript, "%s: time is up at tick %u", room.getName().c_str(), _engine.getScript()->getTick());
				leave(kTimeouts[i][1]);
				break;
			}
		}
	}
	if (_pending.empty())
		return;

	// Step 22 of the oil can's run-off: the oil can goes in the toolbar.
	Inventory &inventory = *_engine.getInventory();
	if (_oilCan && _branch >= 0 && room.getStep(_branch) == 22 && !inventory.hasItem(kOilCan)) {
		if (_oilCanSprite >= 0)
			room.setHidden(_oilCanSprite, true);
		inventory.addItem(kOilCan, "ANIM_TOOLBAROILCAN", "ANIM_CURSOROILCAN");
		debugC(1, kDebugScript, "%s: the oil can is in the toolbar at tick %u", room.getName().c_str(), _engine.getScript()->getTick());
	}
	// The run-off has played out: the next room, through a movie to the corral.
	if (_branch >= 0 && room.getMode(_branch) == 0)
		_engine.requestRoom(_pending, _pending == kCorral ? kCorralMovie : -1);
}

// ---------------------------------------------------------------------------
// Chase C

void ChaseCRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x420690: the canned run-off, under layer 0, moved to depth -1, and
	// started once through (graphics_AnimSprite_Start 3, 3, -1) in enter().
	if (object.hasPrefix("OBJ_CANNED")) {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = -1;
	}
}

void ChaseCRoom::enter(const Common::String &from) {
	// 0x420510, after the base entry: input held, the cursor hidden, and
	// the run-off's sound.
	Room &room = *_engine.getRoom();
	_leaving = false;
	_lastSound.clear();
	_canned = findPrefixed(room, "OBJ_CANNED");
	if (_canned >= 0)
		room.startAnim(_canned, room.getAnim(_canned), kModeOnce, -1);
	static const char *const kRooms[] = {
		"ROOM_P33_Chase_C1", "ROOM_P37_Chase_C2", "ROOM_P38_Chase_C3",
		"ROOM_P39_Chase_C4", "ROOM_P34_Chase_C5", "ROOM_P35_Chase_C6"
	};
	for (uint i = 0; i < ARRAYSIZE(kRooms); i++) {
		if (room.getName() == kRooms[i]) {
			_lastSound = Common::String::format("Chase.Canned_%u", i + 1);
			_engine.getSound()->play(_lastSound);
			debugC(1, kDebugScript, "%s: %s", room.getName().c_str(), _lastSound.c_str());
			break;
		}
	}
}

void ChaseCRoom::tick() {
	// 0x41FC10, after the base tick: when the run-off has played out, the
	// default exit handler (0x40C1E0) to the next branch, or back to the
	// chase's start through movie 31.
	Room &room = *_engine.getRoom();
	if (_leaving || _canned < 0 || room.getMode(_canned) != 0)
		return;
	static const struct {
		const char *room;
		const char *destination;
		int movie;
	} kExits[] = {
		{ "ROOM_P33_Chase_C1", "ROOM_P26_Chase_B3", -1 },
		{ "ROOM_P37_Chase_C2", "ROOM_P24_Chase_B1", -1 },
		{ "ROOM_P38_Chase_C3", "ROOM_P28_Chase_B5", -1 },
		{ "ROOM_P39_Chase_C4", "ROOM_P29_Chase_B6", -1 },
		{ "ROOM_P34_Chase_C5", "ROOM_P40_Chase_Start", kRestartMovie },
		{ "ROOM_P35_Chase_C6", "ROOM_P40_Chase_Start", kRestartMovie }
	};
	for (uint i = 0; i < ARRAYSIZE(kExits); i++) {
		if (room.getName() == kExits[i].room) {
			_leaving = true;
			debugC(1, kDebugScript, "%s: run-off over at tick %u; to %s, movie %d", room.getName().c_str(),
			       _engine.getScript()->getTick(), kExits[i].destination, kExits[i].movie);
			_engine.exitTo(kExits[i].destination, kExits[i].movie);
			return;
		}
	}
}

RoomClass *createChaseRoom(ScoobyEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0); Phantom has no chase.
	static const char *const kBRooms[] = {
		"ROOM_P24_Chase_B1", "ROOM_P25_Chase_B2", "ROOM_P26_Chase_B3",
		"ROOM_P27_Chase_B4", "ROOM_P28_Chase_B5", "ROOM_P29_Chase_B6"
	};
	static const char *const kCRooms[] = {
		"ROOM_P33_Chase_C1", "ROOM_P37_Chase_C2", "ROOM_P38_Chase_C3",
		"ROOM_P39_Chase_C4", "ROOM_P34_Chase_C5", "ROOM_P35_Chase_C6"
	};
	for (uint i = 0; i < ARRAYSIZE(kBRooms); i++) {
		if (room == kBRooms[i])
			return new ChaseBRoom(engine);
	}
	for (uint i = 0; i < ARRAYSIZE(kCRooms); i++) {
		if (room == kCRooms[i])
			return new ChaseCRoom(engine);
	}
	return nullptr;
}

} // End of namespace Scooby
