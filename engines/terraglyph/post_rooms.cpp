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
#include "terraglyph/post_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const char *const kPost = "ROOM_P08_Post_Office";
static const char *const kStreet1 = "ROOM_P03_Street1";
static const char *const kDark = "ROOM_P14_Tunnel_Dark";
static const char *const kLit = "ROOM_P18_Tunnel_Lit";
static const char *const kLantern = "OBJ_INV_LANTERN";
static const char *const kLanternCursor = "ANIM_CURSORLANTERN";
static const char *const kHole = "OBJ_P08_POST_OFFICE_HOLE";
static const char *const kHoleLit = "OBJ_P08_POST_OFFICE_HOLE_LIT";
static const char *const kTossScene = "Post_Office.160C_";

static int findSprite(const Room &room, const char *object) {
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

static bool inSlot(const Inventory &inventory, const char *item) {
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (inventory.getSlot(i) == item)
			return true;
	}
	return false;
}

// No scene (+0x14) and the opening done (+0x10, or no +0x78).
static bool isFree(const RoomScript &script) {
	return !script.isScenePlaying() && script.isOpeningDone();
}

void PostOfficeRoom::reset() {
	// 0x413A00, after the base reset.
	_holeLine = 0;
	_hotspot3Line = 0;
	_oldRoomName = kStreet1;
}

void PostOfficeRoom::arrive(const Common::String &from) {
	// 0x413EE0 (+0x2C): while MoaningActive, 160A_; up from the dark tunnel,
	// 160D_; both unplayed. Otherwise none, its played flag as it was
	// (0x40C6B0).
	RoomScript &script = *_engine.getScript();
	if (_engine.getGameFlag("MoaningActive")) {
		script.setOpening("Post_Office.160A_");
		return;
	}
	if (from == kDark) {
		script.setOpening("Post_Office.160D_");
		return;
	}
	RoomScript::Memory &memory = script.getMemory(kPost);
	const bool played = memory.openingPlayed;
	script.setOpening("");
	memory.openingPlayed = played;
}

void PostOfficeRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x413F40. The two holes are exits built as the default path builds
	// them (0x408D20 with -1, -1000), and found by name: entry hides the one
	// TunnelLit (game +0x275) doesn't show. Hotspot 2 a clickable under layer
	// 1 (game +0x5C) at the depth 0x401000 gives it without priority=, -1.
	// Hotspot 3 as the default path builds it.
	if (object == "OBJ_P08_POST_OFFICE_HOTSPOT2") {
		claim.build = Room::Claim::kClickable;
		claim.layer = 1;
		claim.depth = -1;
	}
}

void PostOfficeRoom::enter(const Common::String &from) {
	// 0x413A30, after the base entry. The room come from is the saved one
	// unless it is the dark tunnel or Street 1. Up from the dark tunnel only
	// Shaggy and Scooby are here, and hotspots 2 and 3 have no open hand
	// (0x401770). Otherwise, before the lantern has been had, only Fred and
	// Daphne are.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	_lastScene.clear();
	_lastSound.clear();
	_hole = findSprite(room, kHole);
	_holeLit = findSprite(room, kHoleLit);
	_hotspot2 = findSprite(room, "OBJ_P08_POST_OFFICE_HOTSPOT2");
	_hotspot3 = findSprite(room, "OBJ_P08_POST_OFFICE_HOTSPOT3");
	// 0x413F40's hiding: the dark hole once lit, the lit one until then.
	const bool lit = _engine.getGameFlag("TunnelLit");
	if (_hole >= 0)
		room.setHidden(_hole, lit);
	if (_holeLit >= 0)
		room.setHidden(_holeLit, !lit);

	Common::String came = from;
	if (came != kDark && came != kStreet1)
		came = _oldRoomName;
	auto hide = [&](const char *name) {
		if (Character *c = script.getCharacter(name))
			room.setHidden(c->getSprite(), true);
	};
	if (came == kDark) {
		hide("FRED");
		hide("DAPHNE");
		hide("VELMA");
		if (_hotspot3 >= 0)
			room.setRollover(_hotspot3, "");
		if (_hotspot2 >= 0)
			room.setRollover(_hotspot2, "");
	} else if (!_engine.getInventory()->hasItem(kLantern)) {
		hide("SCOOBY");
		hide("SHAGGY");
		hide("VELMA");
	}
	_oldRoomName = came;
	_tossPending = false;
	_tossing = false;
	_lanternOnHole = false;
	_lanternScene = false;
}

bool PostOfficeRoom::click(int sprite) {
	// 0x414270 reads the hole's clicked flag (+8) with the lantern as the
	// cursor; the class takes that click, so nothing else does.
	if (sprite >= 0 && sprite == _hole && _engine.getInventory()->getCursor() == kLanternCursor) {
		_lanternOnHole = true;
		return true;
	}
	return false;
}

void PostOfficeRoom::event(const Common::String &object, bool success) {
	// 0x4140F0. Every scene here holds input.
	RoomScript &script = *_engine.getScript();
	const char *scene = nullptr;
	if (object == "OBJ_P08_POST_OFFICE_HOTSPOT3") {
		static const char *const kLines[] = { "P08_POST_OFFICE_HOTSPOT3A_", "P08_POST_OFFICE_HOTSPOT3B_", "P08_POST_OFFICE_HOTSPOT3C_" };
		const int old = _hotspot3Line;
		if (old < 3) {
			scene = kLines[old];
			_hotspot3Line = old == 2 ? 0 : old + 1;
		}
	} else if (object == "OBJ_P08_POST_OFFICE_HOTSPOT2") {
		if (_hotspot2 >= 0 && script.getClickableState(_hotspot2) == "ANIM_P08_POST_OFFICE_HOTSPOT2C")
			scene = "Post_Office.160X4_";
	}
	if (scene && script.startScene(scene, true))
		_lastScene = scene;
}

bool PostOfficeRoom::leaveBy(const Common::String &exitObject) {
	// 0x413BC0 (+0x24): down to the lit tunnel, with the lantern not yet
	// used, the gang stays. With the whole gang here, 160AA_ (or 160B_ once
	// Artie has been met), 160B2_, 160B3_ in turn; with Daphne gone, 380A_
	// or 380A1_ (rand() & 1). All held. Else the default handler.
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	if (script.readExit(exitObject).destination != kLit)
		return false;
	if (!inSlot(inventory, kLantern) && inventory.hasItem(kLantern))
		return false;
	const char *scene = nullptr;
	Character *daphne = script.getCharacter("DAPHNE");
	if (daphne && !_engine.getRoom()->isHidden(daphne->getSprite())) {
		if (_holeLine == 0) {
			scene = inventory.hasMetSuspect("ARTIE") ? "Post_Office.160B_" : "Post_Office.160AA_";
			_holeLine++;
		} else if (_holeLine == 1) {
			scene = "Post_Office.160B2_";
			_holeLine++;
		} else if (_holeLine == 2) {
			scene = "Post_Office.160B3_";
			_holeLine = 0;
		}
	} else {
		scene = _engine.getRandomNumber(1) == 0 ? "Post_Office.380A_" : "Post_Office.380A1_";
	}
	if (scene && script.startScene(scene, true))
		_lastScene = scene;
	debugC(1, kDebugScript, "post office: not down in the dark: %s", scene ? scene : "-");
	return true;
}

void PostOfficeRoom::tick() {
	// 0x414270, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	Character *fred = script.getCharacter("FRED");

	// 160C_'s done flag, set by the base tick as it ends.
	if (_lanternScene && script.getScene() != kTossScene) {
		_lanternScene = false;
		_tossPending = true;
	}

	// 160C_ over: Fred tosses the lantern down; on his step 15 the hole is
	// lit, and the gang goes down to the lit tunnel.
	if (_tossPending) {
		if (!_tossing && fred && fred->getState() == Character::kStateIdle && isFree(script)) {
			fred->special(room, sound, "OBJ_FRED_P08_HOLD_LANTERN");
			const bool found = sound.play("Fred.Tosses.Lantern");
			_lastSound = Common::String::format("Fred.Tosses.Lantern found %d", found ? 1 : 0);
			_tossing = true;
		}
		if (_tossing && fred && room.getStep(fred->getSprite()) == 15) {
			if (_hole >= 0)
				room.setHidden(_hole, true);
			if (_holeLit >= 0)
				room.setHidden(_holeLit, false);
			_tossing = false;
			_tossPending = false;
			debugC(1, kDebugScript, "post office: the lantern down the hole at tick %u", script.getTick());
			_engine.requestRoom(kLit);
		}
	}

	// The hole clicked with the lantern: it is used up, the arrow back,
	// TunnelLit, and 160C_, held, with +0xD0 as its done flag.
	if (_lanternOnHole) {
		_lanternOnHole = false;
		inventory.removeItem(kLantern);
		inventory.setCursor("ANIM_CURSORARROW");
		_engine.setGameFlag("TunnelLit", true);
		if (script.startScene(kTossScene, true)) {
			_lanternScene = true;
			_lastScene = kTossScene;
		}
	}
}

void PostOfficeRoom::saveKeys(Common::INIFile &profile) {
	// 0x4141D0: the base keys, then OldRoomName, in the room's section.
	profile.setKey("OldRoomName", kPost, _oldRoomName);
}

void PostOfficeRoom::loadKeys(const Common::INIFile &profile) {
	// 0x414210: a missing key reads as empty.
	Common::String v;
	_oldRoomName = profile.getKey("OldRoomName", kPost, v) ? v : Common::String();
}

RoomClass *createPostRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kPost)
		return new PostOfficeRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
