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
#include "common/str.h"

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

private:
	Character *findCharacter(const char *name);
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
};

} // End of namespace Scooby

#endif // SCOOBY_ROOM_SCRIPT_H
