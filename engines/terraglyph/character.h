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

#ifndef TERRAGLYPH_CHARACTER_H
#define TERRAGLYPH_CHARACTER_H

#include "common/str.h"

namespace Terraglyph {

class Room;
class RoomScript;
class Sound;

/**
 * A Gen 1 character (Scooby.exe 0x4048E0): a placement such as OBJ_FRED_A,
 * named FRED with the set _A, or the room's invisible narrator. Its state
 * picks the sequence it plays and, when that changes, the object holding it
 * (0x405190). Only the states the exit transition, the ghost, the story
 * rooms and the idles need are here.
 */
class Character {
public:
	enum State {
		kStateWalk = 0, // walk off, or turn in place: once, then stay
		kStateEndWalk = 1, // ENDWALK_<direction>: walk in, once, then idle
		kStateIdlePrimary = 2, // IDLE_P<n> once, with Global.Idle.<NAME>_P<n><set>, then idle; BLINK two times in three
		kStateIdleSecondary = 3, // IDLE_S<n> once, with Global.Idle.<NAME>_S<n><set>, then idle
		kStateIdleLine = 4, // the room's idle line (room vtable +0x28), said with the speech lock
		kStateBlink = 5, // BLINK once, then idle
		kStateIdle = 6, // IDLE_MASTER, looping
		kStateStay = 7, // keep the last step
		kStateTalk = 8, // TALKS, its step set by lip sync
		kStateSpecial = 0xD, // another object's ANIM_<object>1 once, then idle (0x406130)
		kStateSpecialLoop = 0xE, // another object's ANIM_<object>1, looping, until another state (0x406190)
		kStateAct = 0xF, // ANIM_<NAME><set><action> once, then idle (0x4061F0)
		kStateActLoop = 0x10, // ANIM_<NAME><set><action>, looping (0x406210)
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
	 * next idle. With @p takeLock, only if the speech lock is free: the
	 * character lets it go once it stops talking (+0xB4), and a line said
	 * at once starts the room's idle timer again.
	 */
	void say(Room &room, Sound &sound, const Common::String &clip, bool takeLock = false);

	/** Talking, with a line waiting, or Scooby in his second idle (0x406060). */
	bool isBusy() const;

	/**
	 * The idles, for a character in @p script's room: the countdown (+0x5C)
	 * and its cap (+0x98), as the constructor and the placement set them
	 * (0x4048E0, 0x4091FF, 0x409670). They run only while the script has an
	 * idle random source.
	 */
	void startIdles(RoomScript *script);
	/** Clicked with the arrow (+0x08 and game +0x20 bit 4), seen by the next update. */
	void click() { _clicked = true; }
	int getIdleCountdown() const { return _countdown; }
	int getIdleCap() const { return _idleCap; }
	/** The Global.Idle line the last idle (state 2 or 3) played, or empty. */
	const Common::String &getIdleSound() const { return _idleSound; }
	bool holdsSpeechLock() const { return _holdsLock; }
	/** The line being spoken, or last spoken (+0x7C). */
	const Common::String &getClipName() const { return _clip; }

	/** Play ANIM_<NAME><set>@p action once, then go idle (0x4061F0, state 0xF). */
	void act(Room &room, Sound &sound, const Common::String &action);
	/**
	 * Become @p object and play its sequence ANIM_<object without OBJ_>1
	 * once, then go idle as before (0x406130, state 0xD): Shaggy pumping.
	 */
	void special(Room &room, Sound &sound, const Common::String &object);
	/**
	 * Become @p object and loop its sequence ANIM_<object without OBJ_>1
	 * until another state (0x406190, state 0xE): Artie gagged.
	 */
	void specialLoop(Room &room, Sound &sound, const Common::String &object);
	/**
	 * The set (+0x60): "_" and @p digit, or none for 0 (0x406230). The next
	 * state change shows it: Artie in his chains, OBJ_ARTIE_3.
	 */
	void setSet(char digit);
	/** Play ANIM_<NAME><set>@p action looping, until another state (0x406210, state 0x10). */
	void actLoop(Room &room, Sound &sound, const Common::String &action);
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
	 * Walk in from @p direction (0x405E60 to 0x405FE0): ENDWALK, 50 behind
	 * the character's depth (50 in front for DL, DR) until the next state.
	 */
	void walkIn(Room &room, Sound &sound, const Common::String &direction);

private:
	/** rand() % @p n from the script's idle source. */
	uint idleDraw(uint n);
	/**
	 * 0x405CE0: @p prefix and the first digit that makes an ANIM, the digit
	 * r + 1 as r = rand() % r goes down from 9 to 0. Returns 0 if none does.
	 */
	int findIdle(Room &room, const Common::String &prefix, Common::String &anim);

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
	RoomScript *_script = nullptr;
	int _countdown = 0;        // +0x5C: ticks to the next idle
	int _idleCap = 0;          // +0x98: the countdown's most
	bool _holdsLock = false;   // +0xB4: talking with the speech lock
	bool _clicked = false;     // +0x08 with game +0x20 bit 4
	Common::String _idleSound;
};

} // End of namespace Terraglyph

#endif // TERRAGLYPH_CHARACTER_H
