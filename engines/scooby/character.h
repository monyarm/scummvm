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

#ifndef SCOOBY_CHARACTER_H
#define SCOOBY_CHARACTER_H

#include "common/str.h"

namespace Scooby {

class Room;
class Sound;

/**
 * A Gen 1 character (Scooby.exe 0x4048E0): a placement such as OBJ_FRED_A,
 * named FRED with the set _A, or the room's invisible narrator. Its state
 * picks the sequence it plays and, when that changes, the object holding it
 * (0x405190). Only the states the exit transition, the ghost and the story
 * rooms need are here.
 */
class Character {
public:
	enum State {
		kStateWalk = 0, // walk off, or turn in place: once, then stay
		kStateEndWalk = 1, // ENDWALK_<direction>: walk in, once, then idle
		kStateIdle = 6, // IDLE_MASTER, looping
		kStateStay = 7, // keep the last step
		kStateTalk = 8, // TALKS, its step set by lip sync
		kStateAct = 0xF, // ANIM_<NAME><set><action> once, then idle (0x4061F0)
		kStateScared = 0x11, // SCARED once, with Global.Scared.<NAME>, then idle
		kStateStayScared = 0x12 // STAY_SCARED once, with Global.Scared.<NAME>: the ghost appears
	};

	/** @p sprite is the placement's sprite in the room, or -1 for the narrator. */
	Character(const Common::String &object, int sprite);

	const Common::String &getName() const { return _name; }
	int getSprite() const { return _sprite; }
	int getState() const { return _state; }

	void setState(Room &room, Sound &sound, int state);

	/** Once a tick, after the sprites' players (vtable +0x0C, 0x404B70). */
	void update(Room &room, Sound &sound);

	/**
	 * Speak a clip (0x404DB0): at once if idle, else when the character is
	 * next idle.
	 */
	void say(Room &room, Sound &sound, const Common::String &clip);

	/** Talking, or with a line waiting (0x406060). */
	bool isBusy() const;
	/** The line being spoken, or last spoken (+0x7C). */
	const Common::String &getClipName() const { return _clip; }

	/** Play ANIM_<NAME><set>@p action once, then go idle (0x4061F0, state 0xF). */
	void act(Room &room, Sound &sound, const Common::String &action);
	/** The sequence act() plays. */
	Common::String getActAnim(const Common::String &action) const;
	const Common::String &getSet() const { return _set; }

	/**
	 * The walk or turn for @p direction: its object and sequence names
	 * (0x404EA0). A direction starting with TURN turns in place.
	 */
	void getWalk(const Common::String &direction, Common::String &obj, Common::String &anim) const;

	/** Start the walk or turn for @p direction (0x405D90). */
	void walk(Room &room, Sound &sound, const Common::String &direction);

	/**
	 * Walk in from @p direction (0x405E60, 0x405EE0, 0x405FE0): ENDWALK, 50
	 * behind the character's depth until the next state.
	 */
	void walkIn(Room &room, Sound &sound, const Common::String &direction);

private:
	Common::String _name;
	Common::String _set;
	int _sprite;
	int _state;
	int _next;           // +0x78: the state after the sequence ends
	bool _toNext;        // +0x70: go there when the sequence stops
	Common::String _clip;    // +0x7C: the line being spoken
	Common::String _waiting; // +0x84: a line to speak when next idle
	Common::String _direction; // +0xA4
	Common::String _action;    // +0x8C
	Common::String _inFrom;    // +0xAC: the walk-in's direction
	int _depth = 0;            // +0x9C: the depth to go back to after it
};

} // End of namespace Scooby

#endif // SCOOBY_CHARACTER_H
