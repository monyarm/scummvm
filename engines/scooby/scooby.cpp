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
#include "scooby/room_script.h"
#include "scooby/sound.h"
#include "common/algorithm.h"
#include "common/config-manager.h"
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
	delete _inventory;
	delete _script;
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
		checkAllExits();
		checkAllClickables();
		checkAllInventory();
		checkAllMovies();
	}

	// No game logic yet: show a room as it is set up on entry and let its
	// sprites play. The console's "room" command switches rooms. A
	// "start_room" key in the game's config domain picks the first one, for
	// testing.
	Common::String room = _art->getRoomName(0);
	if (ConfMan.hasKey("start_room"))
		room = ConfMan.get("start_room");
	showRoom(room);
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

		bool changed = false;
		while (g_system->getEventManager()->pollEvent(e)) {
			// While a scene or an exit runs, the cursor is hidden and clicks
			// are ignored (game +0x24).
			if (e.type == Common::EVENT_MOUSEMOVE) {
				_mousePos = e.mouse;
				if (_script->isBusy()) {
					_currentRoom->setCursorPosition(e.mouse.x, e.mouse.y);
				} else {
					updateHover(e.mouse.x, e.mouse.y);
					changed = true;
				}
			} else if (e.type == Common::EVENT_LBUTTONDOWN && !_script->isBusy() && !_inventory->isInputBlocked()) {
				_mousePos = e.mouse;
				updateHover(e.mouse.x, e.mouse.y);
				changed = true;
				// The toolbar, inventory and requires objects take the click on
				// the next tick, and so does the game's update (0x4098B0).
				_inventory->click(_hover);
				// Exits and clickables answer only the arrow (0x402130,
				// 0x401440).
				if (_inventory->isDefaultCursor()) {
					if (_hover >= 0 && !_currentRoom->getDestination(_hover).empty())
						clickExit(_hover);
					// A clickable's update takes the click on the next tick,
					// while the cursor shows its rollover (0x401440).
					else if (_hover >= 0 && _currentRoom->isClickable(_hover))
						_script->clickClickable(_hover);
				}
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
			changed = changed || _script->isBusy() || _inventory->getState() != Inventory::kStateParked;
			if (_script->getNextRoom(nextRoom, movie) || _inventory->getNextRoom(nextRoom, movie))
				break;
		}
		_ticksDone += owed;
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
	// The hovered object's rollover (vtable +0x18), which is empty for
	// every object while an item is the cursor; else the game's cursor.
	Common::String rollover;
	if (_hover >= 0) {
		if (_inventory->handles(_hover))
			rollover = _inventory->getRollover(_hover);
		else if (_inventory->isDefaultCursor())
			rollover = _currentRoom->getRollover(_hover);
	}
	const Common::String cursorName = rollover.empty() ? _inventory->getCursor() : rollover;
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
	_script->startExit(exit);
}

bool ScoobyEngine::showRoom(const Common::String &name) {
	if (!_currentRoom || !_currentRoom->load(name))
		return false;
	if (_script)
		_script->enter();
	if (_inventory)
		_inventory->enterRoom();
	_hover = _lastHover = -1;
	_currentRoom->setCursor("ANIM_CURSORARROW");

	_system->fillScreen(0);
	drawRoom();
	_tickBase = _system->getMillis();
	_ticksDone = 0;

	if (debugChannelSet(1, kDebugGraphics))
		debugC(1, kDebugGraphics, "room %s drawn, %dx%d md5 %s", name.c_str(), _room.w, _room.h, hashSurface(_room).c_str());
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

	// The toolbar, in the first room.
	const Common::String first = _art->getRoomName(0);
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
				const int open = run.until(Inventory::kStateOpen, kMaxTicks);
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
