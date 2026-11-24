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
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/rooms/showdown/jail.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kJail = "ROOM_P07_Jail";
static const char *const kTunnelDark = "ROOM_P14_Tunnel_Dark";
static const char *const kBlanket = "OBJ_INV_BLANKET";
static const char *const kBed = "OBJ_P07_JAIL_HOTSPOT2";
static const char *const kHotspot3 = "OBJ_P07_JAIL_HOTSPOT3";
static const char *const kHotspot4 = "OBJ_P07_JAIL_HOTSPOT4";
static const char *const kHole = "OBJ_P07_JAIL_OVERLAY_HOLE";
static const char *const kStool = "OBJ_P07_JAIL_OVERLAY_STOOL";
static const char *const kHoleScene = "Jail_Cell.150D_";
static const char *const kLeave = "OBJ_SHAGGY_P07_LEAVE_JAIL";

static int findSprite(const Room &room, const char *object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

static bool isIdle(const Character *c) {
	return c && c->getState() == Character::kStateIdle;
}

// 0x4070C0: whether the item is in one of the four slots now.
static bool inSlot(const Inventory &inventory, const char *item) {
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (inventory.getSlot(i) == item)
			return true;
	}
	return false;
}

static void giveBlanket(Inventory &inventory) {
	if (!inventory.hasItem(kBlanket))
		inventory.addItem(kBlanket, "ANIM_TOOLBARBLANKET", "ANIM_CURSORBLANKET");
}

void JailRoom::reset() {
	// 0x412EC0, after the base reset.
	_hotspot3 = 0;
	_leaving = false;
	_bedBroken = false;
	_holeDone = false;
}

void JailRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x413690. The blanket a requires object (0x408C80 with -1: layer 0)
	// at depth 1, hidden once taken; the hole and the stool sprites under
	// layer 0 at depths 0 and 500, hidden until the bed is broken. Hotspots
	// 2 and 4 are built as the default path builds a clickable
	// (0x408DC0 with 1, -1: depth priority=, -1 without one), so they are
	// left to it and found by name on entry.
	if (object == kBlanket) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.depth = 1;
		claim.hidden = _engine.getInventory()->hasItem(kBlanket);
	} else if (object == kHole || object == kStool) {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = object == kStool ? 500 : 0;
		claim.hidden = !_bedBroken;
	}
}

void JailRoom::enter(const Common::String &from) {
	// 0x413600, which comes before the base entry in the original: nothing
	// under way; a broken bed shows the hole and the stool instead. Once
	// the gang has dropped through the hole, the jail leads straight to
	// the dark tunnel.
	Room &room = *_engine.getRoom();
	_blanketClicked = false;
	_takingBlanket = false;
	_bedClicked = false;
	_breakingBed = false;
	_falling = false;
	_holeScene = false;
	_lastScene.clear();
	_lastSound.clear();
	_blanket = findSprite(room, kBlanket);
	_bed = findSprite(room, kBed);
	_hotspot4 = findSprite(room, kHotspot4);
	_hole = findSprite(room, kHole);
	_stool = findSprite(room, kStool);
	if (_bedBroken) {
		// 0x401770: no open hand over the bed.
		if (_bed >= 0) {
			room.setRollover(_bed, "");
			room.setHidden(_bed, true);
		}
		if (_hole >= 0)
			room.setHidden(_hole, false);
		if (_stool >= 0)
			room.setHidden(_stool, false);
	}
	if (_holeDone) {
		debugC(1, kDebugScript, "jail: the hole again, straight to %s", kTunnelDark);
		_engine.requestRoom(kTunnelDark);
	}
}

void JailRoom::roomLeft() {
	// 0x413310, after the base leave: what was under way is done.
	if (_blanketClicked)
		giveBlanket(*_engine.getInventory());
	if (_bedClicked)
		_bedBroken = true;
}

void JailRoom::playSound(const char *name) {
	// 0x434DC0 looks the name up as it is: the disc's names have dots.
	const bool found = _engine.getSound()->play(name);
	_lastSound = Common::String::format("%s found %d", name, found ? 1 : 0);
}

void JailRoom::event(const Common::String &object, bool success) {
	// 0x413470
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	if (object == kBlanket && _engine.getInventory()->isDefaultCursor()) {
		// The blanket with the arrow: Shaggy takes it; input held.
		_blanketClicked = true;
		return;
	}
	const char *scene = nullptr;
	if (object == kHotspot4) {
		// Moved on to its B: 150X3_.
		if (_hotspot4 >= 0 && script.getClickableState(_hotspot4) == "ANIM_P07_JAIL_HOTSPOT4B")
			scene = "Jail_Cell.150X3_";
	} else if (object == kBed) {
		// Moved on to its B: 150X1_. Back to A: Scooby breaks it. The bed
		// can't be clicked again and loops its B.
		if (_bed >= 0 && script.getClickableState(_bed) == "ANIM_P07_JAIL_HOTSPOT2B") {
			scene = "Jail_Cell.150X1_";
		} else if (_bed >= 0) {
			_bedClicked = true;
			room.setHitType(_bed, 0);
			room.startAnim(_bed, "ANIM_P07_JAIL_HOTSPOT2B", 1, -1);
			debugC(1, kDebugScript, "jail: the bed clicked to break at tick %u", script.getTick());
		}
	} else if (object == kHotspot3) {
		// 150X2_ and 150X23_ in turn.
		scene = _hotspot3 == 0 ? "Jail_Cell.150X2_" : "Jail_Cell.150X23_";
		_hotspot3 = _hotspot3 == 0 ? 1 : 0;
	}
	if (scene && script.startScene(scene, true))
		_lastScene = scene;
}

bool JailRoom::leaveBy(const Common::String &exitObject) {
	// 0x413370 (+0x24): never the default handler. With the blanket in a
	// slot, no scene and the opening done, Shaggy leaves; the tick takes
	// the gang to the dark tunnel when he is idle again. The jail has no
	// exit object, so this runs only if something else asks.
	RoomScript &script = *_engine.getScript();
	if (inSlot(*_engine.getInventory(), kBlanket) && !_leaving && !script.isScenePlaying() && script.isOpeningDone()) {
		_leaving = true;
		if (Character *shaggy = script.getCharacter("SHAGGY"))
			shaggy->special(*_engine.getRoom(), *_engine.getSound(), kLeave);
		debugC(1, kDebugScript, "jail: Shaggy leaves by %s at tick %u", exitObject.c_str(), script.getTick());
	}
	return true;
}

void JailRoom::tick() {
	// 0x412EF0, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Character *shaggy = script.getCharacter("SHAGGY");
	Character *scooby = script.getCharacter("SCOOBY");

	// 150D_'s done flag, set by the base tick as the scene ends.
	if (_holeScene && script.getScene() != kHoleScene) {
		_holeScene = false;
		_holeDone = true;
	}

	// Leaving by the exit handler: Shaggy idle, he is gone, and the room.
	if (_leaving && isIdle(shaggy)) {
		room.setHidden(shaggy->getSprite(), true);
		_engine.requestRoom(kTunnelDark);
		_leaving = false;
	}

	// The blanket: Shaggy takes it off the bunk; idle again, it is in the
	// toolbar, with 150C_.
	if (_blanketClicked) {
		if (!_takingBlanket && isIdle(shaggy)) {
			shaggy->special(room, sound, "OBJ_SHAGGY_P07_TAKE_BLANKET");
			if (_blanket >= 0)
				room.setHidden(_blanket, true);
			_takingBlanket = true;
		}
		if (_takingBlanket && isIdle(shaggy)) {
			_blanketClicked = false;
			_takingBlanket = false;
			if (_blanket >= 0)
				room.setHidden(_blanket, true);
			giveBlanket(*_engine.getInventory());
			if (script.startScene("Jail_Cell.150C_", true))
				_lastScene = "Jail_Cell.150C_";
		}
	}

	// The bed: Scooby breaks it; idle again, the bed is gone and the hole
	// and the stool are there.
	if (_bedClicked) {
		if (!_breakingBed && isIdle(scooby)) {
			scooby->special(room, sound, "OBJ_SCOOBY_P07_BREAK_BED");
			_breakingBed = true;
			playSound("Scooby_Breaks_Bed");
		}
		if (_breakingBed && isIdle(scooby)) {
			_bedClicked = false;
			_breakingBed = false;
			if (_bed >= 0) {
				room.setRollover(_bed, "");
				room.setHidden(_bed, true);
			}
			if (_hole >= 0)
				room.setHidden(_hole, false);
			if (_stool >= 0)
				room.setHidden(_stool, false);
			_bedBroken = true;
			debugC(1, kDebugScript, "jail: the bed is broken at tick %u", script.getTick());
		}
	}

	// The bed broken: 150D_, held, until it has played.
	if (_bedBroken && !_holeDone && !_holeScene && script.startScene(kHoleScene, true)) {
		_holeScene = true;
		_lastScene = kHoleScene;
	}

	// 150D_ played: Shaggy goes down the hole, in front of the stool, and
	// Scooby is gone; from his step 6 behind it; idle again, the room.
	if (_holeDone) {
		if (!_falling && isIdle(shaggy)) {
			shaggy->special(room, sound, kLeave);
			if (_stool >= 0)
				room.setDepth(shaggy->getSprite(), room.getDepth(_stool) + 1);
			if (scooby)
				room.setHidden(scooby->getSprite(), true);
			_falling = true;
			playSound("Scooby_Shaggy_Fall_In_Hole");
		}
		if (_falling && shaggy) {
			if (room.getStep(shaggy->getSprite()) == 6 && _stool >= 0)
				room.setDepth(shaggy->getSprite(), room.getDepth(_stool) - 1);
			if (isIdle(shaggy)) {
				_falling = false;
				_engine.requestRoom(kTunnelDark);
				room.setHidden(shaggy->getSprite(), true);
				debugC(1, kDebugScript, "jail: down the hole at tick %u", script.getTick());
			}
		}
	}
}

void JailRoom::saveKeys(Common::INIFile &profile) {
	// 0x4138B0: the base keys, then BedBroken, in the room's section.
	profile.setKey("BedBroken", kJail, _bedBroken ? "Yes" : "No");
}

void JailRoom::loadKeys(const Common::INIFile &profile) {
	// 0x4138F0 -> 0x40C560: a missing key reads as false.
	Common::String v;
	_bedBroken = profile.getKey("BedBroken", kJail, v) && v == "Yes";
}

RoomClass *createJailRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kJail)
		return new JailRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
