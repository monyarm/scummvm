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

// Phantom of the Knight's drawbridge and town. The addresses are Phantom's.

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/rooms/phantomknight/drawbridge.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kDrawbridge = "ROOM_P01_Drawbridge";
static const char *const kTown = "ROOM_P04_Town";
static const char *const kFlagPrefix = "OBJ_P01_DRAWBRIDGE_P";
static const char *const kBannerPrefix = "OBJ_P01_DRAWBRIDGE_SP";
static const char *const kDoor = "OBJ_P01_DRAWBRIDGE_DOOR";
static const char *const kOverlay = "OBJ_P01_DRAWBRIDGE_OVERLAY";
static const char *const kWayOut = "OBJ_P01_TO_P04";
static const char *const kFanfare1 = "Drawbridge.Flag_Fanfare.1";
static const char *const kFanfare2 = "Drawbridge.Flag_Fanfare.2";

// OBJ_P01_DRAWBRIDGE_P<n> or …SP<n>, n 1-4: n, else 0.
static int placementNumber(const Common::String &object, const char *prefix) {
	const uint len = strlen(prefix);
	if (object.size() != len + 1 || !object.hasPrefix(prefix))
		return 0;
	const char c = object[len];
	return c >= '1' && c <= '4' ? c - '0' : 0;
}

void DrawbridgeRoom::reset() {
	// 0x426790, after the base reset: the puzzle not done, the sign's first
	// line next, and for each banner a colour (rand() & 3, 1-4) and its
	// flag one to three colours on ((rand() % 3 + banner) & 3, 1-4).
	_puzzleDone = false;
	_signLine = 0;
	for (uint i = 0; i < kFlags; i++) {
		_bannerState[i] = (int)_engine.getRandomNumber(3) + 1;
		_flagState[i] = (((int)_engine.getRandomNumber(2) + _bannerState[i]) & 3) + 1;
	}
}

void DrawbridgeRoom::setStates(const int banner[kFlags], const int flag[kFlags]) {
	for (uint i = 0; i < kFlags; i++) {
		_bannerState[i] = banner[i];
		_flagState[i] = flag[i];
	}
}

int DrawbridgeRoom::findSprite(const Common::String &object) const {
	Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == object)
			return s;
	}
	return -1;
}

void DrawbridgeRoom::playSound(const Common::String &name) {
	const bool found = _engine.getSound()->play(name);
	if (!_sounds.empty())
		_sounds += ", ";
	_sounds += Common::String::format("%s found %d", name.c_str(), found ? 1 : 0);
	debugC(1, kDebugScript, "drawbridge plays %s", name.c_str());
}

void DrawbridgeRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x426A80. The flags (P1-P4) and the banners (SP1-SP4) sprites under
	// the first layer at depth 0; the door one at depth -1, and the overlay
	// one at depth 0. The way to the town isn't built until the puzzle is
	// done. The flags', banners' and door's sequences are set once the room
	// is built, in placement order (enter()).
	if (placementNumber(object, kFlagPrefix) || placementNumber(object, kBannerPrefix) || object == kDoor || object == kOverlay) {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = object == kDoor ? -1 : 0;
		claim.hitType = 1;
		if (object != kOverlay)
			_claimed.push_back(object);
	} else if (object == kWayOut && !_puzzleDone) {
		claim.build = Room::Claim::kSkip;
	}
}

void DrawbridgeRoom::setFlagState(uint i, int state) {
	// 0x427490. Moved off its banner's colour while Velma is idle, she
	// comments. States 1-4 wave that colour (P<n>F<colour>, looping at 4
	// ticks a step from a random step); a click goes on to 6, 8, 10 or 12,
	// and on the banner's colour the first fanfare plays again. States
	// 5-12 lower a colour (mode 4, the even ones) or raise the next (mode 3,
	// the odd ones) at 3 ticks a step, with Drawbridge.Flag_Raise; each goes
	// on when it ends: 6 -> 7 -> 2, 8 -> 9 -> 3, 10 -> 11 -> 4, 12 -> 5 -> 1.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Flag &f = _flags[i];
	if (f.state == f.target) {
		Character *velma = _engine.getScript()->getCharacter("VELMA");
		if (velma && velma->getState() == Character::kStateIdle)
			velma->say(room, sound, "Drawbridge.100A_3_VELMA");
	}
	f.state = state;
	if (state >= 1 && state <= 4) {
		const int start = (int)_engine.getRandomNumber(3);
		room.startAnim(f.sprite, Common::String::format("ANIM_P01_DRAWBRIDGE_P%iF%i", f.number, state), 1, start, 4);
		f.next = 4 + 2 * state;
		if (f.target == f.state) {
			sound.stopClip(kFanfare1);
			playSound(kFanfare1);
		}
	} else if (state >= 5 && state <= 12) {
		playSound("Drawbridge.Flag_Raise");
		const int colour = (state - 5) / 2 + 1;
		const bool raise = (state & 1) != 0;
		room.startAnim(f.sprite, Common::String::format("ANIM_P01_DRAWBRIDGE_P%iF%i_RISE", f.number, colour), raise ? 3 : 4, -1, 3);
		f.next = raise ? colour : state + 1;
	}
}

void DrawbridgeRoom::enter(const Common::String &from) {
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	// 0x426A80, for the sprites built: each flag on its saved colour, its
	// banner's colour its target, clickable until the puzzle is done; each
	// banner waving its colour (SP<n>F<colour>, looping at 4 ticks a step
	// from a random step); the door stopped on its first step, or its last
	// once the puzzle is done (mode 5).
	for (uint i = 0; i < kFlags; i++)
		_flags[i] = Flag();
	_banners[0] = _banners[1] = _banners[2] = _banners[3] = -1;
	_door = -1;
	for (uint k = 0; k < _claimed.size(); k++) {
		const Common::String &object = _claimed[k];
		const int s = findSprite(object);
		if (s < 0)
			continue;
		int n;
		if ((n = placementNumber(object, kFlagPrefix)) != 0) {
			Flag &f = _flags[n - 1];
			f.sprite = s;
			f.target = _bannerState[n - 1];
			f.number = n;
			f.enabled = !_puzzleDone;
			setFlagState(n - 1, _flagState[n - 1]);
			room.setActor(s, this);
			room.setRollover(s, f.enabled ? "ANIM_CURSOROPEN" : "");
		} else if ((n = placementNumber(object, kBannerPrefix)) != 0) {
			_banners[n - 1] = s;
			const int start = (int)_engine.getRandomNumber(3);
			room.startAnim(s, Common::String::format("ANIM_P01_DRAWBRIDGE_SP%iF%i", n, _bannerState[n - 1]), 1, start, 4);
		} else if (object == kDoor) {
			_door = s;
			room.setPlayer(s, _puzzleDone ? 5 : 0, -1, 3);
		}
	}
	_claimed.clear();

	// 0x426840, after the base entry: the sound section (0x435B60) isn't
	// preloaded here; the flags' loop plays; one snack is needed (+0x24);
	// from the town Velma walks in (U, 0x405FE0); no fanfare is waited on.
	playSound("Drawbridge.Flag_Loop");
	script.setSnacksNeeded(1);
	if (from == kTown) {
		Character *velma = script.getCharacter("VELMA");
		if (velma)
			velma->walkIn(room, *_engine.getSound(), "U");
	}
	_fanfare.clear();
	_clicked = -1;
	debugC(1, kDebugScript, "drawbridge: banners %d %d %d %d, flags %d %d %d %d, puzzle %s", _bannerState[0], _bannerState[1], _bannerState[2],
	       _bannerState[3], _flagState[0], _flagState[1], _flagState[2], _flagState[3], _puzzleDone ? "done" : "not done");
}

bool DrawbridgeRoom::click(int sprite) {
	// The flags read the click (game +0x20 bit 4) in their update; the
	// engine still handles it.
	_clicked = sprite;
	return false;
}

void DrawbridgeRoom::actorUpdate(uint sprite) {
	// 0x427430, after the flag's player: clicked with the arrow while it is
	// clickable, or its sequence over, it goes on to its next state.
	for (uint i = 0; i < kFlags; i++) {
		Flag &f = _flags[i];
		if (f.sprite != (int)sprite)
			continue;
		if (f.enabled && _clicked == (int)sprite && _engine.getInventory()->isDefaultCursor())
			setFlagState(i, f.next);
		if (_engine.getRoom()->getMode(sprite) == 0)
			setFlagState(i, f.next);
		return;
	}
}

void DrawbridgeRoom::tick() {
	// 0x4268D0, after the base tick.
	Room &room = *_engine.getRoom();
	if (!_fanfare.empty() && !_engine.getSound()->isClipPlaying(_fanfare)) {
		// The second fanfare over: the drawbridge comes down, and the gang
		// goes in (Drawbridge.Enter_, blocking). 0x436810 restarts the
		// game's tick clock, which the fork doesn't keep.
		_fanfare.clear();
		if (_door >= 0)
			room.setPlayer(_door, 3, -1, 3);
		playSound("Drawbridge.Down");
		_engine.getScript()->startScene("Drawbridge.Enter_", true);
		debugC(1, kDebugScript, "drawbridge: down");
	}
	if (!_puzzleDone) {
		// Each flag's colour is kept as it is read; the first off its
		// banner's ends the look.
		bool all = true;
		for (uint i = 0; i < kFlags && all; i++) {
			if (_flags[i].sprite < 0) {
				all = false;
				break;
			}
			_flagState[i] = _flags[i].state;
			all = _flags[i].state == _bannerState[i];
		}
		if (all) {
			// All four right: the puzzle done, the flags no longer
			// clickable, the way to the town built (0x401850 under the first
			// layer at depth -1, hit by its polygon), and the second fanfare.
			_puzzleDone = true;
			for (uint i = 0; i < kFlags; i++) {
				_flags[i].enabled = false;
				room.setRollover(_flags[i].sprite, "");
			}
			room.addExitSprite(room.getLayerSprite(0), kWayOut, "ANIM_P01_TO_P04EXIT", -1);
			playSound(kFanfare2);
			_fanfare = kFanfare2;
			debugC(1, kDebugScript, "drawbridge: puzzle done");
		}
	}
	_clicked = -1;
}

void DrawbridgeRoom::event(const Common::String &object, bool success) {
	// 0x427270: the sign clicked says its two lines in turn, not blocking.
	if (object != "OBJ_INV_1428_SIGN")
		return;
	const char *scene = _signLine == 0 ? "Drawbridge.100X1_" : "Drawbridge.100X2_";
	_signLine = _signLine == 0 ? 1 : 0;
	_engine.getScript()->startScene(scene, false);
}

Common::String DrawbridgeRoom::idleLine(RoomScript &script, const Common::String &name) {
	// 0x4272D0: the base line, but Velma's own here is Global.Idle.VELMA
	// once the puzzle is done (+0xFC).
	const Common::String line = script.getBaseIdleLine(name);
	if (line == "P01_Drawbridge.Idle.VELMA" && _puzzleDone)
		return "Global.Idle.VELMA";
	return line;
}

bool DrawbridgeRoom::leaveBy(const Common::String &exitObject) {
	// 0x426EA0: with no snack eaten here, Drawbridge.110A_ plays, blocking,
	// and the snack box flashes, instead of leaving.
	RoomScript &script = *_engine.getScript();
	if (script.getMemory(kDrawbridge).snacksGiven != 0)
		return false;
	script.startScene("Drawbridge.110A_", true);
	_engine.getInventory()->flashSnackBox();
	debugC(1, kDebugScript, "drawbridge: no snack yet");
	return true;
}

void DrawbridgeRoom::saveKeys(Common::INIFile &profile) {
	// 0x427010: FlagState0-3 and BannerState0-3, then PuzzleDone.
	for (uint i = 0; i < kFlags; i++) {
		profile.setKey(Common::String::format("FlagState%u", i), kDrawbridge, Common::String::format("%d", _flagState[i]));
		profile.setKey(Common::String::format("BannerState%u", i), kDrawbridge, Common::String::format("%d", _bannerState[i]));
	}
	profile.setKey("PuzzleDone", kDrawbridge, _puzzleDone ? "Yes" : "No");
}

void DrawbridgeRoom::loadKeys(const Common::INIFile &profile) {
	// 0x427140: each key read while those before it were; PuzzleDone last,
	// only Yes true.
	Common::String v;
	for (uint i = 0; i < kFlags; i++) {
		if (!profile.getKey(Common::String::format("FlagState%u", i), kDrawbridge, v))
			return;
		_flagState[i] = atoi(v.c_str());
		if (!profile.getKey(Common::String::format("BannerState%u", i), kDrawbridge, v))
			return;
		_bannerState[i] = atoi(v.c_str());
	}
	if (profile.getKey("PuzzleDone", kDrawbridge, v))
		_puzzleDone = v == "Yes";
}

void TownRoom::enter(const Common::String &from) {
	// 0x418150, after the base entry: from the drawbridge Velma walks in (D,
	// 0x406020), from the souvenir shop Shaggy (UR, 0x405E60), from the
	// gardener's house Fred (UR), from the room of doors Velma (U, 0x405FE0).
	// While the story has Jane screaming (game +0x288, Screaming), one of
	// her screams: rand() % 5 picks 1, 2, 3, or 5 for 3 and 4.
	RoomScript &script = *_engine.getScript();
	const char *who = nullptr;
	const char *direction = nullptr;
	if (from == kDrawbridge) {
		who = "VELMA";
		direction = "D";
	} else if (from == "ROOM_P03_Souvenir_Shop") {
		who = "SHAGGY";
		direction = "UR";
	} else if (from == "ROOM_P05_Gardeners_House") {
		who = "FRED";
		direction = "UR";
	} else if (from == "ROOM_P11_Room_Of_Doors") {
		who = "VELMA";
		direction = "U";
	}
	if (who) {
		Character *c = script.getCharacter(who);
		if (c)
			c->walkIn(*_engine.getRoom(), *_engine.getSound(), direction);
	}
	_scream.clear();
	if (_engine.getGameFlag("Screaming")) {
		static const int kScreams[] = { 1, 2, 3, 5, 5 };
		_scream = Common::String::format("Town.Jane_Screaming%d", kScreams[_engine.getRandomNumber(4)]);
		_engine.getSound()->play(_scream);
	}
	debugC(1, kDebugScript, "town: from %s, %s walks in %s; scream %s", from.c_str(), who ? who : "nobody", direction ? direction : "-",
	       _scream.empty() ? "none" : _scream.c_str());
}

RoomClass *createDrawbridgeRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kDrawbridge)
		return new DrawbridgeRoom(engine);
	if (room == kTown)
		return new TownRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
