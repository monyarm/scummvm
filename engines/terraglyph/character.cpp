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

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

// Sequence modes (0x430AA0): 0 stopped, 1 loop forward, 3 once forward.
static const int kModeStopped = 0;
static const int kModeLoop = 1;
static const int kModeOnce = 3;

// A walk starting with D or U moves the character this far in front or
// behind (0x405D90).
static const int kWalkDepth = 50;

// The idle countdown's most (+0x98): the constructor's 3600 ticks (0x4048E0),
// Scooby's 108 (0x4091FF) and the narrator's 0x2F7600 (0x409670). The
// countdown itself is (rand() % 15 + 10) seconds, at most that.
static const int kIdleCap = 3600;
static const int kScoobyIdleCap = 108;
static const int kNarratorIdleCap = 0x2F7600;

Character::Character(const Common::String &object, int sprite) :
	_sprite(sprite), _state(kStateIdle), _next(kStateIdle), _toNext(false) {
	// OBJ_<NAME><set>, the set being "_" and one character when present.
	Common::String stem = object;
	if (stem.size() > 2 && stem[stem.size() - 2] == '_') {
		_set = Common::String(stem.c_str() + stem.size() - 2);
		stem = Common::String(stem.c_str(), stem.size() - 2);
	}
	_name = stem.size() > 4 ? Common::String(stem.c_str() + 4) : Common::String();
}

void Character::getWalk(const Common::String &direction, Common::String &obj, Common::String &anim) const {
	if (direction.hasPrefix("TURN")) {
		obj = Common::String::format("OBJ_%s%s", _name.c_str(), _set.c_str());
		anim = Common::String::format("ANIM_%s%s%s", _name.c_str(), _set.c_str(), direction.c_str());
	} else {
		obj = Common::String::format("OBJ_%s_STARTWALK_%s%s", _name.c_str(), direction.c_str(), _set.c_str());
		anim = Common::String::format("ANIM_%s_STARTWALK_%s%s1", _name.c_str(), direction.c_str(), _set.c_str());
	}
}

void Character::startIdles(RoomScript *script) {
	// The constructor's switch to IDLE_MASTER ends with the countdown
	// (0x405190); the placement then sets Scooby's, and 0x409670 the
	// narrator's, countdown and cap alike.
	_script = script;
	_idleCap = kIdleCap;
	_countdown = kIdleCap;
	if (_script && _script->getIdleRandom())
		_countdown = MIN<int>((idleDraw(15) + 10) * Room::kTicksPerSecond, _idleCap);
	if (_sprite < 0)
		_idleCap = _countdown = kNarratorIdleCap;
	else if (_name == "SCOOBY")
		_idleCap = _countdown = kScoobyIdleCap;
}

uint Character::idleDraw(uint n) {
	Common::RandomSource *source = _script ? _script->getIdleRandom() : nullptr;
	return source && n ? source->getRandomNumber(n - 1) : 0;
}

int Character::findIdle(Room &room, const Common::String &prefix, Common::String &anim) {
	// 0x405CE0
	uint r = 9;
	do {
		r = idleDraw(r);
		anim = prefix + (char)('1' + r);
		if (room.hasAnim(anim)) {
			debugC(2, kDebugScript, "Found idle %s", anim.c_str());
			return r + 1;
		}
	} while (r != 0);
	debugC(2, kDebugScript, "Failed to find idle %s", anim.c_str());
	anim.clear();
	return 0;
}

void Character::setState(Room &room, Sound &sound, int state) {
	// 0x405190: the first idle is BLINK two times in three.
	if (state == kStateIdlePrimary && idleDraw(3) != 0)
		state = kStateBlink;
	_idleSound.clear();

	// Idle with a line waiting speaks it instead.
	if (state == kStateIdle && !_waiting.empty()) {
		_clip = _waiting;
		_waiting.clear();
		sound.play(_clip);
		state = kStateTalk;
	}

	// Scared again while the fright is still in its first two steps: no
	// change (0x405190 state 0x11).
	if (state == kStateScared && _sprite >= 0 &&
	    (_state == kStateStayScared || (_state == kStateScared && room.getStep(_sprite) < 2)))
		return;
	// Leaving the walk-in puts the character back at its depth.
	if (_state == kStateEndWalk && _sprite >= 0)
		room.setDepth(_sprite, _depth);

	Common::String obj = Common::String::format("OBJ_%s%s", _name.c_str(), _set.c_str());
	Common::String anim;
	int mode = kModeOnce;
	switch (state) {
	case kStateWalk:
		getWalk(_direction, obj, anim);
		_next = kStateStay;
		break;
	case kStateEndWalk:
		obj = Common::String::format("OBJ_%s_ENDWALK_%s%s", _name.c_str(), _inFrom.c_str(), _set.c_str());
		anim = Common::String::format("ANIM_%s_ENDWALK_%s%s1", _name.c_str(), _inFrom.c_str(), _set.c_str());
		_next = kStateIdle;
		break;
	case kStateScared:
		// The line being spoken stops, and Global.Scared.<NAME> plays.
		anim = Common::String::format("ANIM_%s%sSCARED", _name.c_str(), _set.c_str());
		if (!_clip.empty())
			sound.stopClip(_clip);
		_clip = Common::String::format("Global.Scared.%s", _name.c_str());
		sound.stopClip(_clip);
		sound.play(_clip);
		_next = kStateIdle;
		break;
	case kStateIdlePrimary:
	case kStateIdleSecondary: {
		// IDLE_P<n> or IDLE_S<n> once, n as 0x405CE0 finds it; none found:
		// nothing changes, and the countdown stays run out. Its line plays
		// if there is one, unless the character is hidden (+0x14).
		const char letter = state == kStateIdlePrimary ? 'P' : 'S';
		if (!findIdle(room, Common::String::format("ANIM_%s%sIDLE_%c", _name.c_str(), _set.c_str(), letter), anim))
			return;
		_next = kStateIdle;
		const Common::String line = Common::String::format("Global.Idle.%s_%c%c%s", _name.c_str(), letter, anim.lastChar(), _set.c_str());
		if (_sprite >= 0 && !room.isHidden(_sprite) && sound.findClip(line)) {
			sound.play(line);
			_idleSound = line;
		}
		break;
	}
	case kStateIdleLine: {
		// The room names the line (vtable +0x28). Not on the disc: idle.
		// Else it is said with the speech lock, from idle, and this state
		// is never kept.
		const Common::String line = _script ? _script->getIdleLine(_name) : Common::String();
		if (line.empty() || !sound.findClip(line)) {
			if (_state != kStateIdle)
				setState(room, sound, kStateIdle);
			return;
		}
		_state = kStateIdle;
		say(room, sound, line, true);
		return;
	}
	case kStateBlink:
		anim = Common::String::format("ANIM_%s%sBLINK", _name.c_str(), _set.c_str());
		_next = kStateIdle;
		break;
	case kStateIdle:
		anim = Common::String::format("ANIM_%s%sIDLE_MASTER", _name.c_str(), _set.c_str());
		mode = kModeLoop;
		_next = kStateIdle;
		break;
	case kStateTalk:
		anim = Common::String::format("ANIM_%s%sTALKS", _name.c_str(), _set.c_str());
		mode = kModeStopped;
		_next = kStateIdle;
		break;
	case kStateAct:
		anim = getActAnim(_action);
		_next = kStateIdle;
		break;
	case kStateSpecial:
		// 0x405190 state 0xD: the object in +0x8C, its sequence the name
		// without OBJ_, with 1 after it.
		obj = _action;
		anim = Common::String("ANIM_") + Common::String(_action.c_str() + MIN<uint>(4, _action.size())) + "1";
		_next = kStateIdle;
		break;
	case kStateSpecialLoop:
		// 0x405190 state 0xE: as 0xD, looping, until another state.
		obj = _action;
		anim = Common::String("ANIM_") + Common::String(_action.c_str() + MIN<uint>(4, _action.size())) + "1";
		mode = kModeLoop;
		_next = kStateSpecialLoop;
		break;
	case kStateActLoop:
		// 0x405190 state 0x10: the action, looping, until another state.
		anim = getActAnim(_action);
		mode = kModeLoop;
		_next = kStateActLoop;
		break;
	case kStateStayScared:
		// The line being spoken stops (0x434CB0 on +0x7C), and
		// Global.Scared.<NAME> plays.
		anim = Common::String::format("ANIM_%s%sSTAY_SCARED", _name.c_str(), _set.c_str());
		if (!_clip.empty())
			sound.stopClip(_clip);
		_clip = Common::String::format("Global.Scared.%s", _name.c_str());
		sound.play(_clip);
		_next = kStateStay;
		break;
	default:
		_next = kStateStay;
		break;
	}
	if (!anim.empty() && _sprite >= 0)
		room.setSequence(_sprite, obj, anim, mode, -1);

	_state = state;
	_toNext = state != kStateIdle && state != kStateTalk && state != kStateActLoop && state != kStateSpecialLoop;
	// Every change that gets here, 7 not among them, sets the next idle's
	// countdown: (rand() % 15 + 10) seconds, at most the cap.
	if (state != kStateStay && _script && _script->getIdleRandom()) {
		_countdown = MIN<int>((idleDraw(15) + 10) * Room::kTicksPerSecond, _idleCap);
		_script->logState(*this);
	}
}

void Character::update(Room &room, Sound &sound) {
	// 0x404B70: a line said with the speech lock lets it go when it is over.
	if (_state != kStateTalk && _holdsLock) {
		_holdsLock = false;
		if (_script)
			_script->releaseSpeechLock();
	}

	// The countdown runs out while idle: an idle (state 2).
	const bool idles = _script && _script->getIdleRandom();
	_countdown--;
	if (idles && _countdown < 1 && _state == kStateIdle) {
		setState(room, sound, kStateIdlePrimary);
	} else {
		if (_toNext && (_sprite < 0 || room.isStopped(_sprite)))
			setState(room, sound, _next);

		if (_state == kStateTalk) {
			uint shape;
			uint32 index;
			if (sound.getLipShape(_clip, shape, &index)) {
				debugC(2, kDebugScript, "%s lip sync: %s byte %u, shape %u", _name.c_str(), _clip.c_str(), index, shape);
				// A shape of 0 keeps the step.
				if (shape && _sprite >= 0)
					room.setStep(_sprite, shape - 1);
			} else if (!sound.isClipPlaying(_clip)) {
				setState(room, sound, kStateIdle);
			}
		}
	}

	// Clicked with the arrow while idle: the second idle or the room's line,
	// one in two; the second idle whenever the speech lock is held, if there
	// is one.
	if (_clicked) {
		_clicked = false;
		if (idles && _state == kStateIdle) {
			uint r = idleDraw(2);
			if (_script->isSpeechLocked())
				r = 0;
			const bool second = r == 0 && room.hasAnim(Common::String::format("ANIM_%s%sIDLE_S1", _name.c_str(), _set.c_str()));
			setState(room, sound, second ? kStateIdleSecondary : kStateIdleLine);
		}
	}
}

void Character::say(Room &room, Sound &sound, const Common::String &clip, bool takeLock) {
	// 0x404DB0: a line that asks for the speech lock waits for nobody.
	if (takeLock && (!_script || !_script->acquireSpeechLock()))
		return;
	if (_state != kStateIdle) {
		_waiting = clip;
		_holdsLock = takeLock;
		return;
	}
	_clip = clip;
	setState(room, sound, kStateTalk);
	sound.play(clip);
	// The room's idle timer starts again (+0xB4 of the room). The fork does
	// it only for the lines that take the lock; a scene starts it again
	// every tick it plays.
	if (takeLock)
		_script->restartIdle();
	_holdsLock = takeLock;
}

bool Character::isBusy() const {
	// 0x406060
	if (_name == "SCOOBY" && _state == kStateIdleSecondary)
		return true;
	return _state == kStateTalk || !_waiting.empty();
}

Common::String Character::getActAnim(const Common::String &action) const {
	// 0x405190 state 0xF: "ANIM_%s%s%s" of the name, the set and the action.
	return Common::String::format("ANIM_%s%s%s", _name.c_str(), _set.c_str(), action.c_str());
}

void Character::special(Room &room, Sound &sound, const Common::String &object) {
	// 0x406130
	_action = object;
	setState(room, sound, kStateSpecial);
}

void Character::specialLoop(Room &room, Sound &sound, const Common::String &object) {
	// 0x406190
	_action = object;
	setState(room, sound, kStateSpecialLoop);
}

void Character::setSet(char digit) {
	// 0x406230
	_set = digit ? Common::String::format("_%c", digit) : Common::String();
}

void Character::actLoop(Room &room, Sound &sound, const Common::String &action) {
	// 0x406210
	_action = action;
	setState(room, sound, kStateActLoop);
}

void Character::act(Room &room, Sound &sound, const Common::String &action) {
	_action = action;
	setState(room, sound, kStateAct);
}

void Character::walkIn(Room &room, Sound &sound, const Common::String &direction) {
	// 0x405E60 to 0x405FE0: keep the depth (+0x9C), go 50 behind it from
	// U, UL or UR, 50 in front from DL or DR, at its depth from L or R
	// (0x405F60, 0x405FA0), then state 1.
	_inFrom = direction;
	if (_sprite >= 0) {
		_depth = room.getDepth(_sprite);
		const int offset = direction == "L" || direction == "R" ? 0 : direction.hasPrefix("D") ? kWalkDepth : -kWalkDepth;
		room.setDepth(_sprite, _depth + offset);
	}
	setState(room, sound, kStateEndWalk);
}

void Character::walk(Room &room, Sound &sound, const Common::String &direction) {
	_direction = direction;
	setState(room, sound, kStateWalk);
	if (_sprite < 0)
		return;
	if (direction.hasPrefix("D"))
		room.setDepth(_sprite, room.getDepth(_sprite) + kWalkDepth);
	else if (direction.hasPrefix("U"))
		room.setDepth(_sprite, room.getDepth(_sprite) - kWalkDepth);
}

} // End of namespace Terraglyph
