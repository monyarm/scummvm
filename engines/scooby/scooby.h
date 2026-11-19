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
class Music;
class RoomClass;
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
	void checkMenus();
	void checkSignIn();
	void checkOptionsCredits();
	void checkAccusation();
	void checkMusicFade();
	// Each room's profile section (room_setup.cpp).
	void saveRoomKeys(Common::INIFile &profile);
	void loadRoomKeys(const Common::INIFile &profile);
	void checkRoomSetup();   // story_check.cpp
	void checkTownCenter();  // story_check.cpp
	void checkStreets();     // story_check.cpp
	void checkStreet1();     // story_check.cpp
	void checkClues();       // story_check.cpp
	void newGameForCheck();  // story_check.cpp
	void checkChases();      // chase_check.cpp
	void checkGhostSpot();   // ghost_spot_check.cpp
	void checkDrawbridge();  // drawbridge_check.cpp
	void checkSouvenirShop(); // souvenir_check.cpp
	void checkGarden();      // garden_check.cpp
	void checkDoors();       // doors_check.cpp
	void checkThrone();      // throne_check.cpp
	void checkDining();      // dining_check.cpp
	void checkMaria();       // maria_check.cpp
	void checkKitchen();     // kitchen_check.cpp
	void checkEwan();        // ewan_check.cpp
	void checkStorage();     // storage_check.cpp
	void checkArmory();      // armory_check.cpp
	void checkArenas();      // arena_check.cpp
	void checkBrad();        // brad_check.cpp
	void checkPassages();    // passage_check.cpp
	void checkGallery();     // gallery_check.cpp
	void checkKST();         // kst_check.cpp
	void checkMaze();        // maze_check.cpp
	void checkIdles();       // idle_check.cpp
	int _checkSprite = -1;   // a sprite a check waits on
	bool _checking = false;
	Music *_music = nullptr;
	/** One game tick as run() does it; true if it asks for another room. */
	bool tickOnce(Common::String &nextRoom, int &movie);
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

	// Room classes (room_class.h), built once per room name and kept, as the
	// original's factory builds them when the game starts.
	Common::HashMap<Common::String, RoomClass *> _roomClasses;
	RoomClass *_roomClass = nullptr;
	bool _mouseDown = false;
	int _difficulty = 0;            // game +0x160
	Common::String _requestedRoom;  // game vtable +0x04 / +0x08
	int _requestedMovie = -1;
	int _requestedLoad = -1;        // a player's save slot to load (game +0x58)
	int _lastMovie = -1;            // game +0x90, set by the main loop (0x4362E0)
	// Big Head's scene, background and the room after (game +0x220, +0x228,
	// +0x230), and a clue found and not yet named (+0x109).
	Common::String _bigHeadScene, _bigHeadBackground, _bigHeadNext;
	bool _newClue = false;
	// The signed-in player (game +0x16C) and the save slot that holds their
	// profile, or -1.
	Common::String _playerName;
	int _playerSlot = -1;
	void setStoryValues();
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

	// For room classes.
	Room *getRoom() const { return _currentRoom; }
	RoomScript *getScript() const { return _script; }
	/** The sprite under the mouse, or -1. */
	int getHover() const { return _hover; }
	bool isMouseDown() const { return _mouseDown; }
	/** Go to @p room after the current tick, after @p movie if 0-74 (game vtable +0x04, +0x08). */
	void requestRoom(const Common::String &room, int movie = -1);
	/** A room change asked for and not yet made (game +0x08 not empty). */
	bool hasRequestedRoom() const { return !_requestedRoom.empty(); }
	/** Leave by a room's exit (vtable +0x24) for @p room: no scene, no walk. */
	void exitTo(const Common::String &room, int movie = -1);
	/** Quit the game (game +0x110), or only log it during the checks. */
	void requestQuit();
	int getDifficulty() const { return _difficulty; }
	void setDifficulty(int level) { _difficulty = level; }
	/** The story's first room: ROOM_P01_Town_Center, ROOM_P01_Drawbridge. */
	Common::String getFirstGameRoom() const;
	/** The class of @p room, built on first use, or nullptr for the base class. */
	RoomClass *getRoomClass(const Common::String &room);
	/** The game's music (room_setup.h). */
	Music *getMusic() const { return _music; }
	/** Whether @p name is one of ClueOwners0-4 (game +0xD8, 0x40A870). */
	bool isClueOwner(const Common::String &name) const;
	/** Whether @p clue is one of CluesFound0-4 (game +0xB0, 0x4301A0). */
	bool isClueFound(const Common::String &clue) const;
	/** The clue @p suspect gives, once (Showdown's game vtable +0x44, 0x42F990; clues.cpp). */
	void giveClue(const Common::String &suspect);
	/** Every room's class, in the art file's room order, built if need be (the factory, 0x42E0E0). */
	Common::Array<RoomClass *> getAllRoomClasses();
	Common::Point getMousePos() const { return _mousePos; }

	/**
	 * A new game (game vtable +0x4C): the level, the story's state, the
	 * player, the movies seen, every room's class (0x435780) and the
	 * inventory (+0x50) back to the start, StartRoom the level select.
	 * Showdown's also sets its story's values (0x42DBC0).
	 */
	void newGame();
	/** The saved players: the saves whose description is a player's name, by name. */
	void listPlayers(Common::Array<Common::String> &names, Common::Array<int> &slots);
	/** Sign in as @p name, whose profile is in @p slot, or in a free slot if -1. */
	void signIn(const Common::String &name, int slot);
	const Common::String &getPlayerName() const { return _playerName; }
	int getPlayerSlot() const { return _playerSlot; }
	/** Load the profile in @p slot after the current tick (game +0x58). */
	void requestLoad(int slot) { _requestedLoad = slot; }
	/** Delete the profile in @p slot. */
	void deletePlayer(int slot);
	/**
	 * Play @p scene in ROOM_Big_Head over @p background (BIG_HEAD_BG_...),
	 * then go to @p next: game +0x220, +0x228 and +0x230, then game vtable
	 * +0x04. An empty scene leaves the last one, as the original does.
	 */
	void startBigHead(const Common::String &scene, const Common::String &background, const Common::String &next, int movie = -1) {
		if (!scene.empty())
			_bigHeadScene = scene;
		_bigHeadBackground = background;
		_bigHeadNext = next;
		requestRoom("ROOM_Big_Head", movie);
	}
	const Common::String &getBigHeadScene() const { return _bigHeadScene; }
	const Common::String &getBigHeadBackground() const { return _bigHeadBackground; }
	const Common::String &getBigHeadNext() const { return _bigHeadNext; }
	/** A clue just found (game +0x109): Scrappy names it when the suspects and clues next open. */
	void setNewClue(bool found) { _newClue = found; }
	bool hasNewClue() const { return _newClue; }
	/** The last movie played (game +0x90), or -1. */
	int getLastMovie() const { return _lastMovie; }
	/** Every movie in the history counts as seen (0x40B0D0, after the credits). */
	void markAllMoviesSeen();

	/**
	 * The story's values in the profile's Game section, as the original
	 * keeps them on the game object and writes them (0x42F3A0): set by the
	 * new game (Showdown 0x42DBC0) and by the story rooms, saved and loaded
	 * with the profile. Flags read true only for "Yes" (0x40A170); numbers
	 * read with atoi (0x40A0F0); a missing key reads false, 0 or "".
	 */
	bool hasGameValue(const Common::String &key) const { return _profile.hasKey(key, "Game"); }
	bool getGameFlag(const Common::String &key) const;
	int getGameInt(const Common::String &key) const;
	Common::String getGameString(const Common::String &key) const;
	void setGameFlag(const Common::String &key, bool value) { _profile.setKey(key, "Game", value ? "Yes" : "No"); }
	void setGameInt(const Common::String &key, int value) { _profile.setKey(key, "Game", Common::String::format("%d", value)); }
	void setGameString(const Common::String &key, const Common::String &value) { _profile.setKey(key, "Game", value); }

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
	/** Ids 0 and 25-28 (0x42DE10). */
	static bool isTitleMovie(int id) { return id == 0 || (id >= 25 && id <= 28); }

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime;
	}

	Common::Error loadGameState(int slot) override;
	/** Not from the menus: Sign In's new game isn't worth the autosave slot. */
	bool canSaveAutosaveCurrently() override;
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
