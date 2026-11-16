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

#include "scooby/character.h"
#include "scooby/detection.h"
#include "scooby/room.h"
#include "scooby/sound.h"

namespace Scooby {

// Sequence modes (0x430AA0): 0 stopped, 1 loop forward, 3 once forward.
static const int kModeStopped = 0;
static const int kModeLoop = 1;
static const int kModeOnce = 3;

// A walk starting with D or U moves the character this far in front or
// behind (0x405D90).
static const int kWalkDepth = 50;

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

void Character::setState(Room &room, Sound &sound, int state) {
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
		// The line being spoken stops; each file has one handle, so the
		// scared line replaces it on the speech channel.
		anim = Common::String::format("ANIM_%s%sSTAY_SCARED", _name.c_str(), _set.c_str());
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
}

void Character::update(Room &room, Sound &sound) {
	// Random idles (the countdown at +0x5C) are not implemented.
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

void Character::say(Room &room, Sound &sound, const Common::String &clip) {
	if (_state != kStateIdle) {
		_waiting = clip;
		return;
	}
	_clip = clip;
	setState(room, sound, kStateTalk);
	sound.play(clip);
}

bool Character::isBusy() const {
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

} // End of namespace Scooby
