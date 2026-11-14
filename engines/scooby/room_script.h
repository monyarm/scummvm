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

#ifndef SCOOBY_ROOM_SCRIPT_H
#define SCOOBY_ROOM_SCRIPT_H

#include "common/array.h"
#include "common/formats/ini-file.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/str.h"

namespace Common {
class RandomSource;
}

#include "scooby/character.h"

namespace Scooby {

class Room;
class Sound;

/**
 * The default room behaviour of Scooby.exe's base room class (vtable
 * 0x45E5FC): the room's characters, the scene player and the exit
 * transition. Rooms with classes of their own (story checks, set pieces)
 * are not implemented.
 */
class RoomScript {
public:
	/** The gang, in the order the walk-off measures them (0x40BE20). */
	enum Gang {
		kFred,
		kVelma,
		kDaphne,
		kShaggy,
		kScooby,
		kGangCount
	};

	/** What an exit's constructor (0x401850) reads from object.ini. */
	struct Exit {
		Common::String object;
		Common::String destination;
		Common::String scene;
		int movie;
		Common::String directions[kGangCount];
	};

	RoomScript(Room &room, Sound &sound, const Common::INIFile &objectIni);

	/** Set up for the room just loaded: its characters, then the narrator. */
	void enter();

	/** Read @p object's exit keys. */
	Exit readExit(const Common::String &object) const;

	/**
	 * The default exit handler (0x40C1E0): block input, start the exit's
	 * scene and park the exit until the scene is over.
	 */
	void startExit(const Exit &exit);

	/** Start a scene (0x40B400) if none is playing and no exit is parked. */
	void startScene(const Common::String &scene);

	/**
	 * Once a tick, after the room's sprites: the characters, the scene
	 * player (0x40C810) and the walk-off (0x40BE20).
	 */
	void tick();

	/** Input is blocked: a scene or an exit is under way (game +0x24). */
	bool isBusy() const { return _busy; }

	/**
	 * After the walk-off: the room to go to and the movie to play first
	 * (-1 for none). Returns false until then.
	 */
	bool getNextRoom(Common::String &room, int &movie) const;

	/** The names of the room's characters, then the narrator. */
	Common::String describeCharacters() const;
	/** The scene's lines, resolved now by clip name, as "1:FRED 2:VELMA". */
	Common::String describeScene(const Common::String &scene) const;
	/** The walks started since enter(), as "NAME direction steps@tick ...". */
	const Common::String &getWalkLog() const { return _walkLog; }
	/** Ticks run since enter(). */
	uint32 getTick() const { return _tick; }

	/**
	 * A click on a clickable's sprite, handled by its update on the next
	 * tick (0x401440). Returns false if the sprite isn't a clickable.
	 */
	bool clickClickable(uint sprite);
	/** Whether the clickable on @p sprite is playing its click. */
	bool isClickablePlaying(uint sprite) const;
	/** The clickable's state ANIM name (+0x60), or empty. */
	Common::String getClickableState(uint sprite) const;
	/** What the last click did, as "anim A steps N sound S next B", or "ghost". */
	const Common::String &getClickLog() const { return _clickLog; }

	/**
	 * The ghost's chance on a click is 1 in @p chance, drawn from @p random
	 * (0x401440: rand() % 30). A null source or 0 turns the ghost off.
	 */
	void setGhostChance(Common::RandomSource *random, uint chance);
	/** Forget every clickable's saved state (the save file's Clickables section). */
	void resetClickableStates() { _clickableStates.clear(); }

private:
	/** A default clickable (0x401000), its fields by offset. */
	struct Clickable {
		uint sprite;
		Common::String object; // +0x24
		Common::String state;  // +0x60: the ANIM it plays on a click
		Common::String sound;  // +0x68: SFX_ and the state without ANIM_
		bool playing;          // +0x70
		bool clicked;          // a click waiting for the update
	};

	Character *findCharacter(const char *name);
	Clickable *findClickable(uint sprite);
	void setSoundName(Clickable &c);
	void tickClickable(Clickable &c);
	void advanceClickable(Clickable &c);
	bool startGhost();
	void tickScene();
	void tickExit();

	Room &_room;
	Sound &_sound;
	const Common::INIFile &_ini;
	Common::Array<Character> _characters;

	// Scene state (+0x14 name, +0x1C line, +0x20 speaker).
	Common::String _scene;
	int _line;
	int _speaker;

	// The parked exit (+0x2C onwards) and the walk-off's leader (+0x90).
	bool _exitParked;
	Exit _exit;
	int _leader;
	Common::String _leaderDirection;
	int _lengths[kGangCount];

	bool _busy;
	bool _done;
	uint32 _tick;
	Common::String _walkLog;

	Common::Array<Clickable> _clickables;
	// The save file's Clickables section: object name -> state ANIM.
	Common::HashMap<Common::String, Common::String, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _clickableStates;
	Common::String _clickLog;

	// The ghost (game +0x1DC, +0x1E0) and its chance.
	int _ghost;
	bool _ghostPlaying;
	bool _ghostDone;
	Common::RandomSource *_random;
	uint _ghostChance;
};

} // End of namespace Scooby

#endif // SCOOBY_ROOM_SCRIPT_H
