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
 * What a room's own class hears from the base room. The base class does
 * nothing with any of it.
 */
class RoomHooks {
public:
	virtual ~RoomHooks() {}
	/** An object tells the room (room vtable +0x14, through game +0x40). */
	virtual void event(const Common::String &object, bool success) {}
	/** Idle chatter (room vtable +0x34), every 25-54 seconds without a scene. */
	virtual void idle() {}
	/**
	 * An exit clicked (room vtable +0x24). Returns true if the class handled
	 * it, false for the default handler (0x40C1E0).
	 */
	virtual bool leaveBy(const Common::String &exitObject) { return false; }
};

/**
 * The default room behaviour of Scooby.exe's base room class (vtable
 * 0x45E5FC): the room's characters, the scene player and the exit
 * transition, and the base room's own fields: the opening scene, the snacks
 * eaten and the idle timer. A room's own class hears of objects, exits and
 * idle chatter through RoomHooks.
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

	/** A room's own base state, kept for the whole game (+0x10, +0x88). */
	struct Memory {
		bool openingPlayed = false; // +0x10, saved as OpeningPlayed
		uint snacksGiven = 0;       // +0x88, saved as SnacksGiven
	};
	typedef Common::HashMap<Common::String, Memory> MemoryMap;

	RoomScript(Room &room, Sound &sound, const Common::INIFile &objectIni);

	/** Set up for the room just loaded: its characters, then the narrator. */
	void enter();

	/**
	 * The end of the base entry (0x40B920) for the room left @p from: the
	 * opening scene (+0x78, vtable +0x3C 0x40C7B0), the snacks needed
	 * (+0x24), and the timers. Call after enter().
	 */
	void setup(const Common::String &opening, uint snacksNeeded, const Common::String &from);
	/** The room's class, or nullptr for the base class. */
	void setHooks(RoomHooks *hooks) { _hooks = hooks; }
	RoomHooks *getHooks() const { return _hooks; }
	/** For the ghost spot's openings and the idle timer (the original's rand()). */
	void setRandom(Common::RandomSource *random) { _rng = random; }
	/** An object tells the room's class (game +0x40). */
	void sendEvent(const Common::String &object, bool success);
	/** An exit clicked: the room's class first (vtable +0x24), else startExit(). */
	void takeExit(const Exit &exit);
	/**
	 * Input slot 5 (Escape or Space): the scene playing ends, and so does
	 * the opening's hold (0x40B490, 0x40C810). Returns true if it did.
	 */
	bool skip();
	/**
	 * A snack eaten here (0x40B490, +0x88). Returns true once the room has
	 * had the snacks it needs, when the snack-given line plays.
	 */
	bool snackEaten();
	/** The opening scene has played, or there is none. */
	bool isOpeningDone() const;
	const Common::String &getOpening() const { return _opening; }
	/** A room's state, by name; every room has one. */
	Memory &getMemory(const Common::String &room) { return _memories.getOrCreateVal(room); }
	const MemoryMap &getMemories() const { return _memories; }
	void resetMemories() { _memories.clear(); }

	/** Read @p object's exit keys. */
	Exit readExit(const Common::String &object) const;

	/**
	 * The default exit handler (0x40C1E0): block input, start the exit's
	 * scene and park the exit until the scene is over.
	 */
	void startExit(const Exit &exit);

	/**
	 * Start a scene (0x40B400) if none is playing and no exit is parked;
	 * a blocking one stops input until it is over. Returns false if it
	 * didn't start.
	 */
	bool startScene(const Common::String &scene, bool block = true);

	/** Whether a character of that name is placed in the room (0x40A800). */
	bool hasCharacter(const char *name);
	/** Whether a scene is playing. */
	bool isScenePlaying() const { return !_scene.empty(); }
	/** The scene playing, or empty. */
	const Common::String &getScene() const { return _scene; }

	/**
	 * The speech lock (game +0x15C): a scene holds it while it plays, and
	 * Scrappy while he speaks, so no two talk at once. Returns false if it is
	 * held (0x40A230).
	 */
	bool acquireSpeechLock();
	void releaseSpeechLock();
	bool isSpeechLocked() const { return _speechLock; }

	/** A character placed in the room, by name, or nullptr (0x40A800). */
	Character *getCharacter(const char *name);
	/** The room's characters in placement order, then the narrator (game +0x19C). */
	const Common::Array<Character> &getCharacters() const { return _characters; }

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
	typedef Common::HashMap<Common::String, Common::String, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> StateMap;
	/** The Clickables section: object name -> state ANIM, as the destructor (0x401350) writes it. */
	const StateMap &getClickableStates() const { return _clickableStates; }
	void setClickableState(const Common::String &object, const Common::String &state) { _clickableStates[object] = state; }

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
	void endScene();
	void tickExit();
	void tickOpening();
	void tickIdle();
	uint32 draw(uint n);

	Room &_room;
	Sound &_sound;
	const Common::INIFile &_ini;
	Common::Array<Character> _characters;

	// Scene state (+0x14 name, +0x1C line, +0x20 speaker).
	Common::String _scene;
	int _line;
	int _speaker;
	bool _sceneBlocks = true;
	bool _sceneHoldsLock = false; // +0x84
	bool _speechLock = false;     // game +0x15C

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

	// The base room's own fields.
	RoomHooks *_hooks = nullptr;
	Common::RandomSource *_rng = nullptr;
	MemoryMap _memories;
	Memory *_memory = nullptr;       // the current room's
	Common::String _opening;         // +0x78
	uint _snacksNeeded = 0;          // +0x24
	int _openingDelay = 100;         // +0x74
	int _idleDelay = 0;              // +0xB4
	bool _sceneIsOpening = false;    // the scene's done flag (+0x80) is +0x10

	// The ghost (game +0x1DC, +0x1E0) and its chance.
	int _ghost;
	bool _ghostPlaying;
	bool _ghostDone;
	Common::RandomSource *_random;
	uint _ghostChance;
};

} // End of namespace Scooby

#endif // SCOOBY_ROOM_SCRIPT_H
