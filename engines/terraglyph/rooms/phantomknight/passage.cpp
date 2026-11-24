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

// Phantom of the Knight's secret passages, storage room and treasury. The
// addresses are Phantom's.

#include "common/debug.h"
#include "common/formats/ini-file.h"

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/rooms/phantomknight/passage.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kPassage1 = "ROOM_P15_Secret_Passage1";
static const char *const kPassage2 = "ROOM_P19_Secret_Passage2";
static const char *const kStorage = "ROOM_P18_Storage";
static const char *const kTreasury = "ROOM_P16_Treasury";
static const char *const kFlame = "OBJ_P19_SECRET_PASSAGE_GAR_FLAME";
static const char *const kRope = "OBJ_INV_ROPE";

// ---------------------------------------------------------------------------
// The first secret passage

void SecretPassage1Room::enter(const Common::String &from) {
	// 0x416CA0, after the base entry: from the armory's slide Velma walks in
	// (D, 0x406020), from the storage room Daphne (UL, 0x405EE0).
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	if (from == "ROOM_P20_Kings_Armory_Slide") {
		if (Character *velma = script.getCharacter("VELMA"))
			velma->walkIn(room, sound, "D");
	} else if (from == kStorage) {
		if (Character *daphne = script.getCharacter("DAPHNE"))
			daphne->walkIn(room, sound, "UL");
	}
}

// ---------------------------------------------------------------------------
// The second secret passage

void SecretPassage2Room::reset() {
	// 0x416D70: the wall of fire burning.
	_flame = true;
}

void SecretPassage2Room::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x416D90: the wall of fire a requires object (0x408C80 with -1; its
	// requiredanim= is a cursor the game never shows, so only the arrow's
	// event reaches it), burning (FLAMEA, with Secret_2.Wall_Of_Fire) or
	// out (FLAMEB).
	if (object != kFlame)
		return;
	claim.build = Room::Claim::kRequires;
	claim.layer = 0;
	claim.depth = 0;
	if (_flame) {
		claim.anim = "ANIM_P19_SECRET_PASSAGE_GAR_FLAMEA";
		_engine.getSound()->play("Secret_2.Wall_Of_Fire");
	} else {
		claim.anim = "ANIM_P19_SECRET_PASSAGE_GAR_FLAMEB";
	}
}

void SecretPassage2Room::enter(const Common::String &from) {
	// 0x416F30, after the base entry: the level's snacks plus two needed
	// here (+0x24).
	_engine.getScript()->setSnacksNeeded(_engine.getDifficulty() + 2);
	debugC(1, kDebugScript, "second secret passage: from %s, the wall of fire %s", from.c_str(), _flame ? "burning" : "out");
}

void SecretPassage2Room::event(const Common::String &object, bool success) {
	// 0x416E90, the wall of fire clicked: fewer snacks given here (+0x88)
	// than the level plus two, a line by level (Treasury.360A_, 360A1_,
	// 360A2_), blocking, and the snack box flashes (0x40D4D0); else the
	// fire out and movie 39 back to this room (game vtable +0x08).
	if (object != kFlame)
		return;
	RoomScript &script = *_engine.getScript();
	const int level = _engine.getDifficulty();
	if (script.getMemory(kPassage2).snacksGiven < (uint)(level + 2)) {
		static const char *const kLines[] = { "Treasury.360A_", "Treasury.360A1_", "Treasury.360A2_" };
		script.startScene(level == 0 ? kLines[0] : (level == 1 ? kLines[1] : kLines[2]), true);
		_engine.getInventory()->flashSnackBox();
		debugC(1, kDebugScript, "second secret passage: the fire wants %d snacks (%u given)", level + 2, script.getMemory(kPassage2).snacksGiven);
		return;
	}
	_flame = false;
	_engine.requestRoom(kPassage2, 39);
	debugC(1, kDebugScript, "second secret passage: the fire out");
}

void SecretPassage2Room::saveKeys(Common::INIFile &profile) {
	// 0x416F60 -> 0x40C530: GargoyleFlameActive, in this room's section.
	profile.setKey("GargoyleFlameActive", kPassage2, _flame ? "Yes" : "No");
}

void SecretPassage2Room::loadKeys(const Common::INIFile &profile) {
	// 0x416FA0 -> 0x40C560: only Yes reads as true; no key leaves it.
	Common::String v;
	if (profile.getKey("GargoyleFlameActive", kPassage2, v))
		_flame = v == "Yes";
}

// ---------------------------------------------------------------------------
// The storage room

void StorageRoomP18::reset() {
	// 0x4176B0: the hotspots' lines from the first.
	_hotspotLine = 0;
}

void StorageRoomP18::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x4179C0: the rope, never had, a requires object (0x408C80 with -1)
	// the arrow answers (+0xC0); had, not built.
	if (object != kRope)
		return;
	if (_engine.getInventory()->hasItem(kRope)) {
		claim.build = Room::Claim::kSkip;
		return;
	}
	claim.build = Room::Claim::kRequires;
	claim.layer = 0;
	claim.depth = 0;
}

void StorageRoomP18::enter(const Common::String &from) {
	// 0x417950, after the base entry.
	_take = _taking = false;
	_doneFlag = nullptr;
	_doneScene.clear();
	_rope = -1;
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == kRope)
			_rope = s;
	}
	debugC(1, kDebugScript, "storage room: from %s", from.c_str());
}

void StorageRoomP18::roomLeft() {
	// 0x417970, after the base leave: left with the rope to take, it goes
	// in the toolbar all the same.
	Inventory &inventory = *_engine.getInventory();
	if (_take && !inventory.hasItem(kRope))
		inventory.addItem(kRope, "ANIM_TOOLBARROPE", "ANIM_CURSORROPE");
}

void StorageRoomP18::startScene(const Common::String &scene, bool block, bool *done) {
	// 0x40B400 on the room, with @p done set as the scene ends.
	if (_engine.getScript()->startScene(scene, block) && done) {
		_doneFlag = done;
		_doneScene = scene;
	}
}

void StorageRoomP18::event(const Common::String &object, bool success) {
	// 0x4176D0. Either hotspot: Storage_Room.370X_, 370X1_, 370X2_ in turn
	// (+0xB8), not blocking. The rope: before the well is visited
	// (WellVisited, game +0x262) Storage_Room.370B_, after it 370A_,
	// blocking; as it ends, Velma is to take the rope (+0xBC).
	if (object == "OBJ_P18_STORAGE_HOTSPOT1" || object == "OBJ_P18_STORAGE_HOTSPOT2") {
		static const char *const kLines[] = { "Storage_Room.370X_", "Storage_Room.370X1_", "Storage_Room.370X2_" };
		const int line = _hotspotLine++;
		if (line >= 0 && line < 3)
			startScene(kLines[line], false, nullptr);
		if (line == 2)
			_hotspotLine = 0;
		return;
	}
	if (object == kRope)
		startScene(_engine.getGameFlag("WellVisited") ? "Storage_Room.370A_" : "Storage_Room.370B_", true, &_take);
}

void StorageRoomP18::tick() {
	// 0x4177C0, after the base tick. Velma to take the rope:
	// - idle, with no scene playing and the opening done: she takes it
	//   (OBJ_VELMA_P18_TAKE_ROPE, with Storage.Velma_Takes_Rope), and it is
	//   hidden; 0x437540 restarts the tick clock, which the fork doesn't
	//   keep;
	// - the cursor out and input held throughout (0x435E80, game +0x24);
	// - on her step 34 (+0x4C), the rope in the toolbar unless had;
	// - idle again: done, the cursor back and input free.
	if (_doneFlag && _engine.getScript()->getScene() != _doneScene) {
		*_doneFlag = true;
		_doneFlag = nullptr;
		_doneScene.clear();
	}
	if (!_take)
		return;
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	Character *velma = script.getCharacter("VELMA");
	if (!_taking && velma && velma->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
		_taking = true;
		velma->special(room, sound, "OBJ_VELMA_P18_TAKE_ROPE");
		sound.play("Storage.Velma_Takes_Rope");
		if (_rope >= 0)
			room.setHidden(_rope, true);
		debugC(1, kDebugScript, "storage room: Velma takes the rope");
	}
	if (_taking && velma && velma->getSprite() >= 0 && room.getStep(velma->getSprite()) == 0x22 && !inventory.hasItem(kRope)) {
		inventory.addItem(kRope, "ANIM_TOOLBARROPE", "ANIM_CURSORROPE");
		debugC(1, kDebugScript, "storage room: the rope in the toolbar");
	}
	if (_taking && velma && velma->getState() == Character::kStateIdle) {
		_take = false;
		_taking = false;
		debugC(1, kDebugScript, "storage room: Velma has the rope");
	}
}

// ---------------------------------------------------------------------------
// The treasury

bool TreasuryRoom::click(int sprite) {
	_clicked = sprite;
	return false;
}

void TreasuryRoom::tick() {
	// 0x418340, after the base tick: the Princess clicked with the arrow
	// (game +0x20 bit 4, her +0x08): the final clue (FinalClue, game
	// +0x108) and movie 55 to ROOM_Suspect_Chooser (game vtable +0x08).
	const int clicked = _clicked;
	_clicked = -1;
	Character *princess = _engine.getScript()->getCharacter("PRINCESS");
	if (princess && clicked >= 0 && clicked == princess->getSprite() && _engine.getInventory()->isDefaultCursor()) {
		_engine.setGameFlag("FinalClue", true);
		_engine.requestRoom("ROOM_Suspect_Chooser", 55);
		debugC(1, kDebugScript, "treasury: the Princess, the final clue");
	}
}

RoomClass *createPassageRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kPassage1)
		return new SecretPassage1Room(engine);
	if (room == kPassage2)
		return new SecretPassage2Room(engine);
	if (room == kStorage)
		return new StorageRoomP18(engine);
	if (room == kTreasury)
		return new TreasuryRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
