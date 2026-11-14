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

#ifndef SCOOBY_H
#define SCOOBY_H

#include "common/scummsys.h"
#include "common/error.h"
#include "common/random.h"
#include "common/str.h"
#include "common/formats/ini-file.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/rect.h"
#include "engines/engine.h"
#include "graphics/surface.h"

#include "scooby/detection.h"

namespace Scooby {

class ArtArchive;
class Inventory;
class Room;
class RoomScript;
class Sound;

class ScoobyEngine : public Engine {
private:
	const ScoobyGameDescription *_gameDescription;
	Common::RandomSource _randomSource;
	ArtArchive *_art;
	Common::INIFile _objectIni;
	Room *_currentRoom;
	uint32 _tickBase;  // getMillis() when the room started
	uint32 _ticksDone; // game ticks run since then
	Graphics::Surface _room; // the current room, at its own size

	void checkAllRooms();
	void checkAllExits();
	void checkAllMovies();
	void checkAllClickables();
	void checkAllInventory();
	void checkAllHints();
	void checkProfile();
	void logProfile(const char *what, const Common::String &text);

	// The player profile (profile.cpp): the original's <name>.sav, which a
	// ScummVM save holds.
	Common::INIFile _profile;   // as last read, so keys not modelled survive
	Common::String _startRoom;  // game +0x24C: the last room that isn't a menu
	static bool isMenuRoom(const Common::String &room);
	void buildProfile(Common::INIFile &profile);
	bool applyProfile(const Common::INIFile &profile);
	bool readProfileText(const Common::String &text, Common::INIFile &profile);
	void drawRoom();
	void updateHover(int x, int y);
	bool handleMouse(bool click, const Common::Point &p);
	bool runTestScript();
	Common::Array<Common::String> _testSteps;
	uint _testStep = 0;
	uint32 _testNext = 0;
	bool _testLoaded = false;
	void clickExit(int sprite);
	int _hover;     // sprite under the mouse, or -1
	int _lastHover;
	Sound *_sound;
	RoomScript *_script;
	Inventory *_inventory;
	Common::Point _mousePos;
	// Movies played this session: those can be skipped (0x42DE10).
	Common::HashMap<int, bool> _seenMovies;
	// Exits taken this session; the original keeps them in its save's
	// Exits section, so each exit's scene plays once.
	Common::HashMap<Common::String, bool, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _takenExits;

protected:
	// Engine APIs
	Common::Error run() override;

public:
	ScoobyEngine(OSystem *syst, const ScoobyGameDescription *gameDesc);
	~ScoobyEngine() override;

	uint32 getFeatures() const;

	/**
	 * Returns the game Id
	 */
	Common::String getGameId() const;

	/**
	 * Returns the engine generation (1, 2 or 3) from the detection table
	 */
	ScoobyGeneration getGeneration() const;

	/**
	 * Returns the Gen 1 art file, or nullptr until run() opens it
	 */
	ArtArchive *getArt() const { return _art; }

	/**
	 * Returns the Gen 1 sound files, or nullptr until run() opens them
	 */
	Sound *getSound() const { return _sound; }

	/**
	 * Returns the inventory, or nullptr until run() creates it
	 */
	Inventory *getInventory() const { return _inventory; }

	/** The INI file of the game's objects */
	const Common::INIFile &getObjectIni() const { return _objectIni; }

	/**
	 * Draws a room as it is set up on entry: its layers and every placement
	 * at step 0, in the original's draw order
	 */
	bool showRoom(const Common::String &name);

	/**
	 * Play movie @p id (BK.NNN) to its end, or until skipped once it has been
	 * seen. Returns false, after a warning, if the file can't be played.
	 */
	bool playMovie(int id);

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime;
	}

	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;

	/** The profile as it would be saved now, as INI text. */
	Common::String getProfileText();
	/** Read a profile (0x42F640) and go to its StartRoom. */
	bool loadProfile(const Common::INIFile &profile);
};

extern ScoobyEngine *g_engine;
#define SHOULD_QUIT ::Scooby::g_engine->shouldQuit()

} // End of namespace Scooby

#endif // SCOOBY_H
