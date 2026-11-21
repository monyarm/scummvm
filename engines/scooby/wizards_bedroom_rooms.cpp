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

// Phantom of the Knight's wizard's bedroom and veranda (WP-081), the third of
// the audit's group-8 (Upstairs) rooms. The addresses are Phantom's. The last
// group-8 room, ROOM_P30_WFR (the Wizard's Rule Room), is its own packet (WP-082).

#include "common/debug.h"

#include "scooby/character.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"
#include "scooby/wizards_bedroom_rooms.h"

namespace Scooby {

static const char *const kBedroom = "ROOM_P24_Wizards_Bedroom";
static const char *const kVeranda = "ROOM_P28_Veranda";

static const char *const kWizard = "WIZARD";
static const char *const kMazeMap = "OBJ_INV_MAZE_MAP";
static const char *const kWFR = "ROOM_P30_WFR";
static const char *const kBigHead = "ROOM_Big_Head";

// ---------------------------------------------------------------------------
// The wizard's bedroom

void WizardsBedroomRoom::reset() {
	// 0x42CD50, after the base reset: nothing armed.
	_armed = false;
}

void WizardsBedroomRoom::enter(const Common::String &from) {
	// 0x42CD70, after the base entry. The wizard is met. Up from the Wizard's
	// Rule Room with his clue not yet given, his clue is given, he ACTs GIVES and
	// the accusation is armed (the tick fires it). The maze-map scroll shows only
	// while his clue is given and the gang hasn't the map, with Wizards_Bedroom.
	// Scroll; otherwise it is hidden. (The original also parks the cursor and sets
	// a game interaction flag here; the fork's scene and cursor handling cover it.)
	_armed = false;
	_mazeMap = -1;
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == kMazeMap)
			_mazeMap = s;
	}

	_engine.getInventory()->meetSuspect(kWizard);

	if (!_engine.isClueOwner(kWizard) && from == kWFR) {
		_engine.giveClue(kWizard);
		if (Character *wizard = script.getCharacter(kWizard))
			wizard->act(room, sound, "GIVES");
		_armed = true;
	}

	if (!_engine.getInventory()->hasItem(kMazeMap) && _engine.isClueOwner(kWizard)) {
		script.startScene("Wizards_Bedroom.Scroll", true);
		debugC(1, kDebugScript, "wizard's bedroom: the scroll, from %s", from.c_str());
		return;
	}
	if (_mazeMap >= 0)
		room.setHidden(_mazeMap, true);
	debugC(1, kDebugScript, "wizard's bedroom: from %s%s", from.c_str(), _armed ? "; the wizard gives" : "");
}

void WizardsBedroomRoom::tick() {
	// 0x42CF70, after the base tick. (The original also runs the base click of
	// the wizard with the plain arrow, which the fork's engine already does.) The
	// accusation armed: once the wizard is idle, the exit handler takes the gang
	// to ROOM_Suspect_Chooser with Global.Thank_You_ (as the dining room and the
	// arenas do).
	if (!_armed)
		return;
	RoomScript &script = *_engine.getScript();
	Character *wizard = script.getCharacter(kWizard);
	if (wizard && wizard->getState() == Character::kStateIdle) {
		_armed = false;
		RoomScript::Exit exit;
		exit.destination = "ROOM_Suspect_Chooser";
		exit.scene = "Global.Thank_You_";
		exit.movie = -1;
		script.takeExit(exit);
		debugC(1, kDebugScript, "wizard's bedroom: thanks, then the suspects and clues");
	}
}

void WizardsBedroomRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42D140: the maze-map scroll is an inventory placement (0x408BD0) the enter
	// shows or hides.
	if (object == kMazeMap)
		claim.build = Room::Claim::kInventory;
}

// ---------------------------------------------------------------------------
// The veranda

void VerandaRoom::reset() {
	// 0x4185C0, after the base reset: no entries, no well clicks.
	_entrances = 0;
	_wellClicks = 0;
}

void VerandaRoom::arrive(const Common::String &from) {
	// 0x4185E0 (+0x2C): not back from Big Head, the first ever entry opens with
	// Veranda.270D_ (Entrances); before the first well click, Veranda.290_.
	if (from == kBigHead)
		return;
	RoomScript &script = *_engine.getScript();
	if (_entrances == 0) {
		_entrances = 1;
		script.setOpening("Veranda.270D_");
	} else if (_wellClicks == 0) {
		script.setOpening("Veranda.290_");
	}
}

void VerandaRoom::event(const Common::String &object, bool success) {
	// 0x418410: the well hotspot. WellClicks cycles 1..3 (wrapping), each click a
	// line (Veranda.290B_ / 290X1_ / 290X2_) the gang says over Big Head
	// (BIG_HEAD_BG_P44P22), back here.
	if (object != "OBJ_P28_VERANDA_HOTSPOT1")
		return;
	if (_wellClicks == 0)
		_wellClicks = 1;
	const int which = _wellClicks;
	_wellClicks = which + 1;
	Common::String line;
	if (which == 1) {
		line = "Veranda.290B_";
	} else if (which == 2) {
		line = "Veranda.290X1_";
	} else { // which == 3
		_wellClicks = 1;
		line = "Veranda.290X2_";
	}
	_engine.startBigHead(line, "BIG_HEAD_BG_P44P22", kVeranda);
	debugC(1, kDebugScript, "veranda: the well, %s", line.c_str());
}

void VerandaRoom::saveKeys(Common::INIFile &profile) {
	// 0x418650: Entrances and WellClicks, in this room's section.
	profile.setKey("Entrances", kVeranda, Common::String::format("%d", _entrances));
	profile.setKey("WellClicks", kVeranda, Common::String::format("%d", _wellClicks));
}

void VerandaRoom::loadKeys(const Common::INIFile &profile) {
	// 0x4186B0: a missing key reads as 0.
	Common::String v;
	_entrances = profile.getKey("Entrances", kVeranda, v) ? atoi(v.c_str()) : 0;
	_wellClicks = profile.getKey("WellClicks", kVeranda, v) ? atoi(v.c_str()) : 0;
}

// ---------------------------------------------------------------------------

RoomClass *createWizardsBedroomRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kBedroom)
		return new WizardsBedroomRoom(engine);
	if (room == kVeranda)
		return new VerandaRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
