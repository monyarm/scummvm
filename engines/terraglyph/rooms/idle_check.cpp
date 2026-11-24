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
#include "common/random.h"

#include "terraglyph/formats/art_archive.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/rooms/phantomknight/drawbridge.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/class.h"
#include "terraglyph/rooms/script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkIdles() {
	// The characters' idles (states 2-5), in two rooms of each title loaded
	// as the base class with no opening, from a seeded random source of
	// their own: every state change with its tick, sequence, line and next
	// countdown, and clicks with the arrow at set ticks. A line a character
	// starts saying is stopped at once, so its talk ends on the next tick.
	// Then the drawbridge's idle line (room vtable +0x28, 0x4272D0).
	_checking = true;
	Common::RandomSource idleRandom("TerraglyphIdleCheck");
	Common::Array<Common::String> log;
	_script->setStateLog(&log);
	_sound->stopEffects();

	auto hasRoom = [&](const char *room) {
		for (uint i = 0; i < _art->getRoomCount(); i++) {
			if (_art->getRoomName(i) == room)
				return true;
		}
		return false;
	};
	// The room as the base class builds it, without an opening.
	auto loadBare = [&](const char *room, uint32 seed) {
		idleRandom.setSeed(seed);
		_script->setIdleRandom(&idleRandom);
		_script->releaseSpeechLock();
		_currentRoom->setPlacementHook(nullptr);
		if (!_currentRoom->load(room))
			return false;
		_script->enter();
		_script->setHooks(nullptr);
		_script->setup("", 0, "");
		return true;
	};
	auto run = [&](const char *room, uint32 seed, uint ticks, const char *const *clicks, const uint *clickTicks, uint clickCount) {
		const Common::String name(room + 5);
		if (!loadBare(room, seed)) {
			debugC(3, kDebugScript, "idle check %s: not loaded", name.c_str());
			return;
		}
		Common::String out;
		const Common::Array<Character> &chars = _script->getCharacters();
		for (uint i = 0; i < chars.size(); i++) {
			const int s = chars[i].getSprite();
			out += Common::String::format("%s%s cap %d countdown %d rollover %s", i ? "; " : "", chars[i].getName().c_str(), chars[i].getIdleCap(),
			                              chars[i].getIdleCountdown(), s >= 0 ? _currentRoom->getRollover(s).c_str() : "-");
		}
		debugC(3, kDebugScript, "idle check %s loaded: %s", name.c_str(), out.c_str());
		log.clear();
		for (uint t = 1; t <= ticks; t++) {
			for (uint k = 0; k < clickCount; k++) {
				if (clickTicks[k] != t)
					continue;
				Character *c = _script->getCharacter(clicks[k]);
				if (!c)
					continue;
				debugC(3, kDebugScript, "idle check %s tick %u: click %s in state %d", name.c_str(), t, clicks[k], c->getState());
				_script->clickCharacter(c->getSprite());
			}
			_currentRoom->tick();
			_script->tick();
			for (uint k = 0; k < log.size(); k++)
				debugC(3, kDebugScript, "idle check %s tick %u: %s", name.c_str(), t, log[k].c_str());
			log.clear();
			// The device: a line just started stops at once.
			for (uint i = 0; i < chars.size(); i++) {
				if (chars[i].getState() == Character::kStateTalk && _sound->isClipPlaying(chars[i].getClipName()))
					_sound->stopClip(chars[i].getClipName());
			}
		}
		out.clear();
		for (uint i = 0; i < chars.size(); i++)
			out += Common::String::format("%s%s %d countdown %d", i ? "; " : "", chars[i].getName().c_str(), chars[i].getState(), chars[i].getIdleCountdown());
		debugC(3, kDebugScript, "idle check %s end at tick %u: %s; lock %d", name.c_str(), ticks, out.c_str(), _script->isSpeechLocked() ? 1 : 0);
	};

	const bool phantom = hasRoom("ROOM_P01_Drawbridge");
	static const char *const kClicksA[] = { "VELMA", "SHAGGY", "SCOOBY", "FRED", "DAPHNE", "SCOOBY", "SHAGGY", "VELMA" };
	static const uint kTicksA[] = { 30, 31, 300, 301, 700, 900, 1100, 1300 };
	static const char *const kClicksSD[] = { "FRED", "GUMMY", "DONKEY", "VELMA" };
	static const char *const kClicksPH[] = { "FRED", "BRAD", "BRAD", "VELMA" };
	static const uint kTicksB[] = { 40, 41, 500, 520 };
	run(phantom ? "ROOM_P21_Mermaids_Cavern" : "ROOM_P18_Tunnel_Lit", 4242, 1500, kClicksA, kTicksA, ARRAYSIZE(kTicksA));
	run(phantom ? "ROOM_P14_Arena2" : "ROOM_P04_Street2", 777, 900, phantom ? kClicksPH : kClicksSD, kTicksB, ARRAYSIZE(kTicksB));

	if (phantom) {
		const char *const kDB = "ROOM_P01_Drawbridge";
		DrawbridgeRoom *db = dynamic_cast<DrawbridgeRoom *>(getRoomClass(kDB));
		if (db && loadBare(kDB, 1)) {
			const bool done = db->isPuzzleDone();
			db->setPuzzleDone(false);
			const Common::String velma0 = db->idleLine(*_script, "VELMA");
			db->setPuzzleDone(true);
			const Common::String velma1 = db->idleLine(*_script, "VELMA");
			const Common::String fred1 = db->idleLine(*_script, "FRED");
			db->setPuzzleDone(done);
			debugC(3, kDebugScript, "idle check drawbridge line: VELMA puzzle 0 %s, puzzle 1 %s; FRED puzzle 1 %s", velma0.c_str(), velma1.c_str(), fred1.c_str());
		}
	} else {
		debugC(3, kDebugScript, "idle check drawbridge line: no drawbridge");
	}

	_sound->stopEffects();
	_script->releaseSpeechLock();
	_script->setStateLog(nullptr);
	_script->setIdleRandom(nullptr);
	_checking = false;
	debugC(3, kDebugScript, "idle check: done");
}

} // End of namespace Terraglyph
