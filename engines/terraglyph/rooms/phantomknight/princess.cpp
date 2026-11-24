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

// Phantom of the Knight's Princess's bedroom and Mermaid's cavern (WP-078), the
// second of the audit's group-8 (Upstairs) rooms. The addresses are Phantom's.

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/rooms/phantomknight/princess.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kBedroom = "ROOM_P29_Princess_Bedroom";
static const char *const kCavern = "ROOM_P21_Mermaids_Cavern";

static const char *const kSpellBook = "OBJ_INV_SPELL_BOOK";
static const char *const kHotspot1 = "OBJ_P29_PRINCESS_BEDROOM_HOTSPOT1";

// ---------------------------------------------------------------------------
// The Princess's bedroom

const char *PrincessBedroomRoom::kHotspotLine = "Princess_Bedroom.280XA_";
const char *PrincessBedroomRoom::kBookGot = "Princess_Bedroom.280A_";
const char *PrincessBedroomRoom::kTakeScene = "Princess_Bedroom.Daphne_Takes_Book";
const char *PrincessBedroomRoom::kTakeAnim = "OBJ_DAPHNE_P24_TAKE_BOOK";

void PrincessBedroomRoom::enter(const Common::String &from) {
	// 0x415700, after the base entry: nothing pending, the spell book not yet
	// being taken. Find its placement so the take can hide it.
	_takePending = false;
	_taking = false;
	_spellBook = -1;
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == kSpellBook)
			_spellBook = s;
	}
	debugC(1, kDebugScript, "princess bedroom: from %s", from.c_str());
}

void PrincessBedroomRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x4155E0: the one hotspot is a clickable at depth 300; the spell book is a
	// requires object the arrow answers, there only until it is taken.
	if (object == kHotspot1) {
		claim.build = Room::Claim::kClickable;
		claim.depth = 300;
	} else if (object == kSpellBook) {
		claim.build = _engine.getInventory()->hasItem(kSpellBook) ? Room::Claim::kSkip : Room::Claim::kRequires;
	}
}

void PrincessBedroomRoom::event(const Common::String &object, bool success) {
	// 0x415570: the spell book clicked with the plain arrow (the original guards
	// on the cursor being the settled/default one) arms the take the tick runs;
	// the hotspot gives a line.
	if (object == kSpellBook) {
		if (_engine.getInventory()->isDefaultCursor())
			_takePending = true;
		return;
	}
	if (object == kHotspot1)
		_engine.getScript()->startScene(kHotspotLine, true);
}

void PrincessBedroomRoom::tick() {
	// 0x4153B0, after the base tick. The spell book armed holds input (game
	// +0x24, through isBusy()): Daphne idle, no scene playing and the opening
	// done, she takes it (OBJ_DAPHNE_P24_TAKE_BOOK, state 0xD, with
	// Princess_Bedroom.Daphne_Takes_Book), and it is hidden; idle again, it goes
	// in the toolbar unless ever had, and Princess_Bedroom.280A_, blocking.
	// 0x437540 restarts the tick clock, which the fork doesn't keep.
	if (!_takePending)
		return;
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Character *daphne = script.getCharacter("DAPHNE");
	if (!_taking && daphne && daphne->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
		_taking = true;
		daphne->special(room, sound, kTakeAnim);
		sound.play(kTakeScene);
		if (_spellBook >= 0)
			room.setHidden(_spellBook, true);
	}
	if (_taking && daphne && daphne->getState() == Character::kStateIdle) {
		_takePending = false;
		_taking = false;
		Inventory &inventory = *_engine.getInventory();
		if (!inventory.hasItem(kSpellBook))
			inventory.addItem(kSpellBook, "ANIM_TOOLBARSPELL_BOOK", "ANIM_CURSORSPELL_BOOK");
		script.startScene(kBookGot, true);
		debugC(1, kDebugScript, "princess bedroom: Daphne has the spell book");
	}
}

void PrincessBedroomRoom::roomLeft() {
	// 0x415520, after the base leave: left while the book is being taken, it goes
	// in the toolbar all the same.
	Inventory &inventory = *_engine.getInventory();
	if (_takePending && !inventory.hasItem(kSpellBook))
		inventory.addItem(kSpellBook, "ANIM_TOOLBARSPELL_BOOK", "ANIM_CURSORSPELL_BOOK");
}

// ---------------------------------------------------------------------------
// The Mermaid's cavern

const char *MermaidsCavernRoom::kHotspot = "OBJ_P21_MERMAID_HOTSPOT1";

void MermaidsCavernRoom::event(const Common::String &object, bool success) {
	// 0x4152E0: the mermaid hotspot frightens the gang — Daphne, Shaggy and
	// Scooby each play their SCARED reaction (state 0x11). No line.
	if (object != kHotspot)
		return;
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	static const char *const kGang[] = { "DAPHNE", "SHAGGY", "SCOOBY" };
	for (uint i = 0; i < ARRAYSIZE(kGang); i++) {
		Character *c = script.getCharacter(kGang[i]);
		if (c)
			c->setState(room, sound, Character::kStateScared);
	}
	debugC(1, kDebugScript, "mermaid's cavern: the gang is scared");
}

// ---------------------------------------------------------------------------

RoomClass *createPrincessRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kBedroom)
		return new PrincessBedroomRoom(engine);
	if (room == kCavern)
		return new MermaidsCavernRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
