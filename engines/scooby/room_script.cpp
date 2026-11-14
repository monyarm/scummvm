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

#include "scooby/detection.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/sound.h"

namespace Scooby {

// The exit keys, in Gang order (0x401850 reads fred=, velma=, daphne=,
// shaggy= and scooby=; never character= or direction=).
static const char *const kGangNames[] = { "FRED", "VELMA", "DAPHNE", "SHAGGY", "SCOOBY" };
static const char *const kGangKeys[] = { "fred", "velma", "daphne", "shaggy", "scooby" };

// The walk-off starts the others in this order (0x40BE20).
static const RoomScript::Gang kFollowOrder[] = {
	RoomScript::kFred, RoomScript::kDaphne, RoomScript::kVelma, RoomScript::kShaggy, RoomScript::kScooby
};

// After the room's characters comes an invisible narrator (0x409670).
static const char *const kNarrator = "OBJ_NARRATOR_A";

// A scene runs out of lines long before this.
static const int kMaxLines = 99;

static Common::String getIniString(const Common::INIFile &ini, const Common::String &section, const char *key) {
	Common::String value;
	if (!ini.getKey(key, section, value))
		return Common::String();
	value.trim();
	return value;
}

RoomScript::RoomScript(Room &room, Sound &sound, const Common::INIFile &objectIni) :
	_room(room), _sound(sound), _ini(objectIni), _line(0), _speaker(-1), _exitParked(false), _leader(-1),
	_busy(false), _done(false), _tick(0) {
	_exit.movie = -1;
	for (uint g = 0; g < kGangCount; g++)
		_lengths[g] = -1;
}

void RoomScript::enter() {
	// The constructor's switch to IDLE_MASTER (0x4048E0) is skipped: 258 of
	// the 261 character placements already play it, and the other three are
	// set pieces their rooms' own code drives.
	_characters.clear();
	const Common::Array<uint> &sprites = _room.getCharacters();
	for (uint i = 0; i < sprites.size(); i++)
		_characters.push_back(Character(_room.getObjectName(sprites[i]), sprites[i]));
	_characters.push_back(Character(kNarrator, -1));

	_scene.clear();
	_line = 0;
	_speaker = -1;
	_exitParked = false;
	_exit = Exit();
	_exit.movie = -1;
	_leader = -1;
	for (uint g = 0; g < kGangCount; g++)
		_lengths[g] = -1;
	_busy = false;
	_done = false;
	_tick = 0;
	_walkLog.clear();
}

RoomScript::Exit RoomScript::readExit(const Common::String &object) const {
	Exit e;
	e.object = object;
	e.destination = getIniString(_ini, object, "destinationroom");
	e.scene = getIniString(_ini, object, "scene");
	const Common::String movie = getIniString(_ini, object, "movie");
	e.movie = movie.empty() ? -1 : atoi(movie.c_str());
	for (uint g = 0; g < kGangCount; g++)
		e.directions[g] = getIniString(_ini, object, kGangKeys[g]);
	return e;
}

Character *RoomScript::findCharacter(const char *name) {
	// 0x40A800: the first character with the name.
	for (uint i = 0; i < _characters.size(); i++) {
		if (_characters[i].getName().equalsIgnoreCase(name))
			return &_characters[i];
	}
	return nullptr;
}

void RoomScript::startScene(const Common::String &scene) {
	if (scene.empty() || !_scene.empty() || _exitParked)
		return;
	_scene = scene;
	_line = 0;
	_speaker = -1;
	_busy = true;
	debugC(1, kDebugScript, "scene %s starts", scene.c_str());
}

void RoomScript::startExit(const Exit &exit) {
	// The cursor fades out and input stops (0x435150, game +0x24).
	_busy = true;
	startScene(exit.scene);
	_exit = exit;
	_exitParked = true;
	_leader = -1;
	_done = false;
	debugC(1, kDebugScript, "exit %s: scene '%s', then %s", exit.object.c_str(), exit.scene.c_str(), exit.destination.c_str());
}

void RoomScript::tick() {
	// The sprite tree ticks first (0x431130): each character's update runs
	// after its player. Then the room's own tick (0x40B490).
	for (uint i = 0; i < _characters.size(); i++)
		_characters[i].update(_room, _sound);
	if (!_scene.empty())
		tickScene();
	if (_scene.empty() && _exitParked && !_done)
		tickExit();
	_tick++;
}

void RoomScript::tickScene() {
	// 0x40C810: wait for the speaker, then look for the next line.
	if (_speaker >= 0 && _characters[_speaker].isBusy())
		return;
	_line++;
	if (_line <= kMaxLines) {
		for (uint i = 0; i < _characters.size(); i++) {
			const Common::String clip = Common::String::format("%s%d_%s", _scene.c_str(), _line, _characters[i].getName().c_str());
			if (_sound.findClip(clip)) {
				_speaker = i;
				debugC(1, kDebugScript, "scene %s line %d: %s says %s at tick %u", _scene.c_str(), _line,
				       _characters[i].getName().c_str(), clip.c_str(), _tick);
				_characters[i].say(_room, _sound, clip);
				return;
			}
		}
	}

	debugC(1, kDebugScript, "scene %s over after %d lines at tick %u", _scene.c_str(), _line - 1, _tick);
	_scene.clear();
	_speaker = -1;
	if (!_exitParked)
		_busy = false;
}

void RoomScript::tickExit() {
	// 0x40BE20: the longest walk leads; the others start so that they all
	// end with it, and the room changes on the leader's last step.
	if (_leader < 0) {
		int longest = 0;
		for (uint g = 0; g < kGangCount; g++) {
			_lengths[g] = -1;
			Character *c = findCharacter(kGangNames[g]);
			if (!c || c->getSprite() < 0)
				continue;
			Common::String obj, anim;
			c->getWalk(_exit.directions[g], obj, anim);
			_lengths[g] = _room.getSequenceSteps(obj, anim);
			if (_lengths[g] != -1 && longest < _lengths[g]) {
				_leader = g;
				longest = _lengths[g];
			}
		}
	}

	if (_leader >= 0) {
		Character *leader = findCharacter(kGangNames[_leader]);
		if (leader->getState() != Character::kStateWalk) {
			leader->walk(_room, _sound, _exit.directions[_leader]);
			_walkLog += Common::String::format("%s %s %d@%u ", kGangNames[_leader], _exit.directions[_leader].c_str(), _lengths[_leader], _tick);
			debugC(1, kDebugScript, "%s leads: %s, %d steps, at tick %u", kGangNames[_leader], _exit.directions[_leader].c_str(), _lengths[_leader], _tick);
		}
		const int left = (int)_room.getStepCount(leader->getSprite()) - (int)_room.getStep(leader->getSprite());
		if (left != 1) {
			for (uint k = 0; k < ARRAYSIZE(kFollowOrder); k++) {
				const Gang g = kFollowOrder[k];
				Character *c = findCharacter(kGangNames[g]);
				if (left - 1 <= _lengths[g] && c && c->getState() != Character::kStateWalk) {
					c->walk(_room, _sound, _exit.directions[g]);
					_walkLog += Common::String::format("%s %s %d@%u ", kGangNames[g], _exit.directions[g].c_str(), _lengths[g], _tick);
					debugC(1, kDebugScript, "%s follows: %s, %d steps, at tick %u", kGangNames[g], _exit.directions[g].c_str(), _lengths[g], _tick);
				}
			}
			return;
		}
	}

	debugC(1, kDebugScript, "exit %s: going to %s at tick %u", _exit.object.c_str(), _exit.destination.c_str(), _tick);
	_done = true;
}

bool RoomScript::getNextRoom(Common::String &room, int &movie) const {
	if (!_done)
		return false;
	room = _exit.destination;
	movie = _exit.movie;
	return true;
}

Common::String RoomScript::describeCharacters() const {
	Common::String s;
	for (uint i = 0; i < _characters.size(); i++) {
		if (i)
			s += ' ';
		s += _characters[i].getName();
	}
	return s;
}

Common::String RoomScript::describeScene(const Common::String &scene) const {
	Common::String s;
	if (scene.empty())
		return s;
	for (int line = 1; line <= kMaxLines; line++) {
		bool found = false;
		for (uint i = 0; i < _characters.size() && !found; i++) {
			if (_sound.findClip(Common::String::format("%s%d_%s", scene.c_str(), line, _characters[i].getName().c_str()))) {
				s += Common::String::format("%s%d:%s", s.empty() ? "" : " ", line, _characters[i].getName().c_str());
				found = true;
			}
		}
		if (!found)
			break;
	}
	return s;
}

} // End of namespace Scooby
