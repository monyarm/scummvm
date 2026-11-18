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

// Phantom of the Knight's royal gallery and magic mirror room. The
// addresses are Phantom's.

#include "common/debug.h"

#include "scooby/character.h"
#include "scooby/gallery_rooms.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kGallery = "ROOM_P07_Royal_Gallery";
static const char *const kMirrorRoom = "ROOM_P08_Magic_Mirror_Room";
static const char *const kDiningRoom = "ROOM_P12_Dining_Room";
static const char *const kMirror = "OBJ_P07_ROYAL_GALLERY_MIRROR";
static const char *const kToP08 = "OBJ_P07_TO_P08";
static const char *const kInvMirror = "OBJ_INV_MIRROR";
static const char *const kBallRed = "OBJ_INV_BALL_RED";

static bool inSlot(ScoobyEngine &engine, const char *item) {
	// 0x4070C0: the item is in one of the toolbar's slots.
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (engine.getInventory()->getSlot(i) == item)
			return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// The royal gallery

bool RoyalGalleryRoom::isMirrorPlaced(ScoobyEngine &engine) {
	// 0x416A3C: the mirror was found and used, so it is no longer in a slot.
	return engine.getInventory()->hasItem(kInvMirror) && !inSlot(engine, kInvMirror);
}

int RoyalGalleryRoom::findSprite(const Common::String &object) const {
	const Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void RoyalGalleryRoom::reset() {
	// 0x4162D0, after the base reset: the 210A_ line back to the first.
	_cycle = 0;
}

void RoyalGalleryRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x4168D0: the hotspot is a clickable at depth -200; the mirror a
	// requires object answering the found mirror (ANIM_CURSOROPEN), hung or
	// bare; the way to the magic mirror room an exit at depth -1000, hidden
	// until the mirror is hung.
	if (object == "OBJ_P07_ROYAL_GALLERY_HOTSPOT1") {
		claim.build = Room::Claim::kClickable;
		claim.depth = -200;
	} else if (object == kToP08) {
		claim.build = Room::Claim::kExit;
		claim.depth = -1000;
		claim.hidden = !isMirrorPlaced(_engine);
	}
	// The mirror (OBJ_P07_ROYAL_GALLERY_MIRROR) is already a requires object
	// in object.ini, answering the found mirror (requiredanim=
	// ANIM_CURSORHAND_MIRROR); the default path builds it, and enter() sets
	// its animation and rollover. OBJ_INV_BALL_RED is likewise left to the
	// default path and the engine's own inventory handling.
}

void RoyalGalleryRoom::enter(const Common::String &from) {
	// 0x416830, after the base entry: from the dining room Velma walks in
	// (U); from the magic mirror room Shaggy (UR). The level's snacks plus
	// two are needed for the way on (+0x24); nothing is being hung; the
	// Jester's entry.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	if (from == kDiningRoom) {
		if (Character *velma = script.getCharacter("VELMA"))
			velma->walkIn(room, sound, "U");
	} else if (from == kMirrorRoom) {
		if (Character *shaggy = script.getCharacter("SHAGGY"))
			shaggy->walkIn(room, sound, "UR");
	}
	script.setSnacksNeeded(_engine.getDifficulty() + 2);
	_mirrorUsed = false;
	_placing = false;
	_flashPending = false;

	// The mirror sprite, hung and looping (Royal_Gallery.Mirror) or bare and
	// stopped; it rolls over to the open hand. The way on is noted for the
	// hanging to come.
	const bool placed = isMirrorPlaced(_engine);
	_mirror = findSprite(kMirror);
	_exit = findSprite(kToP08);
	if (_mirror >= 0) {
		room.setRollover(_mirror, "ANIM_CURSOROPEN");
		if (placed) {
			room.setPlayer(_mirror, 1, -1, 3);
			sound.play("Royal_Gallery.Mirror");
		} else {
			room.setPlayer(_mirror, 0, 0, 3);
		}
	}

	_jester.enter(_engine);
	debugC(1, kDebugScript, "royal gallery: from %s, the mirror %s", from.c_str(), placed ? "hung" : "bare");
}

bool RoyalGalleryRoom::click(int sprite) {
	_jester.click(sprite);
	return false;
}

void RoyalGalleryRoom::event(const Common::String &object, bool success) {
	// 0x416500: the red ball shown goes to the Jester, who is found
	// (RedBallFound), and the gallery is re-entered through movie 3; the
	// found mirror used on the bare mirror is taken from the toolbar and
	// hung (Shaggy puts it up on the next tick); the bare mirror clicked
	// cycles its 210A_ lines.
	RoomScript &script = *_engine.getScript();
	if (object == kBallRed) {
		_engine.requestRoom(kGallery, 3);
		_engine.setGameFlag("RedBallFound", true);
		debugC(1, kDebugScript, "royal gallery: the red ball found");
		return;
	}
	if (object != kMirror)
		return;
	if (success) {
		// The found mirror used on it: the mirror leaves the toolbar, Shaggy
		// is set to hang it. The original also writes the room's own name to
		// the game's pending-opening string (+0x8E), which names no clip.
		_engine.getInventory()->removeItem(kInvMirror);
		_mirrorUsed = true;
		debugC(1, kDebugScript, "royal gallery: the mirror used, Shaggy to hang it");
		return;
	}
	static const char *const kLines[] = { "Royal_Gallery.210A_", "Royal_Gallery.210A1_", "Royal_Gallery.210A2_" };
	script.startScene(kLines[_cycle], true);
	_cycle = _cycle == 2 ? 0 : _cycle + 1;
	if (_mirror >= 0)
		_engine.getRoom()->setPlayer(_mirror, 3, -1, 3);
}

bool RoyalGalleryRoom::leaveBy(const Common::String &exitObject) {
	// 0x4162F0: the way to the magic mirror room with fewer snacks given
	// (+0x88) than the level plus two blocks, a line by level
	// (Royal_Gallery.210C_, 210C1_, 210C2_) and the snack box flashes; else
	// the base handler lets it through.
	if (exitObject.empty())
		return false;
	const RoomScript::Exit exit = _engine.getScript()->readExit(exitObject);
	if (exit.destination != kMirrorRoom)
		return false;
	const int level = _engine.getDifficulty();
	if (_engine.getScript()->getMemory(kGallery).snacksGiven >= (uint)(level + 2))
		return false;
	static const char *const kLines[] = { "Royal_Gallery.210C_", "Royal_Gallery.210C1_", "Royal_Gallery.210C2_" };
	_engine.getScript()->startScene(level == 0 ? kLines[0] : (level == 1 ? kLines[1] : kLines[2]), true);
	_engine.getInventory()->flashSnackBox();
	debugC(1, kDebugScript, "royal gallery: the way on wants %d snacks (%u given)", level + 2, _engine.getScript()->getMemory(kGallery).snacksGiven);
	return true;
}

void RoyalGalleryRoom::tick() {
	// 0x416610, after the base tick: the Jester; then the mirror hung. Once
	// the found mirror has been used, Shaggy, idle with no scene playing and
	// the mirror sprite at rest, walks over and puts it up
	// (OBJ_SHAGGY_P07_PUT_MIRROR, Royal_Gallery.Shaggy_Puts_Mirror); input
	// is held meanwhile. Shaggy idle again: the mirror shows, looping
	// (Royal_Gallery.Mirror), the way on opens, and a line by level
	// (210B_, 210B2_, 210B3_) plays, blocking, then the snack box flashes.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	_jester.tick(_engine);

	if (_mirrorUsed) {
		Character *shaggy = script.getCharacter("SHAGGY");
		const bool idle = shaggy && shaggy->getState() == Character::kStateIdle;
		const bool atRest = _mirror >= 0 && room.getMode(_mirror) == 0 && room.getStep(_mirror) == 0;
		if (!_placing && idle && !script.isScenePlaying() && script.isOpeningDone() && atRest) {
			_placing = true;
			shaggy->special(room, sound, "OBJ_SHAGGY_P07_PUT_MIRROR");
			sound.play("Royal_Gallery.Shaggy_Puts_Mirror");
			debugC(1, kDebugScript, "royal gallery: Shaggy hangs the mirror");
		}
		if (_placing && idle) {
			_mirrorUsed = false;
			_placing = false;
			if (_mirror >= 0)
				room.setPlayer(_mirror, 1, -1, 3);
			if (_exit >= 0)
				room.setHidden(_exit, false);
			sound.play("Royal_Gallery.Mirror");
			const int level = _engine.getDifficulty();
			static const char *const kLines[] = { "Royal_Gallery.210B_", "Royal_Gallery.210B2_", "Royal_Gallery.210B3_" };
			script.startScene(level == 0 ? kLines[0] : (level == 1 ? kLines[1] : kLines[2]), true);
			_flashPending = true;
			debugC(1, kDebugScript, "royal gallery: the mirror hung");
		}
	}

	if (_flashPending) {
		_engine.getInventory()->flashSnackBox();
		_flashPending = false;
	}

	// The hung mirror's shimmer: stopped off its first frame, back to it;
	// looped round to it, play on from the first frame and hum again.
	if (_mirror >= 0) {
		if (room.getMode(_mirror) == 0 && room.getStep(_mirror) != 0) {
			room.setPlayer(_mirror, 0, 0, 3);
			return;
		}
		if (room.getMode(_mirror) == 1 && room.getStep(_mirror) == 0) {
			room.setPlayer(_mirror, 1, 1, 3);
			sound.play("Royal_Gallery.Mirror");
		}
	}
}

// ---------------------------------------------------------------------------
// The magic mirror room

void MagicMirrorRoom::reset() {
	// 0x414F20, after the base reset: both hotspot cycles back to the first.
	_cam = 0;
	_toggle = 0;
}

void MagicMirrorRoom::enter(const Common::String &from) {
	// 0x415250, after the base entry: the mirror hums.
	_engine.getSound()->play("Magic_Mirror_Room.Mirror");
	debugC(1, kDebugScript, "magic mirror room: from %s", from.c_str());
}

void MagicMirrorRoom::event(const Common::String &object, bool success) {
	// 0x414F40: HOTSPOT2 toggles 310X1_ / 310X2_; HOTSPOT3 gives HOTSPOT3_,
	// blocking; HOTSPOT4 cycles 310X2_ / 310XA2_ / 310XA3_. None but
	// HOTSPOT3 holds input.
	RoomScript &script = *_engine.getScript();
	if (object == "OBJ_P08_MAGIC_MIRROR_ROOM_HOTSPOT2") {
		script.startScene(_toggle == 0 ? "Magic_Mirror_Room.310X1_" : "Magic_Mirror_Room.310X2_", false);
		_toggle = _toggle == 0 ? 1 : 0;
	} else if (object == "OBJ_P08_MAGIC_MIRROR_ROOM_HOTSPOT3") {
		script.startScene("Magic_Mirror_Room.HOTSPOT3_", true);
	} else if (object == "OBJ_P08_MAGIC_MIRROR_ROOM_HOTSPOT4") {
		static const char *const kLines[] = { "Magic_Mirror_Room.310X2_", "Magic_Mirror_Room.310XA2_", "Magic_Mirror_Room.310XA3_" };
		script.startScene(kLines[_cam], false);
		_cam = _cam == 2 ? 0 : _cam + 1;
	}
}

void MagicMirrorRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x415060: the fifth hotspot a looping sprite; the way to the wizard's
	// skull a looping sprite at depth -500; the four cameras exits at
	// depth 1.
	if (object == "OBJ_P08_MAGIC_MIRROR_ROOM_HOTSPOT5") {
		claim.build = Room::Claim::kSprite;
		claim.depth = 0;
	} else if (object == "OBJ_P08_TO_WIZARDS_SKULL") {
		claim.build = Room::Claim::kSprite;
		claim.depth = -500;
	} else if (object == "OBJ_P08_MAGIC_MIRROR_CAM1" || object == "OBJ_P08_MAGIC_MIRROR_CAM2" ||
	           object == "OBJ_P08_MAGIC_MIRROR_CAM3" || object == "OBJ_P08_MAGIC_MIRROR_CAM4") {
		claim.build = Room::Claim::kExit;
		claim.depth = 1;
	}
}

// ---------------------------------------------------------------------------

RoomClass *createGalleryRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kGallery)
		return new RoyalGalleryRoom(engine);
	if (room == kMirrorRoom)
		return new MagicMirrorRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
