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

#include "terraglyph/detection.h"
#include "terraglyph/rooms/showdown/gummy.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *const kGummyRoom = "ROOM_Gummy";
static const char *const kSnore = "ANIM_GUMMYSLEEP_GUMMYSNORE";
static const char *const kDonkeyIdle = "ANIM_GUMMYSLEEP_DONKEYIDLE";
static const char *const kDonkeyBlink = "ANIM_GUMMYSLEEP_DONKEYBLINK";

// 0x4102C0's table: by level, three tunes, each with its whistle, the bells'
// notes (+0xF4-+0x104) and its length (+0xE0).
struct Tune {
	const char *whistle;
	const char *tune;
	int notes[5];
	uint length;
};

static const Tune kTunes[3][3] = {
	{ { "Gummy.Whistle1a", "87656", { 4, 5, 6, 7, 8 }, 5 },
	  { "Gummy.Whistle2a", "77456", { 4, 5, 6, 7, 8 }, 5 },
	  { "Gummy.Whistle3a", "88645", { 4, 5, 6, 7, 8 }, 5 } },
	{ { "Gummy.Whistle1b", "7844345", { 3, 4, 5, 7, 8 }, 7 },
	  { "Gummy.Whistle2b", "6665565", { 4, 5, 6, 7, 8 }, 7 },
	  { "Gummy.Whistle3b", "4566554", { 4, 5, 6, 7, 8 }, 7 } },
	{ { "Gummy.Whistle1c", "76543454", { 3, 4, 5, 6, 7 }, 8 },
	  { "Gummy.Whistle2c", "41414564", { 1, 2, 4, 5, 6 }, 8 },
	  { "Gummy.Whistle3c", "456889864", { 4, 5, 6, 8, 9 }, 9 } }
};

void GummyRoom::chooseTune(int level, int k) {
	// Level 0, 1, or anything else as 2.
	const Tune &t = kTunes[level == 0 ? 0 : level == 1 ? 1 : 2][k];
	_whistle = t.whistle;
	_tune = t.tune;
	for (uint i = 0; i < 5; i++)
		_notes[i] = t.notes[i];
	_length = t.length;
}

void GummyRoom::playSound(const Common::String &name, bool stopFirst) {
	Sound &sound = *_engine.getSound();
	if (stopFirst)
		sound.stopClip(name);
	const bool found = sound.play(name);
	_lastSound = Common::String::format("%s found %d", name.c_str(), found ? 1 : 0);
}

void GummyRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x410760. The bells clickables without the ghost (0x408DC0 with 0, -1),
	// bell 1 at depth 4 and the others at 10, each with 0x410A00 as its
	// callback and kept by its digit. Gummy a requires object (0x408C80 with
	// -1) on his snore, stopped, hit by his cels. The donkey a sprite at
	// depth 5, its idle played once.
	if (object == "OBJ_GUMMYSLEEP_BELLS") {
		claim.build = Room::Claim::kClickable;
		claim.depth = 10;
	} else if (object == "OBJ_GUMMYSLEEP_GUMMY") {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.anim = kSnore;
	} else if (object == "OBJ_GUMMYSLEEP_DONKEY") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 5;
	}
}

void GummyRoom::enter(const Common::String &from) {
	// 0x4102C0, after the base entry. The toolbar is off here (0x407CC0, in
	// the toolbar's hidden list). Unless back from the quit screen, a tune is
	// drawn for the level (rand() % 3). Then Gummy whistles again, the bells
	// play the whole tune, and the timers start.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	_lastSound.clear();
	_demoLog.clear();
	for (uint i = 0; i < 5; i++)
		_bells[i] = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		if (obj == "OBJ_GUMMYSLEEP_BELLS") {
			const Common::String &anim = room.getAnim(s);
			const int i = anim.empty() ? -1 : anim.lastChar() - '1';
			if (i >= 0 && i < 5) {
				_bells[i] = s;
				if (i == 0)
					room.setDepth(s, 4);
			}
		} else if (obj == "OBJ_GUMMYSLEEP_GUMMY") {
			_gummy = s;
		} else if (obj == "OBJ_GUMMYSLEEP_DONKEY") {
			_donkey = s;
		}
	}
	if (_gummy >= 0) {
		room.startAnim(_gummy, kSnore, 0, -1);
		room.setHitType(_gummy, 1);
	}
	if (_donkey >= 0)
		room.startAnim(_donkey, kDonkeyIdle, 3, -1);
	if (from != "ROOM_Quit")
		chooseTune(_engine.getDifficulty(), _engine.getRandomNumber(2));
	_replay = true;
	_demo = false;
	_current.clear();
	_index = 0;
	_exitCountdown = kNever;
	_demoCount = _length;
	_blink = (_engine.getRandomNumber(7) + 3) * 36;
	_snore = 108;
	_sceneDone = false;
	_scenePlaying = false;
	_leaving = false;
	for (uint b = 0; b < 5; b++)
		_pending[b] = 0;
	_state = script.getMemory(kGummyRoom).openingPlayed ? 3 : 0;
}

void GummyRoom::event(const Common::String &object, bool success) {
	// 0x4109B0: Gummy clicked once the bells are free: he whistles the tune
	// again.
	if (_state == 3 && !_replay && !_demo && object == "OBJ_GUMMYSLEEP_GUMMY") {
		_index = 0;
		_replay = true;
	}
}

void GummyRoom::clicked(uint sprite) {
	// 0x410A00, a bell's callback, unless the bells are playing the tune:
	// its note (Gummy.Bell<note>). The tune's next note: on to the one after;
	// the last, the puzzle solved (GummyPuzzle, game +0x274), Gummy met, and
	// the station in 72 ticks. A wrong one: back to the start, and Gummy
	// whistles it again.
	if (_demo)
		return;
	Room &room = *_engine.getRoom();
	const Common::String &anim = room.getAnim(sprite);
	const int i = anim.empty() ? -1 : anim.lastChar() - '1';
	if (i < 0 || i >= 5)
		return;
	playSound(Common::String::format("Gummy.Bell%d", _notes[i]), true);
	if (_index < _tune.size() && _tune[_index] == '0' + _notes[i]) {
		_index++;
		if (_index == _length) {
			_engine.setGameFlag("GummyPuzzle", true);
			_engine.getInventory()->meetSuspect("GUMMY");
			_exitCountdown = 72;
			debugC(1, kDebugScript, "gummy: the tune rung at tick %u", _engine.getScript()->getTick());
		}
	} else {
		if (_demoCount <= _index)
			_demoCount = _index + 1;
		_index = 0;
		_replay = true;
	}
}

void GummyRoom::tick() {
	// 0x40FD10, after the base tick.
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	// 0x434CE0: a sound that isn't playing, or none, is over.
	auto over = [&](const Common::String &name) { return name.empty() || !sound.isClipPlaying(name); };

	// 220B2_'s done flag, set by the base tick as it ends.
	if (_scenePlaying && script.getScene() != "Gummy_Puzzle.220B2_") {
		_scenePlaying = false;
		_sceneDone = true;
	}

	switch (_state) {
	case 0:
		// The opening played: Gummy whistles the tune.
		if (script.getMemory(kGummyRoom).openingPlayed) {
			_state = 1;
			playSound(_whistle, false);
			_snore = 360;
		}
		break;
	case 1:
		// The whistle over: 220B2_, held, with +0x114 as its done flag.
		if (over(_whistle)) {
			_state = 2;
			if (script.startScene("Gummy_Puzzle.220B2_", true))
				_scenePlaying = true;
		}
		break;
	case 2:
		if (_sceneDone)
			_state = 3;
		break;
	case 3: {
		if (over(_current)) {
			if (_replay) {
				// Gummy whistles it again; the bells then play it back.
				_current = _whistle;
				playSound(_whistle, false);
				_snore = 360;
				_replay = false;
				_demo = true;
				_index = 0;
				_demoLog.clear();
			} else if (_demo) {
				if (_index == _demoCount) {
					// Played back: the bells are free.
					_index = 0;
					if (_length < _demoCount)
						_demoCount = _length;
					_demo = false;
				} else {
					// The next note: its bell sound, and each bell with that note
					// rings (0x401840, the clickable's +0x80).
					const int note = _tune[_index] - '0';
					_current = Common::String::format("Gummy.Bell%d", note);
					playSound(_current, true);
					_demoLog += Common::String::format("%s%s bell", _demoLog.empty() ? "" : "; ", _current.c_str());
					for (uint b = 0; b < 5; b++) {
						if (_notes[b] == note && _bells[b] >= 0) {
							_pending[b]++;
							_demoLog += Common::String::format(" %u", b + 1);
						}
					}
					_index++;
				}
			}
		}
		// Rung: the station when the countdown runs out (the room's exit
		// handler, the base's here).
		if (_exitCountdown < kNever) {
			const int v = _exitCountdown--;
			if (v < 0 && !_leaving) {
				_leaving = true;
				RoomScript::Exit exit;
				exit.destination = "ROOM_P09_Railroad_Station";
				exit.movie = -1;
				script.takeExit(exit);
			}
		}
		// Gummy snores every 108 ticks.
		const int s = _snore--;
		if (s < 0) {
			_snore = 108;
			playSound("Gummy.Snore", false);
			if (_gummy >= 0)
				room.startAnim(_gummy, kSnore, 3, -1);
		}
		// The donkey blinks, or idles, every 3 to 10 seconds; after a blink,
		// its idle, stopped.
		const int b = _blink--;
		if (b < 0) {
			_blink = (_engine.getRandomNumber(7) + 3) * 36;
			if (_donkey >= 0)
				room.startAnim(_donkey, _engine.getRandomNumber(1) == 0 ? kDonkeyBlink : kDonkeyIdle, 3, -1);
		}
		if (_donkey >= 0 && room.getMode(_donkey) == 0 && room.getAnim(_donkey) == kDonkeyBlink)
			room.startAnim(_donkey, kDonkeyIdle, 0, -1);
		break;
	}
	default:
		break;
	}

	// 0x401440: a bell with a ring waiting, and free, rings on the next tick
	// as if clicked.
	for (uint b = 0; b < 5; b++) {
		if (_pending[b] && _bells[b] >= 0 && !script.isClickablePlaying(_bells[b])) {
			script.clickClickable(_bells[b]);
			_pending[b]--;
		}
	}

	// While he whistles, Gummy's step follows the whistle's lip sync
	// (0x434D40), stopped on it.
	if (_gummy >= 0 && !_whistle.empty() && !over(_whistle)) {
		uint shape;
		if (sound.getLipShape(_whistle, shape) && shape != 0)
			room.startAnim(_gummy, room.getAnim(_gummy), 0, (int)shape - 1);
	}
}

RoomClass *createGummyRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kGummyRoom)
		return new GummyRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
