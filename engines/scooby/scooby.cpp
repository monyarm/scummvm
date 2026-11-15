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

#include "scooby/scooby.h"
#include "scooby/art_archive.h"
#include "scooby/cel.h"
#include "scooby/console.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/menu_rooms.h"
#include "scooby/options_rooms.h"
#include "scooby/accusation_rooms.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/room_setup.h"
#include "scooby/signin_room.h"
#include "scooby/sound.h"
#include "scooby/toolbar_rooms.h"
#include "common/algorithm.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "image/bmp.h"
#include "common/debug-channels.h"
#include "common/endian.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/framelimiter.h"
#include "graphics/pixelformat.h"

namespace Scooby {

ScoobyEngine *g_engine;

ScoobyEngine::ScoobyEngine(OSystem *syst, const ScoobyGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Scooby"), _art(nullptr), _currentRoom(nullptr), _tickBase(0), _ticksDone(0), _hover(-1), _lastHover(-1), _sound(nullptr), _script(nullptr), _inventory(nullptr) {
	g_engine = this;

	// Gen 1 discs keep the game files in a "scooby" directory, and detection
	// accepts the disc root, so search that directory too.
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "scooby");
}

ScoobyEngine::~ScoobyEngine() {
	for (Common::HashMap<Common::String, RoomClass *>::iterator it = _roomClasses.begin(); it != _roomClasses.end(); ++it)
		delete it->_value;
	delete _inventory;
	delete _script;
	delete _music;
	delete _sound;
	_room.free();
	delete _currentRoom;
	delete _art;
}

uint32 ScoobyEngine::getFeatures() const {
	return _gameDescription->desc.flags;
}

Common::String ScoobyEngine::getGameId() const {
	return _gameDescription->desc.gameId;
}

ScoobyGeneration ScoobyEngine::getGeneration() const {
	return _gameDescription->generation;
}

Common::Error ScoobyEngine::run() {
	// The original games draw 640x480 16-bit RGB565 art.
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(640, 480, &format);
	if (_system->getScreenFormat() != format)
		return Common::kUnsupportedColorMode;

	setDebugger(new Console());
	debugC(1, kDebugResource, "Starting %s (generation %d)", getGameId().c_str(), getGeneration());

	// Only Gen 1 has readers so far. Detection marks Gen 2 and 3 unsupported,
	// so they should not get here.
	if (getGeneration() != kGen1)
		return Common::kUnsupportedGameidError;

	_art = new ArtArchive();
	if (!_art->open("TGIFILE.ART"))
		return Common::Error(Common::kNoGameDataFoundError, "TGIFILE.ART");
	syncSoundSettings();

	if (!_objectIni.loadFromFile("object.ini"))
		return Common::Error(Common::kNoGameDataFoundError, "object.ini");
	_currentRoom = new Room(*_art, _objectIni);

	_sound = new Sound(_mixer);
	if (!_sound->open())
		return Common::Error(Common::kNoGameDataFoundError, "Sfx.dat, Voice.dat or Music.dat");
	_script = new RoomScript(*_currentRoom, *_sound, _objectIni);
	// A click calls the ghost 1 time in 30 (0x401440). A "ghost_chance" key
	// in the game's config domain changes the odds, for testing; 0 turns
	// the ghost off.
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_script->setRandom(&_randomSource);
	_music = new Music(*_sound);
	// The inventory lasts the whole game; the toolbar is built in each room.
	_inventory = new Inventory(*_currentRoom, *_sound, *_script, _objectIni);
	_inventory->setRandom(&_randomSource);

	if (debugChannelSet(3, kDebugGraphics)) {
		checkAllCels(*_art);
		checkAllRooms();
	}
	if (debugChannelSet(3, kDebugAudio))
		_sound->checkAllClips();
	if (debugChannelSet(3, kDebugScript)) {
		checkRoomSetup();
		checkTownCenter();
		checkStreets();
		checkAllExits();
		checkAllClickables();
		checkAllInventory();
		checkAllHints();
		checkProfile();
		checkMenus();
		checkSignIn();
		checkOptionsCredits();
		checkAccusation();
		checkMusicFade();
		checkAllMovies();
	}

	// The game starts in Scooby.ini's WesternStartRoom (Phantom:
	// MedievalStartRoom), ROOM_Title_Screen by default. A "start_room" key in
	// the game's config domain picks another, for testing; the console's
	// "room" command switches rooms.
	Common::String room = _art->getRoomName(0);
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		if (_art->getRoomName(i) == "ROOM_Title_Screen")
			room = _art->getRoomName(i);
	}
	if (ConfMan.hasKey("start_room"))
		room = ConfMan.get("start_room");
	showRoom(room);
	// A save picked in the launcher (kSupportsLoadingDuringStartup).
	if (ConfMan.hasKey("save_slot"))
		loadGameState(ConfMan.getInt("save_slot"));
	// "start_profile" reads a profile in the original's INI form, for
	// testing; it goes to its StartRoom.
	if (ConfMan.hasKey("start_profile")) {
		Common::FSNode node(ConfMan.getPath("start_profile"));
		Common::SeekableReadStream *in = node.createReadStream();
		Common::INIFile profile;
		if (!in || !profile.loadFromStream(*in) || !loadProfile(profile))
			warning("Cannot start from the profile %s", node.getPath().toString().c_str());
		delete in;
	}
	// Likewise "start_sound" plays one clip, for testing; the console's
	// sound_play does the same.
	if (ConfMan.hasKey("start_sound"))
		_sound->play(ConfMan.get("start_sound"));
	// "start_items" puts inventory objects in the slots (comma-separated
	// OBJ names), for testing; the console's inv_add does the same.
	if (ConfMan.hasKey("start_items")) {
		Common::String list = ConfMan.get("start_items");
		for (Common::String::size_type start = 0; start < list.size();) {
			Common::String::size_type end = list.findFirstOf(',', start);
			if (end == Common::String::npos)
				end = list.size();
			Common::String item(list.c_str() + start, end - start);
			item.trim();
			if (!item.empty())
				_inventory->addItem(item);
			start = end + 1;
		}
	}
	// And "start_movie" plays one movie first, then draws the room again.
	if (ConfMan.hasKey("start_movie")) {
		playMovie(ConfMan.getInt("start_movie"));
		drawRoom();
	}

	Common::Event e;
	Graphics::FrameLimiter limiter(g_system, 60);
	uint32 lastLog = 0;
	uint32 soundLog = 0;
	bool soundWasPlaying = false;
	while (!shouldQuit()) {
		// Report playback once a second, and when a clip ends.
		if (_sound->isPlaying()) {
			const uint32 played = _sound->getElapsedMillis();
			if (!soundWasPlaying)
				soundLog = 0;
			if (played >= soundLog + 1000) {
				soundLog = played - played % 1000;
				debugC(1, kDebugAudio, "sound %s: %u ms played at %u ms", _sound->getPlayingName().c_str(), played, _system->getMillis());
			}
			soundWasPlaying = true;
		} else if (soundWasPlaying) {
			debugC(1, kDebugAudio, "sound %s ended at %u ms", _sound->getPlayingName().c_str(), _system->getMillis());
			soundWasPlaying = false;
		}

		bool changed = runTestScript();
		while (g_system->getEventManager()->pollEvent(e)) {
			if (e.type == Common::EVENT_MOUSEMOVE || e.type == Common::EVENT_LBUTTONDOWN)
				changed = handleMouse(e.type == Common::EVENT_LBUTTONDOWN, e.mouse) || changed;
			if (e.type == Common::EVENT_LBUTTONDOWN)
				_mouseDown = true;
			else if (e.type == Common::EVENT_LBUTTONUP)
				_mouseDown = false;
			else if (e.type == Common::EVENT_KEYDOWN && !(_roomClass && _roomClass->key(e.kbd.ascii, e.kbd.keycode)) &&
			         (e.kbd.keycode == Common::KEYCODE_ESCAPE || e.kbd.keycode == Common::KEYCODE_SPACE)) {
				// The room's class first (the title), then the scene playing
				// and the opening (input slot 5).
				if (!(_roomClass && _roomClass->skip()))
					changed = _script->skip() || changed;
			}
		}

		// 0x435BC0: the game owes elapsed_ms * 36 / 1000 ticks since the
		// room started; run them all, then draw once. A room change ends the
		// run, as it does in the original.
		const uint32 elapsed = _system->getMillis() - _tickBase;
		const uint32 owed = (uint32)((uint64)elapsed * Room::kTicksPerSecond / 1000) - _ticksDone;
		Common::String nextRoom;
		int movie = -1;
		for (uint32 i = 0; i < owed; i++) {
			changed = _currentRoom->tick() || changed;
			_inventory->tickObjects();
			_script->tick();
			_inventory->tickGame();
			if (_roomClass) {
				_roomClass->tick();
				changed = true;
			}
			_music->tick();
			_sound->tickMusicFade();
			changed = changed || _script->isBusy() || _inventory->getState() != Inventory::kStateParked ||
			          _inventory->isScrappyTalking();
			if (!_requestedRoom.empty()) {
				nextRoom = _requestedRoom;
				movie = _requestedMovie;
				_requestedRoom.clear();
				break;
			}
			if (_requestedLoad >= 0)
				break;
			if (_script->getNextRoom(nextRoom, movie) || _inventory->getNextRoom(nextRoom, movie))
				break;
		}
		_ticksDone += owed;
		// Sign In loads a saved player's profile (game +0x58), which goes to
		// its StartRoom.
		if (_requestedLoad >= 0) {
			const int slot = _requestedLoad;
			_requestedLoad = -1;
			const Common::Error err = loadGameState(slot);
			if (err.getCode() != Common::kNoError)
				warning("Cannot load the player in slot %d: %s", slot, err.getDesc().c_str());
			updateHover(_mousePos.x, _mousePos.y);
			nextRoom.clear();
			changed = false;
		}
		// The cursor follows the game's (0x4098B0).
		if (owed && nextRoom.empty() && !_script->isBusy())
			updateHover(_mousePos.x, _mousePos.y);
		if (!nextRoom.empty()) {
			// 0x42DE10: ids outside 0-74 mean no movie. The movie plays
			// before the next room loads.
			if (movie >= 0 && movie <= 74)
				playMovie(movie);
			const Common::String from = _currentRoom->getName();
			if (!showRoom(nextRoom)) {
				warning("Cannot go to %s; staying in %s", nextRoom.c_str(), from.c_str());
				showRoom(from);
			}
			updateHover(_mousePos.x, _mousePos.y);
			changed = false;
		}
		if (changed)
			drawRoom();

		if (elapsed - lastLog >= 1000) {
			lastLog = elapsed - elapsed % 1000;
			debugC(1, kDebugGraphics, "room %s: %u ticks after %u ms", _currentRoom->getName().c_str(), _ticksDone, elapsed);
		}

		limiter.delayBeforeSwap();
		g_system->updateScreen();
		limiter.startFrame();
	}

	return Common::kNoError;
}

static Common::String hashSurface(const Graphics::Surface &s) {
	// Hash the pixels row by row, so the surface's pitch doesn't matter.
	Common::Array<byte> rows;
	rows.resize(s.w * s.h * 2);
	for (int y = 0; y < s.h; y++) {
		const uint16 *src = (const uint16 *)s.getBasePtr(0, y);
		for (int x = 0; x < s.w; x++)
			WRITE_LE_UINT16(&rows[(y * s.w + x) * 2], src[x]);
	}
	Common::MemoryReadStream stream(rows.data(), rows.size());
	return Common::computeStreamMD5AsString(stream);
}

bool ScoobyEngine::handleMouse(bool click, const Common::Point &p) {
	// While a scene or an exit runs, the cursor is hidden and clicks are
	// ignored (game +0x24).
	_mousePos = p;
	if (!click) {
		if (_script->isBusy()) {
			_currentRoom->setCursorPosition(p.x, p.y);
			return false;
		}
		updateHover(p.x, p.y);
		return true;
	}
	if (_script->isBusy() || _inventory->isInputBlocked())
		return false;
	// A room class holding input takes a click as its skip key (the title).
	if (_roomClass && _roomClass->isBusy()) {
		_roomClass->skip();
		return false;
	}
	updateHover(p.x, p.y);
	debugC(1, kDebugInput, "click at %d,%d on %s", p.x, p.y, _hover >= 0 ? _currentRoom->getLabel(_hover).c_str() : "-");
	// A room class's own objects (the menus' buttons) first.
	if (_roomClass && _roomClass->click(_hover))
		return true;
	// The toolbar, inventory and requires objects take the click on the next
	// tick, and so does the game's update (0x4098B0).
	_inventory->click(_hover);
	// With a hint showing, an exit or a character has Scrappy say it
	// (0x402130, 0x404B70). Else exits and clickables answer only the arrow
	// (0x402130, 0x401440).
	if (_inventory->clickHint(_hover)) {
		debugC(1, kDebugInput, "hint for %s", _currentRoom->getLabel(_hover).c_str());
	} else if (_inventory->isDefaultCursor()) {
		if (_hover >= 0 && !_currentRoom->getDestination(_hover).empty())
			clickExit(_hover);
		// A clickable's update takes the click on the next tick, while the
		// cursor shows its rollover (0x401440).
		else if (_hover >= 0 && _currentRoom->isClickable(_hover))
			_script->clickClickable(_hover);
	}
	return true;
}

bool ScoobyEngine::runTestScript() {
	// A "test_script" key in the game's config domain plays input for a live
	// test without the host's mouse: steps separated by ';', each "move x y",
	// "click x y", "down x y", "up x y", "wait ms", "shot name" (the screen as name.bmp in the
	// "test_shots" folder), "type word" (its letters as key presses, '_' for
	// a space), "key code" (one keycode: 8, 13, 27, 127...) or "quit".
	if (_testSteps.empty() && !_testLoaded) {
		_testLoaded = true;
		if (!ConfMan.hasKey("test_script"))
			return false;
		Common::String all = ConfMan.get("test_script");
		for (Common::String::size_type start = 0; start < all.size();) {
			Common::String::size_type end = all.findFirstOf(';', start);
			if (end == Common::String::npos)
				end = all.size();
			Common::String step(all.c_str() + start, end - start);
			step.trim();
			if (!step.empty())
				_testSteps.push_back(step);
			start = end + 1;
		}
		_testNext = _system->getMillis();
	}
	bool changed = false;
	while (_testStep < _testSteps.size() && _system->getMillis() >= _testNext) {
		const Common::String &step = _testSteps[_testStep++];
		char verb[16] = { 0 }, name[64] = { 0 };
		int a = 0, b = 0;
		if (sscanf(step.c_str(), "%15s %d %d", verb, &a, &b) < 1)
			continue;
		debugC(1, kDebugInput, "test step %u: %s", _testStep, step.c_str());
		if (!strcmp(verb, "move") || !strcmp(verb, "click")) {
			changed = handleMouse(false, Common::Point(a, b)) || changed;
			if (!strcmp(verb, "click"))
				changed = handleMouse(true, Common::Point(a, b)) || changed;
		} else if (!strcmp(verb, "wait")) {
			_testNext = _system->getMillis() + a;
		} else if (!strcmp(verb, "shot") && sscanf(step.c_str(), "%*s %63s", name) == 1) {
			Graphics::Surface *screen = _system->lockScreen();
			Common::DumpFile out;
			const Common::Path path = ConfMan.getPath("test_shots").appendComponent(Common::String(name) + ".bmp");
			if (screen && out.open(path))
				Image::writeBMP(out, *screen);
			_system->unlockScreen();
			debugC(1, kDebugInput, "test shot %s", name);
		} else if (!strcmp(verb, "save")) {
			const Common::Error err = saveGameState(a, Common::String::format("test %d", a));
			debugC(1, kDebugInput, "test save %d: %s", a, err.getDesc().c_str());
		} else if (!strcmp(verb, "load")) {
			const Common::Error err = loadGameState(a);
			debugC(1, kDebugInput, "test load %d: %s", a, err.getDesc().c_str());
			changed = true;
		} else if (!strcmp(verb, "type") && sscanf(step.c_str(), "%*s %63s", name) == 1) {
			for (const char *c = name; *c; c++) {
				Common::Event k;
				k.type = Common::EVENT_KEYDOWN;
				const char ch = *c == '_' ? ' ' : *c;
				k.kbd = Common::KeyState((Common::KeyCode)tolower((byte)ch), (byte)ch);
				g_system->getEventManager()->pushEvent(k);
			}
		} else if (!strcmp(verb, "down") || !strcmp(verb, "up")) {
			// The button going down or up at (x, y), as the host sends it.
			Common::Event m;
			m.type = !strcmp(verb, "down") ? Common::EVENT_LBUTTONDOWN : Common::EVENT_LBUTTONUP;
			m.mouse = Common::Point(a, b);
			g_system->getEventManager()->pushEvent(m);
		} else if (!strcmp(verb, "key")) {
			Common::Event k;
			k.type = Common::EVENT_KEYDOWN;
			k.kbd = Common::KeyState((Common::KeyCode)a, a < 0x80 ? a : 0);
			g_system->getEventManager()->pushEvent(k);
		} else if (!strcmp(verb, "quit")) {
			quitGame();
		}
	}
	return changed;
}

void ScoobyEngine::drawRoom() {
	_currentRoom->draw(_room);
	// Rooms wider than the screen (1920 pixels) show their left edge until
	// scrolling exists.
	_system->copyRectToScreen(_room.getPixels(), _room.pitch, 0, 0, MIN<int>(_room.w, 640), MIN<int>(_room.h, 480));
}

void ScoobyEngine::updateHover(int x, int y) {
	// The cursor shows the hovered object's rollover (vtable +0x18), or the
	// default arrow.
	_hover = _currentRoom->hitTest(x, y);
	_currentRoom->setCursorPosition(x, y);
	// A room class holding input hides the cursor (0x435150).
	if (_roomClass && _roomClass->isBusy()) {
		_currentRoom->setCursor("");
		return;
	}
	// The hovered object's rollover for the game's cursor (vtable +0x18),
	// else the game's cursor (0x4098B0).
	const Common::String cursorName = _inventory->getCursorFor(_hover);
	_inventory->cursorShown(cursorName);
	const char *cursor = cursorName.c_str();
	_currentRoom->setCursorPosition(x, y);
	if (_hover != _lastHover) {
		_lastHover = _hover;
		debugC(1, kDebugInput, "hover %s, cursor %s", _hover >= 0 ? _currentRoom->getLabel(_hover).c_str() : "-", cursor);
	}
	_currentRoom->setCursor(cursor);
}

void ScoobyEngine::clickExit(int sprite) {
	// 0x402130: the exit's keys go to the room's handler (vtable +0x24,
	// 0x40C1E0 by default). The scene plays only the first time.
	RoomScript::Exit exit = _script->readExit(_currentRoom->getObjectName(sprite));
	if (_takenExits.contains(exit.object))
		exit.scene.clear();
	_takenExits[exit.object] = true;
	debugC(1, kDebugInput, "exit %s clicked: going to %s", _currentRoom->getLabel(sprite).c_str(), exit.destination.c_str());

	_currentRoom->setCursor("");
	_script->takeExit(exit);
}

RoomClass *ScoobyEngine::getRoomClass(const Common::String &room) {
	RoomClass *c;
	if (!_roomClasses.tryGetVal(room, c)) {
		c = createRoomClass(*this, room);
		_roomClasses[room] = c;
	}
	return c;
}

Common::Array<RoomClass *> ScoobyEngine::getAllRoomClasses() {
	Common::Array<RoomClass *> all;
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		RoomClass *c = getRoomClass(_art->getRoomName(i));
		if (c)
			all.push_back(c);
	}
	return all;
}

void ScoobyEngine::requestRoom(const Common::String &room, int movie) {
	_requestedRoom = room;
	_requestedMovie = movie;
}

void ScoobyEngine::exitTo(const Common::String &room) {
	// The base exit handler (0x40C1E0) with an exit that names only its
	// room: the cursor fades, input stops, and with no scene and no gang to
	// walk off, the room changes.
	RoomScript::Exit exit;
	exit.destination = room;
	exit.movie = -1;
	_currentRoom->setCursor("");
	_script->startExit(exit);
}

void ScoobyEngine::requestQuit() {
	if (_checking) {
		debugC(1, kDebugScript, "quit asked for (not during the checks)");
		return;
	}
	quitGame();
}

bool ScoobyEngine::tickOnce(Common::String &nextRoom, int &movie) {
	_currentRoom->tick();
	_inventory->tickObjects();
	_script->tick();
	_inventory->tickGame();
	if (_roomClass)
		_roomClass->tick();
	_music->tick();
	_sound->tickMusicFade();
	if (!_requestedRoom.empty()) {
		nextRoom = _requestedRoom;
		movie = _requestedMovie;
		_requestedRoom.clear();
		return true;
	}
	if (_requestedLoad >= 0)
		return true;
	return _script->getNextRoom(nextRoom, movie) || _inventory->getNextRoom(nextRoom, movie);
}

void ScoobyEngine::checkMenus() {
	// The front end, driven through the engine: the title's steps, pan and
	// countdown; each menu button's frames, hit test, sound and room; both
	// answers on the quit screen. Hover and the mouse button are set
	// directly.
	_checking = true;
	const uint kMaxTicks = 2000;
	Common::String next;
	int movie;
	// A game room to open the menus from.
	const Common::String game = getFirstGameRoom();
	struct Fresh {
		ScoobyEngine &e;
		void room(const Common::String &name) {
			RoomClass *c;
			if (e._roomClasses.tryGetVal(name, c)) {
				if (e._roomClass == c)
					e._roomClass = nullptr;
				delete c;
				e._roomClasses.erase(name);
			}
		}
	} fresh = { *this };

	// The title from the start of a game: its steps ask for movies, then it
	// shows.
	fresh.room("ROOM_Title_Screen");
	for (int visit = 1; visit <= 4; visit++) {
		showRoom("ROOM_Title_Screen");
		debugC(3, kDebugScript, "menu check title visit %d: asks for %s, movie %d", visit,
		       _requestedRoom.empty() ? "-" : _requestedRoom.c_str(), _requestedRoom.empty() ? -1 : _requestedMovie);
		if (_requestedRoom.empty())
			break;
	}
	const int layer = _currentRoom->getLayerSprite(0);
	Common::String pans;
	uint t = 0;
	while (t < kMaxTicks) {
		t++;
		next.clear();
		const bool asks = tickOnce(next, movie);
		if (t <= 3 || (t >= 318 && t <= 324))
			pans += Common::String::format("%u:%d ", t, layer >= 0 ? _currentRoom->getX(layer) : 0);
		if (asks)
			break;
	}
	debugC(3, kDebugScript, "menu check title: busy %d, layer 0 x [%s], to %s after %u ticks", _roomClass && _roomClass->isBusy(),
	       pans.c_str(), next.c_str(), t);

	// Each menu's buttons, one fresh visit per button: the frames under the
	// mouse and pressed, the sound, and where the click goes.
	static const char *const kMenus[] = { "ROOM_Main_Menu", "ROOM_Difficulty" };
	for (uint m = 0; m < ARRAYSIZE(kMenus); m++) {
		for (uint b = 0;; b++) {
			fresh.room(kMenus[m]);
			showRoom(game);
			showRoom(kMenus[m]);
			ButtonRoom *menu = static_cast<ButtonRoom *>(_roomClass);
			if (!menu || b >= menu->getButtonCount())
				break;
			const int s = menu->getButtonSprite(b);
			_mouseDown = false;
			_hover = -1;
			tickOnce(next, movie);
			const uint32 rest = _currentRoom->getStep(s);
			_sound->stop();
			_hover = s;
			tickOnce(next, movie);
			const uint32 over = _currentRoom->getStep(s);
			const Common::String heard = _sound->isPlaying() ? _sound->getPlayingName() : Common::String("-");
			_mouseDown = true;
			tickOnce(next, movie);
			const uint32 pressed = _currentRoom->getStep(s);
			_mouseDown = false;
			menu->click(s);
			next.clear();
			movie = -1;
			t = 0;
			while (t < kMaxTicks && !tickOnce(next, movie))
				t++;
			debugC(3, kDebugScript, "menu check %s %s: hit %d, steps %u, frames %u %u %u, hears %s, to %s movie %d after %u ticks%s",
			       kMenus[m], _currentRoom->getObjectName(s).c_str(), _currentRoom->hasStepPolygon(s) ? 2 : 1, _currentRoom->getStepCount(s),
			       rest, over, pressed, heard.c_str(), next.c_str(), movie, t,
			       m == 1 ? Common::String::format(", level %d", _difficulty).c_str() : "");
			_sound->stop();
		}
	}

	// Play from the title goes to Sign In.
	fresh.room("ROOM_Main_Menu");
	fresh.room("ROOM_Title_Screen");
	showRoom("ROOM_Title_Screen");
	showRoom("ROOM_Main_Menu");
	debugC(3, kDebugScript, "menu check main menu from the title: Play goes to %s",
	       static_cast<MainMenuRoom *>(_roomClass)->getReturnRoom() == "ROOM_Title_Screen" ? "ROOM_Sign_In" : "?");

	// The quit screen, opened from the menu: the eyes follow, then each
	// answer's line and where it ends.
	for (int yes = 0; yes <= 1; yes++) {
		fresh.room("ROOM_Main_Menu");
		fresh.room("ROOM_Quit");
		showRoom(game);
		showRoom("ROOM_Main_Menu");
		showRoom("ROOM_Quit");
		QuitRoom *quit = static_cast<QuitRoom *>(_roomClass);
		const int button = quit->getButtonSprite(yes ? 0 : 1);
		_hover = button;
		tickOnce(next, movie);
		const int eyes = _currentRoom->getStep(quit->getEyesSprite());
		_hover = -1;
		quit->click(button);
		const Common::String line = _sound->isPlaying() ? _sound->getPlayingName() : Common::String("-");
		_sound->stop();
		next.clear();
		t = 0;
		while (t < kMaxTicks && !tickOnce(next, movie) && !(quit->isQuitting() && quit->isDone()))
			t++;
		debugC(3, kDebugScript, "menu check quit %s: eyes step %d, Scooby says %s, then %s", yes ? "yes" : "no", eyes, line.c_str(),
		       quit->isQuitting() ? "quit" : next.c_str());
	}
	_sound->stop();
	_checking = false;
	debugC(3, kDebugScript, "menu check: done");
}

void ScoobyEngine::checkSignIn() {
	// Sign In, driven through the engine: where its fields and buttons are,
	// how a name lays out, what keys and buttons type, the prompt's and the
	// idle line's ticks, the list's rows, hover, arrows and delete, and
	// where Play and Quit go, with the new game's values.
	_checking = true;
	const uint kMaxTicks = 3000;
	Common::String next;
	int movie;
	SignInRoom *si = nullptr;
	const Common::Array<Common::String> none;
	const Common::Array<int> noSlots;
	auto fresh = [&]() {
		RoomClass *c;
		if (_roomClasses.tryGetVal("ROOM_Sign_In", c)) {
			if (_roomClass == c)
				_roomClass = nullptr;
			delete c;
			_roomClasses.erase("ROOM_Sign_In");
		}
		_sound->stop();
		showRoom("ROOM_Sign_In");
		si = static_cast<SignInRoom *>(_roomClass);
		si->setPlayers(none, noSlots);
		_hover = -1;
		_mouseDown = false;
		_mousePos = Common::Point(0, 0);
	};
	auto typeKeys = [&](const char *keys) {
		for (const char *k = keys; *k; k++)
			si->key((byte)*k, 0);
		tickOnce(next, movie);
	};
	auto shown = [&](const char *keys) {
		Common::String s;
		for (const char *k = keys; *k; k++)
			s += *k == '\b' ? Common::String("<BS>") : *k == '\r' ? Common::String("<CR>") : Common::String(*k);
		return s;
	};
	auto clear = [&]() {
		typeKeys("\b\b\b\b\b\b\b\b\b\b\b");
	};
	auto hidden = [&](int sprite) { return sprite < 0 || _currentRoom->isHidden(sprite); };

	// The room as it starts.
	fresh();
	static const char *const kButtons[] = { "OBJ_SIGN_IN_PLAY", "OBJ_SIGN_IN_QUIT", "OBJ_SIGN_IN_U_ARROW", "OBJ_SIGN_IN_D_ARROW",
		"OBJ_SIGN_IN_R_ARROW", "OBJ_SIGN_IN_L_ARROW" };
	Common::String found;
	for (uint i = 0; i < ARRAYSIZE(kButtons); i++)
		found += si->findButtonFor(kButtons[i], "") >= 0 ? "y" : "n";
	debugC(3, kDebugScript, "sign in check entry: list %d,%d, name %d,%d, letters %s, buttons %s, countdown %d, idle %d",
	       si->getListX(), si->getListY(), si->getFieldX(), si->getFieldY(), si->getLetters().c_str(), found.c_str(),
	       si->getCountdown(), si->getIdle());

	// The prompt, typing and the idle line, with no input.
	int typingAt = -1, promptAt = -1, idleAt = -1;
	Common::String idleLine = "-";
	for (int t = 1; t <= 1100; t++) {
		const int idle = si->getIdle();
		tickOnce(next, movie);
		if (typingAt < 0 && si->isTyping())
			typingAt = t;
		if (promptAt < 0 && si->getCountdown() == 0)
			promptAt = t;
		if (idleAt < 0 && si->getIdle() > idle) {
			idleAt = t;
			if (_sound->isClipPlaying("Sign_In.SHAGGY.Type_Name_Idle"))
				idleLine = "Sign_In.SHAGGY.Type_Name_Idle";
		}
	}
	debugC(3, kDebugScript, "sign in check timers: typing at %d, prompt at %d, idle at %d says %s", typingAt, promptAt, idleAt,
	       idleLine.c_str());

	// Names laid out in the typed field: each letter's step and place.
	static const char *const kTexts[] = { "A", "SCOOBY DOO", "W1 M", "0123456789", "IIIII" };
	for (uint i = 0; i < ARRAYSIZE(kTexts); i++) {
		fresh();
		si->skip();
		clear();
		typeKeys(kTexts[i]);
		Common::String out;
		const int box = si->getFieldBox(-1);
		for (uint c = 0; c < si->getName().size(); c++) {
			const int s = si->getFieldChar(-1, c);
			if (_currentRoom->isHidden(s))
				out += " -";
			else
				out += Common::String::format(" %u@%d,%d", _currentRoom->getStep(s), _currentRoom->getX(s) - _currentRoom->getX(box),
				                              _currentRoom->getY(s) - _currentRoom->getY(box));
		}
		debugC(3, kDebugScript, "sign in check text '%s':%s", si->getName().c_str(), out.c_str());
	}

	// What keys type.
	static const char *const kKeys[] = { "scooby", " a  b ", "abcdefghijkl", "ab\bc", "a!?-z", "doo\r" };
	for (uint i = 0; i < ARRAYSIZE(kKeys); i++) {
		fresh();
		si->skip();
		typeKeys(kKeys[i]);
		debugC(3, kDebugScript, "sign in check keys '%s': name '%s', leaving %s", shown(kKeys[i]).c_str(), si->getName().c_str(),
		       si->getLeavingLine().empty() ? "-" : si->getLeavingLine().c_str());
	}
	// Keys before typing starts are lost.
	fresh();
	typeKeys("abc");
	debugC(3, kDebugScript, "sign in check keys before the prompt: name '%s'", si->getName().c_str());

	// The buttons: a letter, space twice, another letter, back three times;
	// then every letter in turn.
	fresh();
	si->skip();
	const Common::String letters = si->getLetters();
	const int right = si->findButtonFor("OBJ_SIGN_IN_R_ARROW", ""), left = si->findButtonFor("OBJ_SIGN_IN_L_ARROW", "");
	Common::String names;
	if (letters.size() >= 2) {
		const int b0 = si->findButtonFor("OBJ_SIGN_IN_CHARACTERS", Common::String());
		int b1 = -1;
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == "OBJ_SIGN_IN_CHARACTERS" && (int)s != b0) {
				b1 = s;
				break;
			}
		}
		const int seq[] = { b0, right, right, b1, left, left, left };
		for (uint i = 0; i < ARRAYSIZE(seq); i++) {
			si->click(seq[i]);
			tickOnce(next, movie);
			names += Common::String::format(" '%s'", si->getName().c_str());
		}
	}
	debugC(3, kDebugScript, "sign in check buttons:%s", names.c_str());
	clear();
	for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
		if (_currentRoom->getObjectName(s) == "OBJ_SIGN_IN_CHARACTERS")
			si->click(s);
	}
	tickOnce(next, movie);
	debugC(3, kDebugScript, "sign in check every letter: name '%s'", si->getName().c_str());

	// The list: six players.
	static const char *const kPlayers[] = { "ALPHA", "BRAVO", "CHARLIE", "DELTA", "ECHO", "FOXTROT" };
	Common::Array<Common::String> players;
	Common::Array<int> slots;
	for (uint i = 0; i < ARRAYSIZE(kPlayers); i++) {
		players.push_back(kPlayers[i]);
		slots.push_back(i + 1);
	}
	auto rows = [&]() {
		Common::String r;
		for (int i = 0; i < (int)SignInRoom::kRows; i++)
			r += Common::String::format("%s%s", i ? "|" : "", si->getFieldText(i).c_str());
		return Common::String::format("rows [%s] selected %d scroll %d up %d down %d", r.c_str(), si->getSelected(), si->getScroll(),
		                              !hidden(si->getArrowSprite(true)), !hidden(si->getArrowSprite(false)));
	};
	fresh();
	si->setPlayers(players, slots);
	tickOnce(next, movie);
	debugC(3, kDebugScript, "sign in check list: %s", rows().c_str());
	const int lx = si->getListX(), ly = si->getListY();
	static const int kPoints[][2] = { { 5, 0 }, { 5, 1 }, { 5, 29 }, { 5, 30 }, { 5, 31 }, { 239, 119 }, { 240, 50 }, { -1, 50 }, { 5, 120 } };
	Common::String hovers;
	for (uint i = 0; i < ARRAYSIZE(kPoints); i++) {
		si->mouseOnList(lx + kPoints[i][0], ly + kPoints[i][1], false);
		hovers += Common::String::format(" %d,%d:%d", kPoints[i][0], kPoints[i][1], si->getHovered());
	}
	debugC(3, kDebugScript, "sign in check hover:%s", hovers.c_str());
	// The arrows, from the first row selected; a hidden arrow can't be
	// clicked.
	si->mouseOnList(lx + 5, ly + 15, true);
	tickOnce(next, movie);
	debugC(3, kDebugScript, "sign in check select row 0: %s", rows().c_str());
	static const char kArrows[] = "DDDDDDUUUUUUUU";
	for (const char *a = kArrows; *a; a++) {
		const int arrow = si->getArrowSprite(*a == 'U');
		const bool shut = hidden(arrow);
		if (!shut)
			si->click(arrow);
		tickOnce(next, movie);
		debugC(3, kDebugScript, "sign in check %s: %s%s", *a == 'U' ? "up" : "down", shut ? "hidden, " : "", rows().c_str());
	}
	// Scrolled to the end, the last row deleted.
	fresh();
	si->setPlayers(players, slots);
	tickOnce(next, movie);
	si->mouseOnList(lx + 5, ly + 15, true);
	for (int i = 0; i < 5; i++) {
		si->click(si->getArrowSprite(false));
		tickOnce(next, movie);
	}
	debugC(3, kDebugScript, "sign in check scrolled: %s", rows().c_str());
	si->key(0, Common::KEYCODE_DELETE);
	tickOnce(next, movie);
	// The rows show it on the next tick: the delete comes after them.
	tickOnce(next, movie);
	Common::String left5;
	for (uint i = 0; i < si->getPlayers().size(); i++)
		left5 += " " + si->getPlayers()[i];
	debugC(3, kDebugScript, "sign in check delete: players%s, %s", left5.c_str(), rows().c_str());

	// A saved player, clicked twice: their profile loads.
	fresh();
	si->setPlayers(players, slots);
	tickOnce(next, movie);
	si->mouseOnList(lx + 5, ly + 45, true);
	si->mouseOnList(lx + 5, ly + 45, true);
	const Common::String leaving = si->getLeavingLine();
	si->skip();
	next.clear();
	uint t = 0;
	while (t < kMaxTicks && _requestedLoad < 0 && !tickOnce(next, movie))
		t++;
	debugC(3, kDebugScript, "sign in check saved player: name %s, says %s, loads slot %d, signed in as %s slot %d, StartRoom %s",
	       si->getName().c_str(), leaving.empty() ? "-" : leaving.c_str(), _requestedLoad, _playerName.c_str(), _playerSlot,
	       _startRoom.c_str());
	_requestedLoad = -1;

	// A new player: a new game, then the level select.
	fresh();
	si->setPlayers(players, slots);
	si->skip();
	typeKeys("newbie\r");
	si->skip();
	next.clear();
	movie = -1;
	t = 0;
	while (t < kMaxTicks && !tickOnce(next, movie))
		t++;
	debugC(3, kDebugScript, "sign in check new player: to %s movie %d, signed in as %s, slot %s, StartRoom %s, level %d, movies %u",
	       next.c_str(), movie, _playerName.c_str(), _playerSlot >= 0 && _playerSlot != getAutosaveSlot() ? "free" : "NONE",
	       _startRoom.c_str(), _difficulty, _seenMovies.size());
	// The story's values: the fixed ones, and whether the random ones are
	// in range.
	static const char *const kFixed[] = { "MoaningActive", "SheriffWaiting", "HotelHallDoor", "SheriffPuzzle", "GummyPuzzle",
		"TunnelLit", "VaultOpen", "BagsRandomized", "ClockHour", "ClockMinute" };
	Common::String story;
	Common::String v;
	for (uint i = 0; i < ARRAYSIZE(kFixed); i++)
		story += Common::String::format(" %s=%s", kFixed[i], _profile.getKey(kFixed[i], "Game", v) ? v.c_str() : "-");
	auto inRange = [&](const char *key, int hi) {
		return _profile.getKey(key, "Game", v) && !v.empty() && atoi(v.c_str()) >= 0 && atoi(v.c_str()) <= hi &&
		       Common::String::format("%d", atoi(v.c_str())) == v;
	};
	story += Common::String::format(" GraveLocation %s StrongBox %s", inRange("GraveLocation", 8) ? "ok" : "BAD", inRange("StrongBox", 3) ? "ok" : "BAD");
	uint mask = 0;
	bool levers = true;
	for (uint i = 0; i < 4; i++) {
		const Common::String key = Common::String::format("LeverAlias%u", i);
		levers = levers && inRange(key.c_str(), 3);
		if (levers)
			mask |= 1 << atoi(v.c_str());
	}
	story += Common::String::format(" levers %s", levers && mask == 15 ? "ok" : "BAD");
	static const char *const kVillains[] = { "ARTIE", "ICDOUBLE", "JENNY", "SHERIFF", "GUMMY" };
	static const char *const kArtifacts[] = { "Bowl", "Deer", "Vase" };
	bool villain = false, artifact = false;
	if (_profile.getKey("Whodunit", "Game", v)) {
		for (uint i = 0; i < ARRAYSIZE(kVillains); i++)
			villain = villain || v == kVillains[i];
	}
	if (_profile.getKey("ArtifactType", "Game", v)) {
		for (uint i = 0; i < ARRAYSIZE(kArtifacts); i++)
			artifact = artifact || v == kArtifacts[i];
	}
	story += Common::String::format(" Whodunit %s ArtifactType %s", villain ? "ok" : "-", artifact ? "ok" : "-");
	if (!_profile.hasKey("Whodunit", "Game"))
		story = " none";
	debugC(3, kDebugScript, "sign in check story:%s", story.c_str());

	// Play with no name: Shaggy asks for one.
	fresh();
	si->skip();
	_sound->stop();
	si->click(si->findButtonFor("OBJ_SIGN_IN_PLAY", ""));
	debugC(3, kDebugScript, "sign in check empty play: leaving %s, hears %s", si->getLeavingLine().empty() ? "-" : si->getLeavingLine().c_str(),
	       _sound->isClipPlaying("Sign_In.SHAGGY.Type_Name_Idle") ? "Sign_In.SHAGGY.Type_Name_Idle" : "-");

	// Quit.
	fresh();
	si->click(si->findButtonFor("OBJ_SIGN_IN_QUIT", ""));
	const Common::String quitLine = si->getLeavingLine();
	si->skip();
	next.clear();
	t = 0;
	while (t < kMaxTicks && !tickOnce(next, movie))
		t++;
	debugC(3, kDebugScript, "sign in check quit: says %s, to %s, signed in as '%s'", quitLine.empty() ? "-" : quitLine.c_str(),
	       next.c_str(), _playerName.c_str());

	// Back to no game and no player.
	_sound->stop();
	newGame();
	_startRoom.clear();
	_profile = Common::INIFile();
	_requestedLoad = -1;
	_checking = false;
	debugC(3, kDebugScript, "sign in check: done");
}

void ScoobyEngine::checkOptionsCredits() {
	// Options and Credits, driven through the engine: the sliders' ends and
	// knobs for known volumes, knobs dragged to points around each line,
	// the volumes and samples they leave; the credits' movies for each
	// villain, the roll's parts and ticks, and how it ends.
	_checking = true;
	const uint kMaxTicks = 20000;
	Common::String next;
	int movie;
	auto freshClass = [&](const char *name) {
		RoomClass *c;
		if (_roomClasses.tryGetVal(name, c)) {
			if (_roomClass == c)
				_roomClass = nullptr;
			delete c;
			_roomClasses.erase(name);
		}
	};
	static const char *const kVolumeKeys[] = { "music_volume", "sfx_volume", "speech_volume" };
	int saved[3];
	for (uint i = 0; i < 3; i++)
		saved[i] = ConfMan.getInt(kVolumeKeys[i]);

	// The sliders, from known volumes.
	static const int kStart[] = { 256, 205, 150 };
	for (uint i = 0; i < 3; i++)
		ConfMan.setInt(kVolumeKeys[i], kStart[i]);
	syncSoundSettings();
	freshClass("ROOM_Options");
	showRoom("ROOM_Options");
	OptionsRoom *opt = static_cast<OptionsRoom *>(_roomClass);
	uint hiddenMarkers = 0;
	for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
		const Common::String &o = _currentRoom->getObjectName(s);
		if ((o == "OBJ_OPTIONS_NAIL" || o == "OBJ_OPTIONS_SWORDS") && _currentRoom->isHidden(s) && !_currentRoom->isEngineSprite(s))
			hiddenMarkers++;
	}
	debugC(3, kDebugScript, "options check entry: %d markers, %u hidden", opt->getMarkerCount(), hiddenMarkers);
	for (uint i = 0; i < OptionsRoom::kSliders && opt->getMarkerCount() == 6; i++) {
		float x1, y1, x2, y2;
		opt->getEnds(i, x1, y1, x2, y2);
		const int k = opt->getKnob(i);
		debugC(3, kDebugScript, "options check slider %u: %d,%d to %d,%d, volume %d, knob %.4f at %d,%d (%s, depth %d)", i, (int)x1, (int)y1,
		       (int)x2, (int)y2, kStart[i], opt->getValue(i), _currentRoom->getX(k), _currentRoom->getY(k),
		       _currentRoom->getAnim(k).c_str(), _currentRoom->getDepth(k));
	}

	// Dragging: grabbed 2 right and 3 down of where the knob is drawn, then
	// the mouse to points along and off the line (percent of the way, and
	// pixels across), then let go.
	static const int kPoints[][2] = { { -20, 0 }, { 25, 0 }, { 50, 15 }, { 50, -15 }, { 75, 40 }, { 130, 0 } };
	for (uint i = 0; i < OptionsRoom::kSliders && opt->getMarkerCount() == 6; i++) {
		for (uint end = 0; end < 2; end++) {
			// Each run from the starting volumes.
			for (uint v = 0; v < 3; v++)
				ConfMan.setInt(kVolumeKeys[v], kStart[v]);
			syncSoundSettings();
			freshClass("ROOM_Options");
			showRoom("ROOM_Options");
			opt = static_cast<OptionsRoom *>(_roomClass);
			_sound->stop();
			float fx1, fy1, fx2, fy2;
			opt->getEnds(i, fx1, fy1, fx2, fy2);
			const int x1 = (int)fx1, y1 = (int)fy1, x2 = (int)fx2, y2 = (int)fy2;
			const int k = opt->getKnob(i);
			_mousePos = Common::Point(_currentRoom->getX(k) + 2, _currentRoom->getY(k) + 3);
			_mouseDown = true;
			const bool grabbed = opt->click(k);
			Common::String out;
			// Each run goes to the points in order; the second ends past the
			// left end instead of the right.
			for (uint p = 0; p < ARRAYSIZE(kPoints); p++) {
				const int pct = end && p == ARRAYSIZE(kPoints) - 1 ? -50 : kPoints[p][0];
				const int across = kPoints[p][1];
				_mousePos = Common::Point(x1 + (x2 - x1) * pct / 100, y1 + (y2 - y1) * pct / 100 + across);
				tickOnce(next, movie);
				out += Common::String::format(" %d,%d:%.4f@%d,%d", _mousePos.x, _mousePos.y, opt->getValue(i), _currentRoom->getX(k),
				                              _currentRoom->getY(k));
			}
			_mouseDown = false;
			tickOnce(next, movie);
			const char *sample = opt->getSample(i);
			debugC(3, kDebugScript, "options check drag %u%s: grabbed %d,%s; let go %.4f, volume %d, dragging %d, hears %s", i, end ? " left" : "",
			       grabbed, out.c_str(), opt->getValue(i), ConfMan.getInt(kVolumeKeys[i]), opt->isDragging(i),
			       *sample && _sound->isClipPlaying(sample) ? sample : "-");
		}
	}
	_sound->stop();
	for (uint i = 0; i < 3; i++)
		ConfMan.setInt(kVolumeKeys[i], saved[i]);
	syncSoundSettings();

	// The credits: the villain's movie after the accusation.
	static const char *const kVillains[] = { "ARTIE", "GUMMY", "ICDOUBLE", "JENNY", "SHERIFF", "BRAD", "JESTER", "EWAN", "MARIA",
		"WIZARD", "NOBODY" };
	Common::String movies;
	for (uint i = 0; i < ARRAYSIZE(kVillains); i++) {
		setGameString("Whodunit", kVillains[i]);
		freshClass("ROOM_Credits");
		showRoom("ROOM_Big_Head");
		showRoom("ROOM_Credits");
		movies += Common::String::format(" %s:%s/%d", kVillains[i], _requestedRoom.empty() ? "-" : _requestedRoom.c_str(),
		                                 _requestedRoom.empty() ? -1 : _requestedMovie);
		_requestedRoom.clear();
	}
	debugC(3, kDebugScript, "credits check villains:%s", movies.c_str());

	// Then movie 58, then the roll; the ending for a signed-in player.
	auto roll = [&](bool ending, int skipAt) {
		freshClass("ROOM_Credits");
		showRoom("ROOM_Big_Head");
		showRoom("ROOM_Credits");
		_requestedRoom.clear();
		showRoom("ROOM_Credits");
		const Common::String asked = Common::String::format("%s/%d", _requestedRoom.empty() ? "-" : _requestedRoom.c_str(),
		                                                    _requestedRoom.empty() ? -1 : _requestedMovie);
		_requestedRoom.clear();
		_lastMovie = ending ? 58 : 1;
		_seenMovies.clear();
		_seenMovies[1] = true;
		signIn("CHECKER", 7);
		showRoom("ROOM_Credits");
		CreditsRoom *cr = static_cast<CreditsRoom *>(_roomClass);
		Room &room = *_currentRoom;
		auto part = [&]() {
			int w, h;
			room.getFirstCelSize(cr->getText(), w, h);
			return Common::String::format("%s %s text height %d, %d to scroll", room.getAnim(cr->getLogo()).c_str(),
			                              room.getAnim(cr->getText()).c_str(), h, cr->getRemaining());
		};
		debugC(3, kDebugScript, "credits check %s: movie 58 asks for %s; rolling %d, busy %d, %s", ending ? "ending" : "skip", asked.c_str(),
		       cr->getStage() == 0, cr->isBusy(), part().c_str());
		next.clear();
		movie = -1;
		int stage = 0;
		uint t = 0;
		Common::String parts;
		while (t < kMaxTicks) {
			t++;
			if ((int)t == skipAt)
				cr->skip();
			_sound->stop();
			const bool asks = tickOnce(next, movie);
			if (cr->getStage() != stage) {
				stage = cr->getStage();
				parts += Common::String::format(" [part %d at tick %u: %s]", stage, t, stage < 3 ? part().c_str() : "done");
			}
			if (t == 30)
				parts += Common::String::format(" [tick 30: text at y %d]", room.getY(cr->getText()));
			if (asks)
				break;
		}
		debugC(3, kDebugScript, "credits check %s:%s to %s at tick %u; player '%s', StartRoom %s, %u movies seen", ending ? "ending" : "skip",
		       parts.c_str(), next.c_str(), t, _playerName.c_str(), _startRoom.c_str(), _seenMovies.size());
	};
	roll(true, -1);
	roll(false, 40);

	// Back to no game.
	_sound->stop();
	newGame();
	_startRoom.clear();
	_profile = Common::INIFile();
	_lastMovie = -1;
	_seenMovies.clear();
	_checking = false;
	debugC(3, kDebugScript, "options and credits check: done");
}

void ScoobyEngine::checkAccusation() {
	// The suspects and clues, and Big Head, driven through the engine: the
	// clues' buttons and hotspots, the pictures for met suspects, the clue
	// cards under the mouse, what Scrappy says, the idle line, the final
	// clue, each accusation's movie and reveal, and Big Head's lines.
	_checking = true;
	const bool showdown = getGameId() == "showdown";
	Common::String next;
	int movie;
	auto freshClass = [&](const char *name) {
		RoomClass *c;
		if (_roomClasses.tryGetVal(name, c)) {
			if (_roomClass == c)
				_roomClass = nullptr;
			delete c;
			_roomClasses.erase(name);
		}
	};
	const Common::String game = getFirstGameRoom();
	// A game with three clues, in slots 0, 1 and 3, and three suspects met.
	static const char *const kClues[2][3] = { { "ARTIE_MAP", "JENNY_COMPACT", "SHERIFF_SPURS" },
		{ "EWAN_FILM", "BRAD_COAT", "WIZARD_WAND" } };
	static const uint kSlots[] = { 0, 1, 3 };
	auto setup = [&](bool final) {
		newGame();
		setGameString("Whodunit", SuspectChooserRoom::getSuspect(getGameId(), 3));
		for (uint i = 0; i < 3; i++)
			setGameString(Common::String::format("CluesFound%u", kSlots[i]), kClues[showdown ? 0 : 1][i]);
		setGameFlag("FinalClue", final);
		_inventory->meetSuspect(SuspectChooserRoom::getSuspect(getGameId(), 0));
		_inventory->meetSuspect(SuspectChooserRoom::getSuspect(getGameId(), 2));
		_inventory->meetSuspect(SuspectChooserRoom::getSuspect(getGameId(), 3));
	};
	auto open = [&]() {
		freshClass("ROOM_Suspect_Chooser");
		_sound->stop();
		showRoom(game);
		showRoom("ROOM_Suspect_Chooser");
		_hover = -1;
		_mouseDown = false;
		return static_cast<SuspectChooserRoom *>(_roomClass);
	};
	auto heard = [&](const Common::String &line) {
		return line.empty() ? Common::String("-") : _sound->isClipPlaying(line) ? line : line + " (missing)";
	};

	for (int final = 0; final <= 1; final++) {
		setup(final);
		SuspectChooserRoom *sc = open();
		Room &room = *_currentRoom;
		const char *tag = final ? "final" : "clues";
		Common::String out;
		for (uint i = 0; i < sc->getClueCount(); i++) {
			const int b = sc->getButtonSprite(sc->getClueButton(i)), h = sc->getButtonSprite(sc->getHotspotButton(i));
			out += Common::String::format(" [%s at %d,%d; %s hit %d]", room.getAnim(b).c_str(), room.getX(b), room.getY(b), room.getAnim(h).c_str(),
			                              room.hasStepPolygon(h) ? 2 : 1);
		}
		Common::String pictures;
		for (uint p = 0; p < 5; p++)
			pictures += sc->getPictureButton(p) >= 0 ? Common::String::format(" %u:%s", p + 1, SuspectChooserRoom::getSuspect(getGameId(), p)) : "";
		int exitShown = -1;
		for (uint s = 0; s < room.getSpriteCount(); s++) {
			if (!room.isEngineSprite(s) && room.getAnim(s).hasSuffix("EXITICON"))
				exitShown = !room.isHidden(s);
		}
		debugC(3, kDebugScript, "accusation check %s: back to %s, says %s, %u clues:%s; pictures:%s; final %s, final hotspot %s; info %s; exit %d",
		       tag, sc->getReturnRoom().c_str(), heard(sc->getIntroLine()).c_str(), sc->getClueCount(), out.c_str(), pictures.c_str(),
		       sc->getFinalButton() >= 0 ? "yes" : "no", sc->getFinalHotspot() >= 0 ? room.getAnim(sc->getButtonSprite(sc->getFinalHotspot())).c_str() : "-",
		       sc->getInfoSprite() >= 0 ? (room.isHidden(sc->getInfoSprite()) ? "hidden" : "shown") : "none", exitShown);

		// The clue cards: each clue and each hotspot under the mouse, the
		// final clue's, and nothing.
		Common::String cards;
		auto card = [&](int sprite) {
			_hover = sprite;
			tickOnce(next, movie);
			const int info = sc->getInfoSprite();
			cards += Common::String::format(" %s:%s", sprite >= 0 ? room.getLabel(sprite).c_str() : "-",
			                                info >= 0 && !room.isHidden(info) ? room.getAnim(info).c_str() : "-");
		};
		for (uint i = 0; i < sc->getClueCount(); i++) {
			card(sc->getButtonSprite(sc->getClueButton(i)));
			card(sc->getButtonSprite(sc->getHotspotButton(i)));
		}
		if (final) {
			card(sc->getButtonSprite(sc->getFinalButton()));
			card(sc->getButtonSprite(sc->getFinalHotspot()));
		}
		card(-1);
		debugC(3, kDebugScript, "accusation check %s cards:%s", tag, cards.c_str());

		// Scrappy: each clue, each hotspot, the final clue's, and the met
		// suspects (before the final clue).
		Common::String lines;
		auto click = [&](int sprite) {
			_sound->stop();
			_hover = sprite;
			sc->click(sprite);
			lines += " " + heard(sc->getLastLine());
		};
		for (uint i = 0; i < sc->getClueCount(); i++) {
			click(sc->getButtonSprite(sc->getClueButton(i)));
			click(sc->getButtonSprite(sc->getHotspotButton(i)));
		}
		if (final) {
			click(sc->getButtonSprite(sc->getFinalButton()));
			click(sc->getButtonSprite(sc->getFinalHotspot()));
		} else {
			for (uint p = 0; p < 5; p++) {
				if (sc->getPictureButton(p) >= 0)
					click(sc->getButtonSprite(sc->getPictureButton(p)));
			}
		}
		_hover = -1;
		debugC(3, kDebugScript, "accusation check %s scrappy:%s", tag, lines.c_str());

		// The idle line.
		sc = open();
		_sound->stop();
		int idleAt = -1;
		for (int t = 1; t <= 800 && idleAt < 0; t++) {
			tickOnce(next, movie);
			if (sc->getLastLine() == "Suspect_and_Clues.SHAGGY.Click_On_Back_Button")
				idleAt = t;
		}
		debugC(3, kDebugScript, "accusation check %s idle: %d", tag, idleAt);
	}

	// A clue just found: input held until Scrappy names the newest.
	setup(false);
	setNewClue(true);
	SuspectChooserRoom *sc = open();
	const bool busy = sc->isBusy();
	_sound->stop();
	tickOnce(next, movie);
	debugC(3, kDebugScript, "accusation check new clue: busy %d, then %s, busy %d", busy, sc->getLastLine().c_str(), sc->isBusy());

	// The way back.
	setup(false);
	sc = open();
	next.clear();
	sc->leaveBy("OBJ_SUSPECT_CLUES_EXIT");
	uint t = 0;
	while (t < 100 && !tickOnce(next, movie))
		t++;
	debugC(3, kDebugScript, "accusation check exit: to %s", next.c_str());

	// Each suspect accused: the movie, then the reveal.
	Common::String accused;
	for (uint p = 0; p < 5; p++) {
		setup(true);
		for (uint q = 0; q < 5; q++)
			_inventory->meetSuspect(SuspectChooserRoom::getSuspect(getGameId(), q));
		sc = open();
		_requestedRoom.clear();
		sc->click(sc->getButtonSprite(sc->getPictureButton(p)));
		const Common::String asked = Common::String::format("%s/%d", _requestedRoom.empty() ? "-" : _requestedRoom.c_str(), _requestedMovie);
		_requestedRoom.clear();
		showRoom("ROOM_Suspect_Chooser");
		// The real loop ticks the room once more before it changes.
		next.clear();
		tickOnce(next, movie);
		accused += Common::String::format(" [%s: %s, then %s %s on %s, then %s]", SuspectChooserRoom::getSuspect(getGameId(), p), asked.c_str(),
		                                  next.c_str(), _bigHeadScene.c_str(), _bigHeadBackground.c_str(), _bigHeadNext.c_str());
		_requestedRoom.clear();
	}
	debugC(3, kDebugScript, "accusation check accused:%s", accused.c_str());

	// Big Head: the right reveal, line by line, each line ended at once.
	setup(true);
	startBigHead(SuspectChooserRoom::getRevealScene(getGameId(), getGameString("Whodunit"), true), showdown ? "BIG_HEAD_BG_P03" : "BIG_HEAD_BG_P16",
	             "ROOM_Credits");
	_requestedRoom.clear();
	freshClass("ROOM_Big_Head");
	showRoom("ROOM_Big_Head");
	BigHeadRoom *bh = static_cast<BigHeadRoom *>(_roomClass);
	Room &room = *_currentRoom;
	auto sprite = [&](int s) {
		return Common::String::format("%s/%s%s%s", room.getObjectName(s).c_str(), room.getAnim(s).c_str(),
		                              room.getDepth(s) ? Common::String::format(" depth %d", room.getDepth(s)).c_str() : "",
		                              room.isHidden(s) ? " hidden" : "");
	};
	debugC(3, kDebugScript, "accusation check big head: %s first; mouth %s, body %s, partner %s %s, background %s/%s; busy %d",
	       bh->getSpeaker().c_str(), sprite(bh->getMouth()).c_str(), sprite(bh->getBody()).c_str(), sprite(bh->getPartnerMouth()).c_str(),
	       sprite(bh->getPartnerBody()).c_str(), room.getObjectName(bh->getBackground()).c_str(), room.getAnim(bh->getBackground()).c_str(),
	       bh->isBusy());
	Common::String said;
	next.clear();
	movie = -1;
	t = 0;
	int line = 0;
	while (t < 400) {
		t++;
		const bool asks = tickOnce(next, movie);
		if (bh->getLine() != line && !bh->getLineClip().empty()) {
			line = bh->getLine();
			said += Common::String::format(" [%d %s%s%s; %s]", line, bh->getSpeaker().c_str(),
			                               bh->getPartner().empty() ? "" : " with ", bh->getPartner().c_str(), bh->getBackgroundAnim().c_str());
			_sound->stop();
		}
		if (asks)
			break;
	}
	debugC(3, kDebugScript, "accusation check big head lines:%s; then %s", said.c_str(), next.c_str());

	_sound->stop();
	newGame();
	_startRoom.clear();
	_profile = Common::INIFile();
	_bigHeadScene.clear();
	_bigHeadBackground.clear();
	_bigHeadNext.clear();
	_checking = false;
	debugC(3, kDebugScript, "accusation check: done");
}

void ScoobyEngine::checkMusicFade() {
	// The music's fade, driven through the engine: the steps of each fade
	// the original starts, where the rooms start them, and the music's
	// channel volume at a percent.
	_checking = true;
	Common::String next;
	int movie;
	auto changes = [&](uint ticks) {
		Common::String out;
		uint last = _sound->getMusicPercent();
		for (uint t = 1; t <= ticks; t++) {
			_sound->tickMusicFade();
			if (_sound->getMusicPercent() != last) {
				last = _sound->getMusicPercent();
				out += Common::String::format(" %u:%u", t, last);
			}
		}
		return out;
	};

	// The steps: down to 90 every 2 ticks from 100, then up to 100 every 4,
	// down again, cut short by up every 3, and down from 90 (already there).
	_sound->cancelMusicFade();
	_sound->fadeMusic(90, 2, false);
	debugC(3, kDebugScript, "fade check down 90/2:%s", changes(30).c_str());
	_sound->fadeMusic(100, 4, true);
	debugC(3, kDebugScript, "fade check up 100/4:%s", changes(50).c_str());
	_sound->fadeMusic(90, 2, false);
	const Common::String partway = changes(9);
	_sound->fadeMusic(100, 3, true);
	debugC(3, kDebugScript, "fade check down 90/2 for 9 ticks:%s, then up 100/3:%s", partway.c_str(), changes(40).c_str());
	_sound->fadeMusic(90, 2, false);
	changes(30);
	_sound->fadeMusic(90, 2, false);
	const Common::String at90Steps = changes(10);
	debugC(3, kDebugScript, "fade check down 90/2 at 90:%s, fading %d", at90Steps.c_str(), _sound->isMusicFading());
	_sound->fadeMusic(80, 2, false);
	changes(4);
	_sound->cancelMusicFade();
	debugC(3, kDebugScript, "fade check cancelled at %u%%: %s", _sound->getMusicPercent(), _sound->describeMusicFade().c_str());

	// The music's channel at a percent: its clip's volume, scaled.
	_sound->stop();
	_sound->fadeMusic(90, 1, false);
	changes(20);
	const Common::String clip = getGameId() == "showdown" ? "Suspect.Music" : "Track6.Music";
	_sound->play(clip, true);
	const uint volumeAt90 = _sound->getMusicVolume();
	_sound->cancelMusicFade();
	debugC(3, kDebugScript, "fade check %s: clip volume %u, at 90%% %u, at 100%% %u", clip.c_str(), _sound->getMusicClipVolume(), volumeAt90,
	       _sound->getMusicVolume());
	_sound->stop();

	// Where the rooms start them: a room's load, a scene starting and
	// ending, the title, and Big Head's opening.
	newGame();
	_sound->cancelMusicFade();
	showRoom(getFirstGameRoom());
	const Common::String load = _sound->describeMusicFade();
	_script->skip();
	_sound->cancelMusicFade();
	_script->startScene("Check.Scene_", true);
	const Common::String start = _sound->describeMusicFade();
	_script->skip();
	debugC(3, kDebugScript, "fade check rooms: load %s, scene start %s, scene end %s", load.c_str(), start.c_str(),
	       _sound->describeMusicFade().c_str());
	RoomClass *c;
	if (_roomClasses.tryGetVal("ROOM_Title_Screen", c)) {
		if (_roomClass == c)
			_roomClass = nullptr;
		delete c;
		_roomClasses.erase("ROOM_Title_Screen");
	}
	_sound->fadeMusic(80, 2, false);
	changes(6);
	for (int visit = 1; visit <= 4; visit++) {
		_requestedRoom.clear();
		showRoom("ROOM_Title_Screen");
	}
	debugC(3, kDebugScript, "fade check title: %u%%, %s", _sound->getMusicPercent(), _sound->describeMusicFade().c_str());
	_sound->stop();

	// Big Head: the opening starts 100 ticks in, and the music goes down.
	setGameString("Whodunit", SuspectChooserRoom::getSuspect(getGameId(), 3));
	startBigHead(SuspectChooserRoom::getRevealScene(getGameId(), getGameString("Whodunit"), true), getGameId() == "showdown" ? "BIG_HEAD_BG_P03" : "BIG_HEAD_BG_P16",
	             "ROOM_Credits");
	_requestedRoom.clear();
	if (_roomClasses.tryGetVal("ROOM_Big_Head", c)) {
		if (_roomClass == c)
			_roomClass = nullptr;
		delete c;
		_roomClasses.erase("ROOM_Big_Head");
	}
	_sound->cancelMusicFade();
	showRoom("ROOM_Big_Head");
	BigHeadRoom *bh = static_cast<BigHeadRoom *>(_roomClass);
	int started = -1, firstLine = -1, at90 = -1;
	for (int t = 1; t <= 200 && at90 < 0; t++) {
		tickOnce(next, movie);
		if (started < 0 && bh->isStarted())
			started = t;
		if (firstLine < 0 && bh->getLine() == 1)
			firstLine = t;
		if (_sound->getMusicPercent() == 90)
			at90 = t;
		_sound->stopClip(bh->getLineClip());
	}
	debugC(3, kDebugScript, "fade check big head: opening at tick %d, line 1 at tick %d, music at 90%% on tick %d", started, firstLine, at90);

	_sound->stop();
	_sound->cancelMusicFade();
	newGame();
	_startRoom.clear();
	_profile = Common::INIFile();
	_bigHeadScene.clear();
	_bigHeadBackground.clear();
	_bigHeadNext.clear();
	_checking = false;
	debugC(3, kDebugScript, "fade check: done");
}

Common::String ScoobyEngine::getFirstGameRoom() const {
	// The level select's rooms: Showdown's ROOM_P01_Town_Center, Phantom's
	// ROOM_P01_Drawbridge.
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		if (_art->getRoomName(i).hasPrefix("ROOM_P01_"))
			return _art->getRoomName(i);
	}
	return _art->getRoomName(0);
}

bool ScoobyEngine::showRoom(const Common::String &name) {
	const Common::String from = _currentRoom ? _currentRoom->getName() : Common::String();
	if (!_currentRoom)
		return false;
	// The room's own class sees each placement first (vtable +0x18).
	RoomClass *roomClass = _inventory ? getRoomClass(name) : nullptr;
	_currentRoom->setPlacementHook(roomClass);
	const bool loaded = _currentRoom->load(name);
	_currentRoom->setPlacementHook(nullptr);
	if (!loaded)
		return false;
	// 0x40B920 (game +0x48): the profile's StartRoom is the last room that
	// isn't a menu.
	if (!isMenuRoom(name))
		_startRoom = name;
	// The setup the room's class gave it (0x40B0F0): the music first.
	const RoomSetup setup = getRoomSetup(getGameId(), name);
	if (_music)
		_music->enterRoom(setup.music);
	if (_script) {
		_script->enter();
		_script->setHooks(roomClass);
	}
	if (_inventory)
		_inventory->enterRoom();
	// The end of the base entry: the opening and the timers (vtable +0x3C).
	if (_script)
		_script->setup(setup.opening, setup.snacksNeeded, from);
	_hover = _lastHover = -1;
	_currentRoom->setCursor("ANIM_CURSORARROW");
	// The room's own class, after the base entry.
	_requestedRoom.clear();
	_roomClass = roomClass;
	if (_roomClass)
		_roomClass->enter(from);
	// The load ends with the music coming up to full (0x435DC0 -> 0x434F30),
	// after the room's entry.
	if (_sound)
		_sound->fadeMusic(100, 3, true);

	_system->fillScreen(0);
	// A class that asks for another room at once (the title's movies) isn't
	// shown first.
	if (_requestedRoom.empty())
		drawRoom();
	_tickBase = _system->getMillis();
	_ticksDone = 0;

	if (debugChannelSet(1, kDebugGraphics))
		debugC(1, kDebugGraphics, "room %s drawn, %dx%d md5 %s", name.c_str(), _room.w, _room.h, hashSurface(_room).c_str());

	// The signed-in player's profile is written as each game room starts.
	// (When the original writes <name>.sav, game vtable +0x54, isn't
	// traced.)
	if (!isMenuRoom(name) && _playerSlot >= 0 && !_checking && _inventory) {
		const Common::Error err = saveGameState(_playerSlot, _playerName);
		debugC(1, kDebugScript, "player %s saved in slot %d: %s", _playerName.c_str(), _playerSlot, err.getDesc().c_str());
	}
	return true;
}

void ScoobyEngine::checkAllExits() {
	// Every exit of every room: its scene's lines as they resolve now, then
	// the walk-off with the scene skipped, tick by tick until the room
	// changes.
	const uint kMaxTicks = 10000;
	uint exits = 0, stuck = 0;
	Room room(*_art, _objectIni);
	RoomScript script(room, *_sound, _objectIni);
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String name = _art->getRoomName(i);
		if (!room.load(name))
			continue;
		script.enter();
		debugC(3, kDebugScript, "exit check %s characters %s", name.c_str(), script.describeCharacters().c_str());
		Common::Array<Common::String> objects;
		for (uint s = 0; s < room.getSpriteCount(); s++) {
			if (!room.getDestination(s).empty())
				objects.push_back(room.getObjectName(s));
		}
		for (uint k = 0; k < objects.size(); k++) {
			RoomScript::Exit exit = script.readExit(objects[k]);
			const Common::String lines = script.describeScene(exit.scene);
			room.load(name);
			script.enter();
			exit.scene.clear();
			script.startExit(exit);
			Common::String to;
			int movie = -1;
			uint t = 0;
			while (t < kMaxTicks && !script.getNextRoom(to, movie)) {
				room.tick();
				script.tick();
				t++;
			}
			exits++;
			if (!script.getNextRoom(to, movie)) {
				stuck++;
				debugC(3, kDebugScript, "exit check %s %s STUCK", name.c_str(), objects[k].c_str());
				continue;
			}
			debugC(3, kDebugScript, "exit check %s %s lines [%s] walks [%s] to %s at tick %u movie %d", name.c_str(),
			       objects[k].c_str(), lines.c_str(), script.getWalkLog().c_str(), to.c_str(), script.getTick() - 1, movie);
		}
	}
	debugC(3, kDebugScript, "exit check: %u exits, %u stuck", exits, stuck);
}

void ScoobyEngine::checkAllClickables() {
	// Every clickable of every room, clicked twice from a fresh room: what
	// each click plays and sounds, the state it moves to, and how many ticks
	// until the object rests. The ghost is off.
	const uint kMaxTicks = 10000;
	uint clicks = 0, stuck = 0;
	Room room(*_art, _objectIni);
	RoomScript script(room, *_sound, _objectIni);
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String name = _art->getRoomName(i);
		if (!room.load(name))
			continue;
		Common::Array<uint> sprites;
		for (uint s = 0; s < room.getSpriteCount(); s++) {
			if (room.isClickable(s))
				sprites.push_back(s);
		}
		for (uint k = 0; k < sprites.size(); k++) {
			room.load(name);
			script.resetClickableStates();
			script.enter();
			for (int n = 1; n <= 2; n++) {
				// A sprite that selects no frame can't be hit, so it can't be
				// clicked.
				if (!room.hasFrame(sprites[k])) {
					debugC(3, kDebugScript, "click %s %s %d gone", name.c_str(), room.getObjectName(sprites[k]).c_str(), n);
					break;
				}
				script.clickClickable(sprites[k]);
				room.tick();
				script.tick();
				const uint32 fired = script.getTick() - 1;
				uint t = 0;
				while (t < kMaxTicks && script.isClickablePlaying(sprites[k])) {
					room.tick();
					script.tick();
					t++;
				}
				clicks++;
				if (script.isClickablePlaying(sprites[k])) {
					stuck++;
					debugC(3, kDebugScript, "click %s %s %d STUCK", name.c_str(), room.getObjectName(sprites[k]).c_str(), n);
					break;
				}
				debugC(3, kDebugScript, "click %s %s %d %s rest %u", name.c_str(), room.getObjectName(sprites[k]).c_str(), n,
				       script.getClickLog().c_str(), script.getTick() - 1 - fired);
			}
		}
	}
	debugC(3, kDebugScript, "click check: %u clicks, %u stuck", clicks, stuck);
}

void ScoobyEngine::checkAllInventory() {
	// The toolbar driving in and out; then, from a new game and a fresh
	// room, a click on every inventory object, and on every requires object
	// with the arrow, with the item it asks for, and with another item. The
	// wrong-item line goes to the first speaker present (no random draw).
	const uint kMaxTicks = 2000;
	Room room(*_art, _objectIni);
	RoomScript script(room, *_sound, _objectIni);
	Inventory inv(room, *_sound, script, _objectIni);
	uint checks = 0, stuck = 0;

	struct Ticker {
		Room &room;
		RoomScript &script;
		Inventory &inv;
		void tick() {
			room.tick();
			inv.tickObjects();
			script.tick();
			inv.tickGame();
		}
		void enter(const Common::String &name) {
			room.load(name);
			script.enter();
			inv.enterRoom();
		}
		// Ticks until the toolbar is in @p state, or -1.
		int until(Inventory::State state, uint max) {
			for (uint t = 1; t <= max; t++) {
				tick();
				if (inv.getState() == state)
					return t;
			}
			return -1;
		}
	} run = { room, script, inv };

	// Whether each room shows the toolbar on entry, or its class hides it
	// (0x407CC0).
	uint hiddenRooms = 0;
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String name = _art->getRoomName(i);
		inv.reset();
		run.enter(name);
		const bool shown = inv.getState() != Inventory::kStateBusy;
		hiddenRooms += !shown;
		debugC(3, kDebugScript, "inventory check room %s toolbar %s at x %d", name.c_str(), shown ? "shown" : "hidden",
		       inv.getVanX());
	}
	debugC(3, kDebugScript, "inventory check rooms: %u of %u hide the toolbar", hiddenRooms, _art->getRoomCount());

	// The inventory objects by name, and the cursor each gives.
	Common::Array<Common::String> items;
	Common::HashMap<Common::String, Common::String> cursorOf;
	const Common::INIFile::SectionList sections = _objectIni.getSections();
	for (Common::INIFile::SectionList::const_iterator it = sections.begin(); it != sections.end(); ++it) {
		Common::String id, cursor;
		if (!_objectIni.getKey("ID", it->name, id))
			continue;
		id.trim();
		if (!id.equalsIgnoreCase("inventoryobject") || !_objectIni.getKey("usecursor", it->name, cursor))
			continue;
		cursor.trim();
		items.push_back(it->name);
		cursorOf[it->name] = cursor;
	}
	Common::sort(items.begin(), items.end());

	// The toolbar, in the first room that shows it (Phantom's first room,
	// P31_KST, never builds it).
	Common::String first = _art->getRoomName(0);
	for (uint i = 0; i < _art->getRoomCount() && isToolbarHidden(first); i++)
		first = _art->getRoomName(i);
	inv.reset();
	run.enter(first);
	debugC(3, kDebugScript, "inventory check toolbar parked %s", inv.describeToolbar().c_str());
	inv.click(inv.getVanSprite());
	const int in = run.until(Inventory::kStateOpen, kMaxTicks);
	debugC(3, kDebugScript, "inventory check toolbar open after %d ticks at x %d %s", in, inv.getVanX(), inv.describeToolbar().c_str());
	inv.click(inv.getVanSprite());
	const int out = run.until(Inventory::kStateParked, kMaxTicks);
	debugC(3, kDebugScript, "inventory check toolbar parked after %d ticks [%s]", out, inv.takeLog().c_str());
	checks++;
	if (in < 0 || out < 0)
		stuck++;

	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String name = _art->getRoomName(i);
		if (!room.load(name))
			continue;
		Common::Array<uint> pickups, requiresObjs;
		for (uint s = 0; s < room.getSpriteCount(); s++) {
			if (room.isInventoryObject(s))
				pickups.push_back(s);
			else if (room.isRequiresObject(s))
				requiresObjs.push_back(s);
		}

		for (uint k = 0; k < pickups.size(); k++) {
			inv.reset();
			run.enter(name);
			const Common::String obj = room.getObjectName(pickups[k]);
			inv.takeLog();
			inv.click(pickups[k]);
			uint t = 0;
			Common::String next;
			int movie = -1;
			while (t < kMaxTicks && !inv.hasItem(obj)) {
				run.tick();
				t++;
			}
			const bool movieWait = inv.getNextRoom(next, movie);
			checks++;
			if (!inv.hasItem(obj))
				stuck++;
			const bool hidden = room.isHidden(pickups[k]);
			const int x = inv.getVanX();
			Common::String log = inv.takeLog();
			run.enter(name);
			debugC(3, kDebugScript, "inventory check pickup %s %s: [%s] in %u ticks, slot %s, toolbar x %d, %s, %s%s", name.c_str(),
			       obj.c_str(), log.c_str(), t, inv.getSlot(0).c_str(), x,
			       hidden ? "hidden" : "shown", room.isHidden(pickups[k]) ? "hidden on return" : "shown on return",
			       movieWait ? Common::String::format(", then movie %d and %s", movie, next.c_str()).c_str() : "");
		}

		for (uint k = 0; k < requiresObjs.size(); k++) {
			const Common::String obj = room.getObjectName(requiresObjs[k]);
			Common::String required;
			_objectIni.getKey("requiredanim", obj, required);
			required.trim();

			// The arrow.
			inv.reset();
			run.enter(name);
			inv.takeLog();
			inv.click(requiresObjs[k]);
			run.tick();
			Common::String line = Common::String::format("arrow [%s] cursor %s", inv.takeLog().c_str(), inv.getCursor().c_str());

			// The item it asks for, then another.
			for (int pass = 0; pass < 2; pass++) {
				// The first item, by name, that gives the cursor (pass 0) or
				// another (pass 1).
				Common::String item;
				for (uint n = 0; n < items.size() && item.empty(); n++) {
					if ((cursorOf[items[n]] == required) == (pass == 0))
						item = items[n];
				}
				if (item.empty()) {
					line += Common::String::format("; no item gives %s", required.c_str());
					continue;
				}
				inv.reset();
				run.enter(name);
				inv.addItem(item);
				// In a room that hides the toolbar, adding an item clears the
				// flag (0x406DD0) and the van comes back only as far as
				// parked, where a slot still takes a click (0x406A70).
				const int open = run.until(isToolbarHidden(name) ? Inventory::kStateParked : Inventory::kStateOpen, kMaxTicks);
				inv.takeLog();
				inv.click(inv.getItemSprite(0));
				run.tick();
				const Common::String held = inv.getCursor();
				inv.click(requiresObjs[k]);
				run.tick();
				const Common::String after = inv.getCursor();
				run.tick();
				checks++;
				if (open < 0)
					stuck++;
				line += Common::String::format("; %s %s held %s [%s] cursor %s", pass == 0 ? "item" : "other", item.c_str(), held.c_str(),
				                               inv.takeLog().c_str(), after.c_str());
			}
			debugC(3, kDebugScript, "inventory check requires %s %s (%s): %s", name.c_str(), obj.c_str(), required.c_str(), line.c_str());
			checks++;
		}
	}
	debugC(3, kDebugScript, "inventory check: %u checks, %u stuck", checks, stuck);
}

void ScoobyEngine::checkAllHints() {
	// From a new game and a fresh room each time: Scrappy's hint for every
	// exit, and for every character with a hint clip once met; a snack for
	// every Scooby and Shaggy, and the ticks until they are idle again;
	// and the empty snack box. No random draws: the first choice each time.
	const uint kMaxTicks = 2000;
	Room room(*_art, _objectIni);
	RoomScript script(room, *_sound, _objectIni);
	Inventory inv(room, *_sound, script, _objectIni);
	uint hints = 0, snacks = 0, stuck = 0;
	struct Ticker {
		Room &room;
		RoomScript &script;
		Inventory &inv;
		void tick() {
			room.tick();
			inv.tickObjects();
			script.tick();
			inv.tickGame();
		}
		void fresh(const Common::String &name) {
			inv.reset();
			room.load(name);
			script.enter();
			inv.enterRoom();
			inv.takeLog();
		}
		// As the engine does: the cursor over the sprite, a hint if one
		// shows, then the click on the next tick.
		void click(int sprite) {
			inv.cursorShown(inv.getCursorFor(sprite));
			inv.clickHint(sprite);
			inv.click(sprite);
			tick();
		}
	} run = { room, script, inv };

	// The empty snack box, in the first room.
	run.fresh(_art->getRoomName(0));
	inv.setSnacks(0);
	run.click(inv.getSnackBoxSprite());
	debugC(3, kDebugScript, "hint check empty snack box: [%s] cursor %s", inv.takeLog().c_str(), inv.getCursor().c_str());
	_sound->stop();
	inv.reset();

	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String name = _art->getRoomName(i);
		if (!room.load(name))
			continue;
		Common::Array<uint> exits;
		for (uint s = 0; s < room.getSpriteCount(); s++) {
			if (!room.getDestination(s).empty())
				exits.push_back(s);
		}
		script.enter();
		Common::Array<Common::String> people;
		for (uint c = 0; c < script.getCharacters().size(); c++) {
			const Character &ch = script.getCharacters()[c];
			if (ch.getSprite() >= 0)
				people.push_back(ch.getName());
		}

		// Hints: exits, then characters with a hint clip.
		for (uint k = 0; k < exits.size() + people.size(); k++) {
			run.fresh(name);
			int sprite;
			Common::String what;
			if (k < exits.size()) {
				sprite = exits[k];
				what = room.getObjectName(sprite);
			} else {
				const Common::String person = people[k - exits.size()];
				if (!_sound->findClip("Global.Scrappy." + person))
					continue;
				Character *c = script.getCharacter(person.c_str());
				sprite = c->getSprite();
				what = person;
				inv.meetSuspect(person);
			}
			run.click(inv.getScrappySprite());
			const Common::String shown = inv.getCursorFor(sprite);
			run.click(sprite);
			hints++;
			debugC(3, kDebugScript, "hint check %s %s: shows %s [%s] cursor %s", name.c_str(), what.c_str(), shown.c_str(),
			       inv.takeLog().c_str(), inv.getCursor().c_str());
			_sound->stop();
		}

		// Snacks.
		static const char *const kEaters[] = { "SCOOBY", "SHAGGY" };
		for (uint e = 0; e < ARRAYSIZE(kEaters); e++) {
			run.fresh(name);
			Character *c = script.getCharacter(kEaters[e]);
			if (!c)
				continue;
			run.click(inv.getSnackBoxSprite());
			const Common::String held = inv.getCursor();
			run.click(c->getSprite());
			uint t = 0;
			while (t < kMaxTicks && c->getState() != Character::kStateIdle) {
				run.tick();
				t++;
			}
			snacks++;
			if (c->getState() != Character::kStateIdle)
				stuck++;
			debugC(3, kDebugScript, "snack check %s %s: held %s [%s] idle after %u ticks, %u left, cursor %s", name.c_str(),
			       kEaters[e], held.c_str(), inv.takeLog().c_str(), t, inv.getSnacks(), inv.getCursor().c_str());
			_sound->stop();
		}
	}
	inv.reset();
	debugC(3, kDebugScript, "hint check: %u hints, %u snacks, %u stuck", hints, snacks, stuck);
}

void ScoobyEngine::checkAllRooms() {
	// Each room at tick 0 and after kCheckTicks ticks, when a sprite at 3
	// ticks per step shows step 12 mod its step count.
	const uint kCheckTicks = 37;
	uint bad = 0;
	Graphics::Surface surface;
	Room room(*_art, _objectIni);
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String name = _art->getRoomName(i);
		if (!room.load(name)) {
			debugC(3, kDebugGraphics, "room check %s BAD", name.c_str());
			bad++;
			continue;
		}
		room.draw(surface);
		debugC(3, kDebugGraphics, "room check %s %dx%d md5 %s", name.c_str(), surface.w, surface.h, hashSurface(surface).c_str());
		debugC(3, kDebugGraphics, "room check %s order %s", name.c_str(), room.getOrder().c_str());

		if (debugChannelSet(3, kDebugInput)) {
			// Hit-test a 4-pixel grid; hash the hit labels in row order.
			Common::String hits;
			uint points = 0, misses = 0;
			for (int y = 0; y < surface.h; y += 4) {
				for (int x = 0; x < surface.w; x += 4) {
					const int hit = room.hitTest(x, y);
					hits += hit >= 0 ? room.getLabel(hit) : Common::String("-");
					hits += '\n';
					points++;
					misses += hit < 0;
				}
			}
			Common::MemoryReadStream stream((const byte *)hits.c_str(), hits.size());
			debugC(3, kDebugInput, "hit check %s %u points %u misses md5 %s", name.c_str(), points, misses,
			       Common::computeStreamMD5AsString(stream).c_str());
		}
		for (uint t = 0; t < kCheckTicks; t++)
			room.tick();
		room.draw(surface);
		debugC(3, kDebugGraphics, "room check %s t%u md5 %s", name.c_str(), kCheckTicks, hashSurface(surface).c_str());
	}
	surface.free();
	debugC(3, kDebugGraphics, "room check: %u rooms, %u bad", _art->getRoomCount(), bad);
}

} // End of namespace Scooby
