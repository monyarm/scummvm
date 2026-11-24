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
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"
#include "terraglyph/rooms/showdown/stage.h"

namespace Terraglyph {

static const char *const kStage = "ROOM_P05_Stage";
static const char *const kTrapdoor = "ROOM_P06_Stage_Trapdoor";
static const char *const kHammer = "OBJ_INV_HAMMER";

static Common::String bagKey(uint i) {
	return Common::String::format("BagDown%u", i);
}

// The digit a sprite's sequence carries at @p fromEnd characters before its
// end, as an index 0-3, or -1.
static int digitAt(const Common::String &anim, uint fromEnd) {
	if (anim.size() < fromEnd)
		return -1;
	const int i = anim[anim.size() - fromEnd] - '1';
	return i >= 0 && i < 4 ? i : -1;
}

// ---------------------------------------------------------------------------
// The stage

void StageRoom::reset() {
	// 0x417CB0, after the base reset.
	_gives = false;
	_thanks = false;
}

void StageRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x4181B0. Jenny, and the trapdoor, only once she has been met; the way
	// down to the trapdoor only until then. Each sandbag a sprite under
	// layer 0 at depth -1001, kept by its sequence's digit.
	const bool met = _engine.getInventory()->hasMetSuspect("JENNY");
	if (object == "OBJ_JENNY" || object == "OBJ_P05_STAGE_TRAPDOOR") {
		if (!met)
			claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_P05_STAGE_SANDBAG") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = -1001;
	} else if (object == "OBJ_P05_T0_P06") {
		if (met)
			claim.build = Room::Claim::kSkip;
	}
}

void StageRoom::enter(const Common::String &from) {
	// 0x417F10, after the base entry.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Inventory &inventory = *_engine.getInventory();
	_lastScene.clear();
	_jennyClicked = false;
	for (uint i = 0; i < 4; i++)
		_sandbags[i] = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) != "OBJ_P05_STAGE_SANDBAG")
			continue;
		const int i = digitAt(room.getAnim(s), 1);
		if (i >= 0)
			_sandbags[i] = s;
	}

	// Jenny met: the rusty hammer, if never had; and before her clue, her
	// scene, blocking, the clue, and she gives when idle.
	const bool met = inventory.hasMetSuspect("JENNY");
	if (met && !inventory.hasItem(kHammer))
		inventory.addItem(kHammer, "ANIM_TOOLBARRUSTY_HAMMER", "ANIM_CURSORRUSTY_HAMMER");
	if (met && !_engine.isClueOwner("JENNY")) {
		if (script.startScene("Stage.380D2_", true))
			_lastScene = "Stage.380D2_";
		_engine.giveClue("JENNY");
		_gives = true;
	}

	// The first visit hangs the sandbags by the level (BagsRandomized):
	// below level 2 the first is up and the others down; at level 2 the
	// first is down and the others up.
	if (!_engine.getGameFlag("BagsRandomized")) {
		const int level = _engine.getDifficulty();
		if (level < 2) {
			for (uint i = 0; i < 4; i++)
				_engine.setGameFlag(bagKey(i), i != 0);
		} else if (level == 2) {
			for (uint i = 0; i < 4; i++)
				_engine.setGameFlag(bagKey(i), i == 0);
		}
		_engine.setGameFlag("BagsRandomized", true);
	}
	// A sandbag that is up is out of sight over the stage (0x4313F0).
	for (uint i = 0; i < 4; i++) {
		if (_sandbags[i] >= 0)
			room.setHidden(_sandbags[i], !_engine.getGameFlag(bagKey(i)));
	}

	// Shaggy walks in: from the saloon down and left (0x405F20), from under
	// the stage up and right (0x405E60).
	Character *shaggy = script.getCharacter("SHAGGY");
	if (shaggy && from == "ROOM_P12_Saloon")
		shaggy->walkIn(room, *_engine.getSound(), "DL");
	else if (shaggy && from == kTrapdoor)
		shaggy->walkIn(room, *_engine.getSound(), "UR");
}

bool StageRoom::click(int sprite) {
	// Jenny's clicked flag (sprite +8), read by the tick.
	Character *jenny = _engine.getScript()->getCharacter("JENNY");
	if (jenny && sprite >= 0 && sprite == jenny->getSprite())
		_jennyClicked = true;
	return false;
}

void StageRoom::tick() {
	// 0x417CD0, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	const bool clicked = _jennyClicked;
	_jennyClicked = false;
	Character *jenny = script.getCharacter("JENNY");
	if (jenny) {
		// Jenny clicked with the arrow: one of three lines, not held.
		if (clicked && _engine.getInventory()->isDefaultCursor()) {
			static const char *const kLines[] = { "Stage.1801_", "Stage.1802_", "Stage.1803_" };
			const char *scene = kLines[_engine.getRandomNumber(2)];
			if (script.startScene(scene, false))
				_lastScene = scene;
		}
		if (jenny->getState() != Character::kStateIdle)
			return;
		// Idle with her clue to give: she gives, and input is held.
		if (_gives) {
			jenny->act(room, sound, "GIVES");
			_gives = false;
			_thanks = true;
			debugC(1, kDebugScript, "stage: Jenny gives at tick %u", script.getTick());
		}
		if (jenny->getState() != Character::kStateIdle)
			return;
	}
	// Done: the gang thanks her and goes to the suspects and clues.
	if (_thanks) {
		_thanks = false;
		RoomScript::Exit exit;
		exit.destination = "ROOM_Suspect_Chooser";
		exit.scene = "Global.Thank_You_";
		exit.movie = -1;
		script.takeExit(exit);
		debugC(1, kDebugScript, "stage: thanks, then the suspects and clues");
	}
}

// ---------------------------------------------------------------------------
// Under the stage

void StageTrapdoorRoom::reset() {
	// 0x418510, after the base reset.
	_knock = -10;
	_doorOpening = false;
	_visits = 0;
}

void StageTrapdoorRoom::prepare() {
	// 0x418490 (+0x30): level 0, 370BS_; levels 1 and 2, 370B_, 370B2_, then
	// 370B3_ on each later visit. The opening plays on every visit.
	RoomScript &script = *_engine.getScript();
	const int level = _engine.getDifficulty();
	if (level == 0) {
		script.setOpening("Stage_Trapdoor.370BS_");
	} else if (level > 0 && level < 3) {
		const int n = _visits++;
		script.setOpening(n == 0 ? "Stage_Trapdoor.370B_" : n == 1 ? "Stage_Trapdoor.370B2_" : "Stage_Trapdoor.370B3_");
	} else {
		script.getMemory(kTrapdoor).openingPlayed = false;
	}
}

void StageTrapdoorRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x418A10. The levers clickables at depth -500, each calling back
	// (0x401740); the sandbags and the trapdoor sprites under layer 0, the
	// trapdoor at depth -100. The sandbags' depth and sequence are set on
	// entry, where their place is known.
	if (object == "OBJ_TRAPDOOR_LEVER") {
		claim.build = Room::Claim::kClickable;
		claim.depth = -500;
	} else if (object == "OBJ_TRAPDOOR_SANDBAG") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
	} else if (object == "OBJ_TRAPDOOR_DOOR") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = -100;
	}
}

void StageTrapdoorRoom::enter(const Common::String &from) {
	// 0x418870, after the base entry: the toolbar slides off (0x407CC0; the
	// toolbar's own list has this room), and the first knock comes on the
	// next tick.
	Room &room = *_engine.getRoom();
	_knock = -5;
	_leaving = false;
	_lastSound.clear();
	_door = _exit = -1;
	for (uint i = 0; i < 4; i++)
		_levers[i] = _sandbags[i] = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &object = room.getObjectName(s);
		const Common::String &anim = room.getAnim(s);
		if (object == "OBJ_TRAPDOOR_LEVER") {
			const int i = digitAt(anim, 1);
			if (i >= 0)
				_levers[i] = s;
		} else if (object == "OBJ_TRAPDOOR_SANDBAG") {
			// At depth y - 240, on the last step of its drop; one that is up
			// on the last step of its raise.
			const int i = digitAt(anim, 6);
			if (i < 0)
				continue;
			_sandbags[i] = s;
			room.setDepth(s, room.getY(s) - 240);
			if (_engine.getGameFlag(bagKey(i)))
				room.setPlayer(s, 5, -1);
			else
				room.startAnim(s, Common::String::format("ANIM_TRAPDOOR_SANDBAG%d_RAISE", i + 1), 5, -1);
		} else if (object == "OBJ_TRAPDOOR_DOOR") {
			_door = s;
		} else if (object == "OBJ_TRAPDOOR_EXIT") {
			// 0x408D20 with layer -1 and depth -1000.
			_exit = s;
			room.setDepth(s, -1000);
		}
	}
}

void StageTrapdoorRoom::moveSandbag(uint i, bool drop) {
	// 0x418D50: up and dropped, it plays its drop; down and raised, its raise.
	Room &room = *_engine.getRoom();
	const int s = _sandbags[i];
	const Common::String anim = s >= 0 ? room.getAnim(s) : Common::String();
	const bool down = _engine.getGameFlag(bagKey(i));
	const char *sound;
	if (!down && drop) {
		if (s >= 0 && anim.hasSuffix("RAISE"))
			room.startAnim(s, Common::String::format("ANIM_TRAPDOOR_SANDBAG%u_DROP", i + 1), 3, -1);
		_engine.setGameFlag(bagKey(i), true);
		sound = "Stage_Trapdoor.TRAPDOOR_SANDBAG1_DROP";
	} else if (down && !drop) {
		if (s >= 0 && anim.hasSuffix("_DROP"))
			room.startAnim(s, Common::String::format("ANIM_TRAPDOOR_SANDBAG%u_RAISE", i + 1), 3, -1);
		_engine.setGameFlag(bagKey(i), false);
		sound = "Stage_Trapdoor.TRAPDOOR_SANDBAG1_RAISE";
	} else {
		return;
	}
	_engine.getSound()->play(sound);
	_lastSound = sound;
}

void StageTrapdoorRoom::clicked(uint sprite) {
	// 0x418F50, a lever's callback after its click: the lever's digit picks
	// its alias (LeverAlias0-3, game +0x25C), and the alias and the level
	// pick the sandbags it moves, each the other way.
	if (_doorOpening)
		return;
	Room &room = *_engine.getRoom();
	const int n = digitAt(room.getAnim(sprite), 1);
	if (n < 0)
		return;
	const int alias = _engine.getGameInt(Common::String::format("LeverAlias%d", n));
	const int level = _engine.getDifficulty();
	bool up[4];
	for (uint i = 0; i < 4; i++)
		up[i] = !_engine.getGameFlag(bagKey(i));
	// The sandbags each alias moves at levels 0, 1 and 2.
	static const int8 kMoves[4][3][3] = {
		{ { 0, -1, -1 }, { 0, 1, -1 }, { 0, 1, 2 } },
		{ { 1, -1, -1 }, { 1, -1, -1 }, { 1, 2, 3 } },
		{ { 2, -1, -1 }, { 2, 3, -1 }, { 2, 3, 0 } },
		{ { 3, -1, -1 }, { 3, -1, -1 }, { 3, 0, 1 } },
	};
	if (alias >= 0 && alias < 4 && level >= 0 && level < 3) {
		for (uint k = 0; k < 3; k++) {
			const int b = kMoves[alias][level][k];
			if (b >= 0)
				moveSandbag(b, up[b]);
		}
	}
	debugC(1, kDebugScript, "under the stage: lever %d (alias %d) at level %d: bags down %d%d%d%d", n + 1, alias, level,
	       _engine.getGameFlag(bagKey(0)) ? 1 : 0, _engine.getGameFlag(bagKey(1)) ? 1 : 0, _engine.getGameFlag(bagKey(2)) ? 1 : 0,
	       _engine.getGameFlag(bagKey(3)) ? 1 : 0);

	// All four up: the trapdoor opens, Jenny is met (0x40A8B0), and the way
	// out can no longer be clicked.
	bool allUp = true;
	for (uint i = 0; i < 4; i++)
		allUp = allUp && !_engine.getGameFlag(bagKey(i));
	if (allUp) {
		if (_door >= 0)
			room.startAnim(_door, "ANIM_TRAPDOOR_DOOROPEN", 3, -1);
		_doorOpening = true;
		_engine.getInventory()->meetSuspect("JENNY");
		_engine.getSound()->play("Stage_Trapdoor.TRAPDOOR_DOOR_OPEN");
		_lastSound = "Stage_Trapdoor.TRAPDOOR_DOOR_OPEN";
		if (_exit >= 0)
			room.setHitType(_exit, 0);
		debugC(1, kDebugScript, "under the stage: the trapdoor opens at tick %u", _engine.getScript()->getTick());
	}
	// The levers can't be clicked until the sandbags stop.
	for (uint i = 0; i < 4; i++) {
		if (_levers[i] >= 0)
			room.setHitType(_levers[i], 0);
	}
}

bool StageTrapdoorRoom::leaveBy(const Common::String &exitObject) {
	leave(_engine.getScript()->readExit(exitObject));
	return true;
}

void StageTrapdoorRoom::leave(RoomScript::Exit exit) {
	// 0x4188B0 (+0x24): the trapdoor open, the way out plays movie 4. Then
	// the default handler.
	if (_doorOpening && _door >= 0 && _engine.getRoom()->isStopped(_door))
		exit.movie = 4;
	_lastExit = exit;
	debugC(1, kDebugScript, "under the stage: out to %s, movie %d", exit.destination.c_str(), exit.movie);
	_engine.getScript()->startExit(exit);
}

void StageTrapdoorRoom::tick() {
	// 0x418540; the original runs the trapdoor check before the base tick.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	// The trapdoor open: up onto the stage.
	if (_doorOpening && !_leaving && _door >= 0 && room.isStopped(_door)) {
		_leaving = true;
		RoomScript::Exit exit;
		exit.destination = kStage;
		exit.movie = -1;
		leave(exit);
	}
	// Jenny knocks or yells every 74 ticks: the countdown runs from 72 to -1.
	if (_knock < -5) {
		sound.play("Stage_Trapdoor.Knock1");
		_lastSound = "Stage_Trapdoor.Knock1";
		_knock = 72;
	}
	if (_knock-- < 0) {
		_knock = 72;
		static const char *const kKnocks[] = { "Stage_Trapdoor.Knock1", "Stage_Trapdoor.Knock2", "Stage_Trapdoor.Knock3",
			"Stage_Trapdoor.Knock4", "Stage_Trapdoor.Knock5", "Stage_Trapdoor.Yell1", "Stage_Trapdoor.Yell2", "Stage_Trapdoor.Yell3",
			"Stage_Trapdoor.Yell4" };
		_lastSound = kKnocks[_engine.getRandomNumber(8)];
		sound.play(_lastSound);
	}
	// Every sandbag still and the trapdoor shut: the levers can be clicked.
	bool still = true;
	for (uint i = 0; i < 4; i++)
		still = still && (_sandbags[i] < 0 || room.isStopped(_sandbags[i]));
	if (still && !_doorOpening) {
		for (uint i = 0; i < 4; i++) {
			if (_levers[i] >= 0)
				room.setHitType(_levers[i], 2);
		}
	}
}

RoomClass *createStageRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kStage)
		return new StageRoom(engine);
	if (room == kTrapdoor)
		return new StageTrapdoorRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
