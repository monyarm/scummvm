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

// Phantom of the Knight's wizard's skull and library (WP-077), the first of the
// audit's group-8 (Upstairs) rooms. The addresses are Phantom's.

#include <math.h>

#include "common/debug.h"

#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"
#include "scooby/wizards_rooms.h"

namespace Scooby {

static const char *const kSkull = "ROOM_Wizards_Skull";
static const char *const kLibrary = "ROOM_P25_Wizards_Library";
static const char *const kMirrorRoom = "ROOM_P08_Magic_Mirror_Room";
static const char *const kBedroom = "ROOM_P24_Wizards_Bedroom";
static const char *const kBigHead = "ROOM_Big_Head";

static const char *const kSkullHand = "OBJ_SKULL_HAND";
static const char *const kSkullExit = "OBJ_SKULL_EXIT";
static const char *const kSpellBook = "OBJ_INV_SPELL_BOOK";

static const char *const kShelf = "OBJ_P25_WIZARDS_LIBRARY_SHELF";
static const char *const kUnderlay = "OBJ_P25_WIZARDS_LIBRARY_UNDERLAY";
static const char *const kToP24 = "OBJ_P25_TO_P24";
static const char *const kLibHotspot1 = "OBJ_P25_WIZARDS_LIBRARY_HOTSPOT1";
static const char *const kLibHotspot3 = "OBJ_P25_WIZARDS_LIBRARY_HOTSPOT3";

// 0x4070C0 pair (the gallery's isMirrorPlaced idiom): the item is held but no
// longer in a toolbar slot, i.e. it has been used.
static bool used(ScoobyEngine &engine, const char *item) {
	if (!engine.getInventory()->hasItem(item))
		return false;
	for (uint i = 0; i < Inventory::kSlotCount; i++)
		if (engine.getInventory()->getSlot(i) == item)
			return false;
	return true;
}

// ---------------------------------------------------------------------------
// The wizard's skull (the Skull Puzzle room)

const char *WizardsSkullRoom::kIntro = "Wizards_Library.300D_";
const char *WizardsSkullRoom::kDemo = "Magic_Mirror_Room.Wizard_";
const char *WizardsSkullRoom::kSlide = "Wizards_Skull.Skull_Slide";
const char *WizardsSkullRoom::kHandLift = "ANIM_SKULL_HANDLIFT";
const char *WizardsSkullRoom::kHandTurn = "ANIM_SKULL_HANDTURN";
const char *WizardsSkullRoom::kHandReturn = "ANIM_SKULL_HANDRETURN";

int WizardsSkullRoom::findSprite(const Common::String &object) const {
	const Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++)
		if (room.getObjectName(s) == object)
			return s;
	return -1;
}

void WizardsSkullRoom::reset() {
	// 0x42D8B0, after the base reset: the four rotations and the room's flags.
	for (int i = 0; i < 4; i++) {
		_state[i] = 0;
		_skull[i] = -1;
	}
	_introDone = false;
	_recheck = false;
	_handMode = 0;
	_handStep = 0;
	_turnCount = 0;
	_hand = -1;
}

void WizardsSkullRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42DBF0: the four skulls are clickables, each showing the face its saved
	// rotation names (ANIM_SKULL{n} + 'A'+state); the wizard's hand a sprite
	// hidden until the demo shows it; the exit back to the magic mirror room.
	for (int i = 0; i < 4; i++) {
		if (object == Common::String::format("OBJ_SKULL%d", i + 1)) {
			claim.build = Room::Claim::kClickable;
			claim.anim = Common::String::format("ANIM_SKULL%d", i + 1) + (char)('A' + _state[i]);
			return;
		}
	}
	if (object == kSkullHand) {
		// Built, but shown only while the hand demo runs (enter from off the
		// mirror room). The engine places it at its object.ini position.
		claim.build = Room::Claim::kSprite;
		claim.hidden = true;
	} else if (object == kSkullExit) {
		// The way back to the magic mirror room (its destination is in object.ini).
		claim.build = Room::Claim::kExit;
	}
}

void WizardsSkullRoom::enter(const Common::String &from) {
	// 0x42D910, after the base entry. The four skulls must be in the scene
	// ("4 Skulls must be placed in the skull room!"). From the magic mirror room
	// the gang attempts the puzzle (SkullPuzzleAttempted; the first time the
	// wizard's 300D_ intro plays); from anywhere else the wizard's hand
	// demonstrates the combination (SkullMovie) and returns to the mirror room.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();

	for (int i = 0; i < 4; i++)
		_skull[i] = findSprite(Common::String::format("OBJ_SKULL%d", i + 1));
	if (_skull[0] < 0 || _skull[1] < 0 || _skull[2] < 0 || _skull[3] < 0)
		debugC(1, kDebugScript, "wizard's skull: not all four skulls placed");

	_hand = findSprite(kSkullHand);
	_handMode = 0;
	_handStep = 0;
	_turnCount = 0;

	if (from == kMirrorRoom) {
		// From the magic mirror room: the wizard's hand demonstrates the
		// combination. Reset the four skulls to face A, show the hand at its
		// rest, and start the easing state machine (input held); it ends by
		// returning to the mirror room.
		_engine.setGameFlag("SkullMovie", true);
		for (int i = 0; i < 4; i++)
			if (_skull[i] >= 0)
				room.startAnim(_skull[i], Common::String::format("ANIM_SKULL%d", i + 1) + 'A', 3, 0, 3);
		if (_hand >= 0) {
			room.setHidden(_hand, false);
			_handRelX = room.getOffsetX(_hand) - (_skull[0] >= 0 ? room.getOffsetX(_skull[0]) : 0);
			_handRelY = room.getOffsetY(_hand) - (_skull[0] >= 0 ? room.getOffsetY(_skull[0]) : 0);
			_handX = (float)room.getOffsetX(_hand);
			_handY = (float)room.getOffsetY(_hand);
		}
		script.startScene(kDemo, true);
		_handMode = 1;
		debugC(1, kDebugScript, "wizard's skull: the hand demo (from %s)", from.c_str());
		return;
	}

	// From anywhere else the gang attempts the puzzle; the first time the
	// wizard's 300D_ intro plays. The exit back to the mirror room is built by
	// the claim.
	_engine.setGameFlag("SkullPuzzleAttempted", true);
	if (!_introDone) {
		script.startScene(kIntro, true);
		_introDone = true;
		debugC(1, kDebugScript, "wizard's skull: the intro");
	}
	debugC(1, kDebugScript, "wizard's skull: attempt (from %s)", from.c_str());
}

void WizardsSkullRoom::roomLeft() {
	// 0x42DBD0, after the base leave: drop the skull-sprite handles.
	for (int i = 0; i < 4; i++)
		_skull[i] = -1;
}

void WizardsSkullRoom::event(const Common::String &object, bool success) {
	// 0x42E060: a click on any skull (the original matches the first nine
	// characters, "OBJ_SKULL") slides it (the engine's clickable cycles its
	// face) and plays Skull_Slide; the tick then re-reads the four faces.
	(void)success;
	if (!object.hasPrefix("OBJ_SKULL"))
		return;
	// The hand and the exit share the prefix but are not clickables, so only a
	// true skull click reaches here; guard anyway.
	if (object == kSkullHand || object == kSkullExit)
		return;
	_engine.getSound()->play(kSlide);
	_recheck = true;
	debugC(1, kDebugScript, "wizard's skull: %s slid", object.c_str());
}

void WizardsSkullRoom::tick() {
	// 0x42E100, after the base tick: the hand demo (0x42E3F0) and then the
	// solve/exit check (0x42E280).
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();

	// --- the wizard-hand demonstration (0x42E3F0) ---
	// An easing state machine: the hand walks to each skull in turn (5 px/tick),
	// turns it SkullCombo{n} times (queuing a click each time), then parks off
	// the bottom of the screen and the demo is done (state 6).
	auto handIdle = [&]() { return _hand < 0 || room.getMode(_hand) == 0; };
	switch (_handMode) {
	case 1: { // approach the next skull that needs turning
		_turnCount = 0;
		while (_handStep < 4 && _engine.getGameInt(Common::String::format("SkullCombo%d", _handStep)) == 0)
			_handStep++;
		float tx, ty;
		const int ref = _handStep < 4 ? _handStep : 3;
		tx = (float)(room.getOffsetX(_skull[ref]) + _handRelX);
		ty = (float)(room.getOffsetY(_skull[ref]) + _handRelY) + (_handStep < 4 ? 0.0f : 480.0f);
		const float dx = tx - _handX, dy = ty - _handY;
		const float dist = sqrtf(dx * dx + dy * dy);
		if (dist > 5.0f) {
			_handX += 5.0f / dist * dx;
			_handY += 5.0f / dist * dy;
		} else {
			_handX = tx;
			_handY = ty;
		}
		if (_hand >= 0)
			room.setOffset(_hand, (int)_handX, (int)_handY);
		if (dist == 0.0f) { // was already at the target: lift
			if (_hand >= 0)
				room.startAnim(_hand, kHandLift, 3, 0, 3);
			_handMode = 2;
		}
		break;
	}
	case 2: // at the skull: start turning it, or finish if past the last
		if (_handStep == 4) {
			_handMode = 6;
			return;
		}
		if (handIdle()) {
			_turnCount = 0;
			if (_hand >= 0)
				room.startAnim(_hand, kHandTurn, 3, 0, 3);
			_handMode = 3;
			if (_skull[_handStep] >= 0)
				script.queueClickable(_skull[_handStep]);
		}
		break;
	case 3: // a turn landed: another if the count is short of the combo, else lift
		if (handIdle()) {
			_turnCount++;
			if (_turnCount == _engine.getGameInt(Common::String::format("SkullCombo%d", _handStep))) {
				if (_hand >= 0)
					room.startAnim(_hand, kHandLift, 3, 0, 3);
				_handMode = 5;
			} else {
				if (_hand >= 0)
					room.startAnim(_hand, kHandReturn, 3, 0, 3);
				_handMode = 4;
			}
		}
		break;
	case 4: // return, then turn the same skull again
		if (handIdle()) {
			if (_hand >= 0)
				room.startAnim(_hand, kHandTurn, 3, 0, 3);
			if (_skull[_handStep] >= 0)
				script.queueClickable(_skull[_handStep]);
			_handMode = 3;
		}
		break;
	case 5: // the skull is set: on to the next
		if (handIdle()) {
			_handMode = 1;
			_handStep++;
		}
		break;
	default:
		break;
	}

	// --- the solve / exit check (0x42E280) ---
	if (_handMode == 6) {
		// The demo is done: back to the magic mirror room (movie 31).
		_engine.requestRoom(kMirrorRoom, 31);
		_handMode = 0;
		return;
	}
	if (_handMode != 0 || !_recheck)
		return;
	// A skull was slid: once all four have settled, read their faces and, if the
	// four match SkullCombo0-3, the puzzle is solved.
	for (int i = 0; i < 4; i++)
		if (_skull[i] < 0 || room.getMode(_skull[i]) != 0)
			return;
	_recheck = false;
	for (int i = 0; i < 4; i++) {
		const Common::String st = script.getClickableState(_skull[i]);
		_state[i] = st.empty() ? 0 : (st.lastChar() - 'A');
	}
	bool solved = true;
	for (int i = 0; i < 4; i++)
		solved = solved && _state[i] == _engine.getGameInt(Common::String::format("SkullCombo%d", i));
	if (solved) {
		_engine.requestRoom(kBedroom, 6);
		_engine.setGameFlag("SkullPuzzle", true);
		debugC(1, kDebugScript, "wizard's skull: solved -> the wizard's bedroom");
	}
}

void WizardsSkullRoom::saveKeys(Common::INIFile &profile) {
	// 0x42E120, after the base keys: the four rotations and IntroDone.
	for (int i = 0; i < 4; i++)
		profile.setKey(Common::String::format("Skull%d", i + 1), kSkull, Common::String::format("%d", _state[i]));
	profile.setKey("IntroDone", kSkull, _introDone ? "Yes" : "No");
}

void WizardsSkullRoom::loadKeys(const Common::INIFile &profile) {
	// 0x42E1D0 -> 0x40C560/0x40C670: a missing key reads as its reset default.
	Common::String v;
	for (int i = 0; i < 4; i++)
		_state[i] = profile.getKey(Common::String::format("Skull%d", i + 1), kSkull, v) ? atoi(v.c_str()) : 0;
	_introDone = profile.getKey("IntroDone", kSkull, v) && v == "Yes";
}

// ---------------------------------------------------------------------------
// The wizard's library

const char *WizardsLibraryRoom::kRevealVoice = "Wizards_Library.300C_";
const char *WizardsLibraryRoom::kRevealBg = "BIG_HEAD_BG_SKULLS";
const char *WizardsLibraryRoom::kTaunt[2] = { "Wizards_Library.300C1_", "Wizards_Library.300C2_" };
const char *WizardsLibraryRoom::kShelfHint[3] = { "Wizards_Library.300F_", "Wizards_Library.300F1_", "Wizards_Library.300F2_" };
const char *WizardsLibraryRoom::kHotspot1 = "Wizards_Library.HOTSPOT1_";
const char *WizardsLibraryRoom::kHotspot3 = "Wizards_Library.HOTSPOT3_";
const char *WizardsLibraryRoom::kSkullRoom = "ROOM_Wizards_Skull";

void WizardsLibraryRoom::reset() {
	// 0x42D250, after the base reset: the shelf-hint cycle.
	_hint = 0;
}

void WizardsLibraryRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42D600: before the puzzle is solved the shelf is a clickable and the way
	// to the wizard's bedroom is suppressed; once solved the shelf gives way to a
	// settled underlay sprite and the way on opens (the default path builds it).
	const bool solved = _engine.getGameFlag("SkullPuzzle");
	if (object == kUnderlay) {
		claim.build = solved ? Room::Claim::kSprite : Room::Claim::kSkip;
		claim.depth = -1001;
	} else if (object == kShelf) {
		claim.build = solved ? Room::Claim::kSkip : Room::Claim::kClickable;
	} else if (object == kToP24) {
		if (!solved)
			claim.build = Room::Claim::kSkip;
		// else the default path builds the exit to the wizard's bedroom.
	}
}

void WizardsLibraryRoom::enter(const Common::String &from) {
	// 0x42D580, after the base entry: back from the skull room with the puzzle
	// attempted but not solved, the wizard taunts (300C1_ or 300C2_, at random).
	if (from == kSkullRoom && !_engine.getGameFlag("SkullPuzzle") && _engine.getGameFlag("SkullPuzzleAttempted")) {
		const int which = _engine.getRandomNumber(1);
		_engine.getScript()->startScene(kTaunt[which], true);
		debugC(1, kDebugScript, "wizard's library: taunt %d", which);
	}
}

void WizardsLibraryRoom::revealOrEnterSkull(int movie) {
	// The first time (SkullMovie clear) the wizard's big head reveals the skull
	// combination (300C_ on BIG_HEAD_BG_SKULLS, returning to the library); after
	// that, straight to the skull room. @p movie is -1 for a plain goto.
	if (!_engine.getGameFlag("SkullMovie"))
		_engine.startBigHead(kRevealVoice, kRevealBg, kLibrary, movie);
	else
		_engine.requestRoom(kSkullRoom, movie);
}

void WizardsLibraryRoom::event(const Common::String &object, bool success) {
	// 0x42D270: the spell book used on the shelf reveals the combination (Big
	// Head the first time, else the skull room) and is spent; a plain click on
	// the shelf with the spell book already used routes there too, otherwise it
	// cycles the shelf's hint lines. The two hotspots give their lines.
	RoomScript &script = *_engine.getScript();
	if (object == kShelf) {
		if (success) {
			// The spell book used: reveal through movie 61, and spend the book.
			revealOrEnterSkull(61);
			_engine.getInventory()->removeItem(kSpellBook);
			debugC(1, kDebugScript, "wizard's library: the spell book reveals the combo");
			return;
		}
		if (used(_engine, kSpellBook)) {
			// Already used: route again, with no movie.
			revealOrEnterSkull(-1);
			return;
		}
		// No spell book yet: cycle the shelf's hint lines.
		script.startScene(kShelfHint[_hint], true);
		_hint = _hint == 2 ? 0 : _hint + 1;
		return;
	}
	if (object == kLibHotspot1)
		script.startScene(kHotspot1, true);
	else if (object == kLibHotspot3)
		script.startScene(kHotspot3, true);
}

// ---------------------------------------------------------------------------

RoomClass *createWizardsRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kSkull)
		return new WizardsSkullRoom(engine);
	if (room == kLibrary)
		return new WizardsLibraryRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
