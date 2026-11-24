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

// Phantom of the Knight's two arenas. The addresses are Phantom's.

#include "common/debug.h"

#include "terraglyph/rooms/phantomknight/arena.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kArena1 = "ROOM_P09_Arena1";
static const char *const kArena2 = "ROOM_P14_Arena2";
static const char *const kMirror = "OBJ_INV_MIRROR";
static const char *const kShield = "OBJ_INV_SHIELD";
static const char *const kPileObject = "OBJ_BRAD_ARMOR_PILE";

static bool inSlot(TerraglyphEngine &engine, const char *item) {
	// 0x4070C0: the item is in one of the toolbar's slots.
	for (uint i = 0; i < Inventory::kSlotCount; i++) {
		if (engine.getInventory()->getSlot(i) == item)
			return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// The first arena

void Arena1Room::reset() {
	// 0x40D540.
	_stoneLine = 0;
	_sharpening = false;
}

void Arena1Room::enter(const Common::String &from) {
	// 0x40D700, after the base entry: from the throne room Velma walks in
	// (U, 0x405FE0), from the second arena Velma (D, 0x406020); back here
	// through the stone's movie, the toolbar opens (0x407C00).
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Character *velma = script.getCharacter("VELMA");
	if (from == "ROOM_P10_Throne_Room") {
		if (velma)
			velma->walkIn(room, sound, "U");
	} else if (from == kArena2) {
		if (velma)
			velma->walkIn(room, sound, "D");
	} else if (from == kArena1) {
		_engine.getInventory()->openToolbar();
	}
}

void Arena1Room::roomLeft() {
	// 0x40D6A0, after the base leave: the shears sharpened, the sharp ones
	// in the toolbar unless ever had.
	if (!_sharpening)
		return;
	_sharpening = false;
	Inventory &inventory = *_engine.getInventory();
	if (!inventory.hasItem("OBJ_INV_SHARP_SHEARS"))
		inventory.addItem("OBJ_INV_SHARP_SHEARS", "ANIM_TOOLBARSHARP_SHEARS", "ANIM_CURSORSHARP_SHEARS");
}

void Arena1Room::event(const Common::String &object, bool success) {
	// 0x40D560. The rusty shears on the stone: movie 40 back to this room
	// (game vtable +0x08), the shears used, the arrow back, sharpened on the
	// way out. The arrow on it: 170XB_, 170A_, 170A1_, 170A2_ in turn,
	// blocking. Hotspot 3: 170X_, not blocking.
	if (object == "OBJ_P09_ARENA_ONE_STONE") {
		if (success) {
			_engine.requestRoom(kArena1, 40);
			Inventory &inventory = *_engine.getInventory();
			inventory.removeItem("OBJ_INV_RUSTY_SHEARS");
			inventory.setCursor("ANIM_CURSORARROW");
			_sharpening = true;
			debugC(1, kDebugScript, "first arena: the shears on the stone");
			return;
		}
		static const char *const kLines[] = { "Arena_1.170XB_", "Arena_1.170A_", "Arena_1.170A1_", "Arena_1.170A2_" };
		_engine.getScript()->startScene(kLines[_stoneLine], true);
		_stoneLine = (_stoneLine + 1) % 4;
	} else if (object == "OBJ_P09_ARENA_ONE_HOTSPOT3") {
		_engine.getScript()->startScene("Arena_1.170X_", false);
	}
}

// ---------------------------------------------------------------------------
// The second arena

int Arena2Room::findSprite(const Common::String &object) const {
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void Arena2Room::reset() {
	// 0x40D7F0.
	_thanks = _giving = _shieldGiven = _bradMoved = false;
	_pile = -1;
	_ignoreClick = _openingPending = false;
}

void Arena2Room::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x40E090.
	// - The armour pile, until Brad's game is won (BradPuzzle, game +0x256),
	//   a clickable as the default path builds it (0x408DC0 with 1, -1),
	//   its first click queued as the room's own (enter()); won, not built.
	// - The mirror, never had, a sprite under the first layer at depth -480;
	//   had, not built.
	// - OBJ_BRAD only once his game is won, OBJ_BRAD_H only until.
	// - The pole, won on level 1 or 2, a sprite at depth 0; else not built.
	// - The arena's shield once the shield is used; else not built.
	Inventory &inventory = *_engine.getInventory();
	const bool won = _engine.getGameFlag("BradPuzzle");
	if (object == kPileObject) {
		if (won)
			claim.build = Room::Claim::kSkip;
	} else if (object == kMirror) {
		if (inventory.hasItem(kMirror)) {
			claim.build = Room::Claim::kSkip;
		} else {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = -480;
		}
	} else if (object == "OBJ_BRAD") {
		if (!won)
			claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_BRAD_H") {
		if (won)
			claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_P14_ARENA2_POLE") {
		if (won && _engine.getDifficulty() != 0) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = 0;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	} else if (object == "OBJ_P14_ARENA2_SHIELD") {
		if (inventory.hasItem(kShield) && !inSlot(_engine, kShield)) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = 0;
		} else {
			claim.build = Room::Claim::kSkip;
		}
	}
}

void Arena2Room::enter(const Common::String &from) {
	// 0x40D830, after the base entry.
	// - His game won and his clue not given: his clue (game vtable +0x44),
	//   Brad met (0x40A8B0), Arena_2.180B_, blocking, and he gives it.
	// - Not won with the opening unplayed: Brad hidden, to rise in it.
	// - The shield given and the mirror never had: Brad 50 to the left
	//   (+0xC7); the mirror had: likewise.
	// - From the throne room Fred walks in (UL), from the first arena Velma
	//   (D); the opening unplayed is kept (+0xCB).
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	_hotspot2 = findSprite("OBJ_P14_ARENA2_HOTSPOT2");
	_pile = findSprite(kPileObject);
	_mirror = findSprite(kMirror);
	_clicked = -1;
	const bool won = _engine.getGameFlag("BradPuzzle");
	const bool played = script.getMemory(kArena2).openingPlayed;
	if (_pile >= 0) {
		// The claim's queued click (0x401840), the room's own.
		_ignoreClick = true;
		script.queueClickable(_pile);
	}
	Character *brad = script.getCharacter("BRAD");
	if (won && !_engine.isClueOwner("BRAD")) {
		_engine.giveClue("BRAD");
		inventory.meetSuspect("BRAD");
		script.startScene("Arena_2.180B_", true);
		_giving = true;
	} else if (!won && !played && brad && brad->getSprite() >= 0) {
		room.setHidden(brad->getSprite(), true);
	}
	const bool hadMirror = inventory.hasItem(kMirror);
	if ((_shieldGiven && !hadMirror) || hadMirror) {
		if (brad && brad->getSprite() >= 0)
			room.setOffset(brad->getSprite(), room.getOffsetX(brad->getSprite()) - 50, room.getOffsetY(brad->getSprite()));
		if (_shieldGiven && !hadMirror)
			_bradMoved = true;
	}
	_mirrorPending = false;
	_velmaTaking = false;
	if (from == "ROOM_P10_Throne_Room") {
		Character *fred = script.getCharacter("FRED");
		if (fred)
			fred->walkIn(room, sound, "UL");
	} else if (from == kArena1) {
		Character *velma = script.getCharacter("VELMA");
		if (velma)
			velma->walkIn(room, sound, "D");
	}
	_openingPending = !played;
	debugC(1, kDebugScript, "second arena: from %s%s%s", from.c_str(), won ? ", Brad's game won" : "", _giving ? "; Brad's clue" : "");
}

void Arena2Room::roomLeft() {
	// 0x40E030, after the base leave: left with the mirror to take, it goes
	// in the toolbar all the same. The pile forgotten.
	Inventory &inventory = *_engine.getInventory();
	if (_mirrorPending && !inventory.hasItem(kMirror))
		inventory.addItem(kMirror, "ANIM_TOOLBARHAND_MIRROR", "ANIM_CURSORHAND_MIRROR");
	_pile = -1;
}

bool Arena2Room::click(int sprite) {
	// 0x40DA80 reads Brad's clicked flag (+0x08) with the shield as the
	// cursor, in the room's tick, before the game's update (0x4098B0) could
	// call it a wrong item; the class takes that click.
	Character *brad = _engine.getScript()->getCharacter("BRAD");
	if (sprite >= 0 && brad && sprite == brad->getSprite() && _engine.getInventory()->getCursor() == "ANIM_CURSORSHIELD") {
		_clicked = sprite;
		return true;
	}
	return false;
}

void Arena2Room::rise() {
	// Brad rises from the pile: RISE (state 0xF), shown, Arena2.Brad_H.Rise.
	// 0x437540 restarts the tick clock, which the fork doesn't keep.
	Character *brad = _engine.getScript()->getCharacter("BRAD");
	if (!brad || brad->getSprite() < 0)
		return;
	brad->act(*_engine.getRoom(), *_engine.getSound(), "RISE");
	_engine.getRoom()->setHidden(brad->getSprite(), false);
	_engine.getSound()->play("Arena2.Brad_H.Rise");
	debugC(1, kDebugScript, "second arena: Brad rises");
}

bool Arena2Room::holdScene() {
	// 0x40E440, before the base scene player. His game not won and the
	// opening played: Brad, still hidden, rises. Else, in the opening
	// (Arena_2.180A_) with its fifth line said: Brad, hidden, rises, and the
	// scene waits until he is idle.
	RoomScript &script = *_engine.getScript();
	Character *brad = script.getCharacter("BRAD");
	const bool hidden = brad && brad->getSprite() >= 0 && _engine.getRoom()->isHidden(brad->getSprite());
	if (!_engine.getGameFlag("BradPuzzle") && script.getMemory(kArena2).openingPlayed) {
		if (hidden)
			rise();
		return false;
	}
	if (script.getScene() == "Arena_2.180A_" && script.getLine() == 5 && !script.isSpeakerBusy()) {
		if (hidden)
			rise();
		if (brad && brad->getState() != Character::kStateIdle)
			return true;
	}
	return false;
}

void Arena2Room::tick() {
	// 0x40DA80, after the base tick.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Inventory &inventory = *_engine.getInventory();
	const int clicked = _clicked;
	_clicked = -1;
	Character *brad = script.getCharacter("BRAD");
	// The opening played on the first visit: the pile clicked, now the
	// player's, into Brad's game.
	if (script.getMemory(kArena2).openingPlayed && _openingPending && _pile >= 0) {
		_ignoreClick = false;
		script.queueClickable(_pile);
	}
	// Brad clicked with the shield: Big Head, Arena_2.180E_ on
	// BIG_HEAD_BG_P14 through movie 29, back here; the shield used, the
	// arrow back (+0xC6).
	if (brad && clicked >= 0 && clicked == brad->getSprite() && inventory.getCursor() == "ANIM_CURSORSHIELD") {
		_engine.startBigHead("Arena_2.180E_", "BIG_HEAD_BG_P14", kArena2, 29);
		inventory.removeItem(kShield);
		inventory.setCursor("ANIM_CURSORARROW");
		_shieldGiven = true;
		debugC(1, kDebugScript, "second arena: the shield for Brad");
	}
	// His clue: no scene playing and the opening done, he GIVES, input
	// held; idle again, the exit handler to ROOM_Suspect_Chooser with
	// Global.Thank_You_.
	if (_giving && !script.isScenePlaying() && script.isOpeningDone()) {
		_giving = false;
		_thanks = true;
		if (brad)
			brad->act(room, sound, "GIVES");
	}
	if (_thanks && brad && brad->getState() == Character::kStateIdle) {
		_thanks = false;
		RoomScript::Exit exit;
		exit.destination = "ROOM_Suspect_Chooser";
		exit.scene = "Global.Thank_You_";
		exit.movie = -1;
		script.takeExit(exit);
		debugC(1, kDebugScript, "second arena: thanks, then the suspects and clues");
	}
	// The mirror: the shield given and Brad stood aside, the mirror never
	// had: Velma, idle with no scene playing and the opening done, takes it
	// (OBJ_VELMA_P14_TAKE_MIRROR, state 0xD, Arena2.Velma_Takes_Mirror),
	// and it is hidden; input held. Idle again: in the toolbar unless had,
	// and input back.
	if (!_mirrorPending && _shieldGiven && _bradMoved && !inventory.hasItem(kMirror))
		_mirrorPending = true;
	if (_mirrorPending) {
		Character *velma = script.getCharacter("VELMA");
		if (!_velmaTaking && velma && velma->getState() == Character::kStateIdle && !script.isScenePlaying() && script.isOpeningDone()) {
			velma->special(room, sound, "OBJ_VELMA_P14_TAKE_MIRROR");
			sound.play("Arena2.Velma_Takes_Mirror");
			_velmaTaking = true;
			if (_mirror >= 0)
				room.setHidden(_mirror, true);
		}
		if (_velmaTaking && velma && velma->getState() == Character::kStateIdle) {
			_velmaTaking = false;
			_mirrorPending = false;
			if (!inventory.hasItem(kMirror))
				inventory.addItem(kMirror, "ANIM_TOOLBARHAND_MIRROR", "ANIM_CURSORHAND_MIRROR");
			debugC(1, kDebugScript, "second arena: Velma has the mirror");
		}
	}
	// The pile rattles, 1 in 500 a tick: Brad.Armor.<rand() % 4 + 1>, and
	// its click queued as the room's own.
	if (_pile >= 0 && _engine.getRandomNumber(499) == 0) {
		sound.play(Common::String::format("Brad.Armor.%d", _engine.getRandomNumber(3) + 1));
		_ignoreClick = true;
		script.queueClickable(_pile);
	}
}

void Arena2Room::event(const Common::String &object, bool success) {
	// 0x40E5A0. The pile clicked by the player: Brad's game for the level
	// (SPOOKY, SPOOKIER, SPOOKTACULAR), through the exit handler; the
	// room's own click only clears the flag. Hotspot 2 moved on to its B:
	// Arena_2.180X_, not blocking.
	RoomScript &script = *_engine.getScript();
	if (object == kPileObject) {
		if (_ignoreClick) {
			_ignoreClick = false;
			return;
		}
		static const char *const kLevels[] = { "ROOM_Brad_SPOOKY", "ROOM_Brad_SPOOKIER", "ROOM_Brad_SPOOKTACULAR" };
		const int level = _engine.getDifficulty();
		if (level < 0 || level > 2)
			return;
		RoomScript::Exit exit;
		exit.destination = kLevels[level];
		exit.movie = -1;
		script.takeExit(exit);
		debugC(1, kDebugScript, "second arena: the pile, to %s", kLevels[level]);
	} else if (object == "OBJ_P14_ARENA2_HOTSPOT2") {
		if (_hotspot2 >= 0 && script.getClickableState(_hotspot2) == "ANIM_P14_ARENA2_HOTSPOT2B")
			script.startScene("Arena_2.180X_", false);
	}
}

Common::String Arena2Room::idleLine(RoomScript &script, const Common::String &name) {
	// 0x40EAC0: Brad's, once his game is won: the shield used, 1010H3_;
	// else 1010H_ or 1010H2_ (rand() & 1). Else the base line.
	if (name == "BRAD" && _engine.getGameFlag("BradPuzzle")) {
		Inventory &inventory = *_engine.getInventory();
		if (inventory.hasItem(kShield) && !inSlot(_engine, kShield))
			return "Arena_2.1010H3_1_BRAD";
		return _engine.getRandomNumber(1) == 0 ? "Arena_2.1010H_1_BRAD" : "Arena_2.1010H2_1_BRAD";
	}
	return script.getBaseIdleLine(name);
}

void Arena2Room::idle() {
	// 0x40EBA0 (+0x34): his game won, Arena_2.1010H4_, not blocking.
	if (_engine.getGameFlag("BradPuzzle"))
		_engine.getScript()->startScene("Arena_2.1010H4_", false);
}

RoomClass *createArenaRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kArena1)
		return new Arena1Room(engine);
	if (room == kArena2)
		return new Arena2Room(engine);
	return nullptr;
}

} // End of namespace Terraglyph
