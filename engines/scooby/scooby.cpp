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
#include "scooby/room.h"
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
	_gameDescription(gameDesc), _randomSource("Scooby"), _art(nullptr), _currentRoom(nullptr), _tickBase(0), _ticksDone(0), _hover(-1), _lastHover(-1) {
	g_engine = this;

	// Gen 1 discs keep the game files in a "scooby" directory, and detection
	// accepts the disc root, so search that directory too.
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "scooby");
}

ScoobyEngine::~ScoobyEngine() {
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

	if (debugChannelSet(3, kDebugGraphics)) {
		checkAllCels(*_art);
		checkAllRooms();
	}

	// No game logic yet: show a room as it is set up on entry and let its
	// sprites play. The console's "room" command switches rooms. A
	// "start_room" key in the game's config domain picks the first one, for
	// testing.
	Common::String room = _art->getRoomName(0);
	if (ConfMan.hasKey("start_room"))
		room = ConfMan.get("start_room");
	showRoom(room);

	Common::Event e;
	Graphics::FrameLimiter limiter(g_system, 60);
	uint32 lastLog = 0;
	while (!shouldQuit()) {
		bool changed = false;
		while (g_system->getEventManager()->pollEvent(e)) {
			if (e.type == Common::EVENT_MOUSEMOVE) {
				updateHover(e.mouse.x, e.mouse.y);
				changed = true;
			} else if (e.type == Common::EVENT_LBUTTONDOWN) {
				updateHover(e.mouse.x, e.mouse.y);
				changed = true;
				// 0x402130 logs to the "Exits" channel and starts the walk
				// and movie to destinationroom=; until those exist, go there
				// directly.
				if (_hover >= 0 && !_currentRoom->getDestination(_hover).empty()) {
					const Common::String from = _currentRoom->getLabel(_hover);
					const Common::String to = _currentRoom->getDestination(_hover);
					debugC(1, kDebugInput, "exit %s clicked: going to %s", from.c_str(), to.c_str());
					if (showRoom(to))
						updateHover(e.mouse.x, e.mouse.y);
				}
			}
		}

		// 0x435BC0: the game owes elapsed_ms * 36 / 1000 ticks since the
		// room started; run them all, then draw once.
		const uint32 elapsed = _system->getMillis() - _tickBase;
		const uint32 owed = (uint32)((uint64)elapsed * Room::kTicksPerSecond / 1000) - _ticksDone;
		for (uint32 i = 0; i < owed; i++)
			changed = _currentRoom->tick() || changed;
		_ticksDone += owed;
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
	const Common::String rollover = _hover >= 0 ? _currentRoom->getRollover(_hover) : Common::String();
	const char *cursor = rollover.empty() ? "ANIM_CURSORARROW" : rollover.c_str();
	_currentRoom->setCursorPosition(x, y);
	if (_hover != _lastHover) {
		_lastHover = _hover;
		debugC(1, kDebugInput, "hover %s, cursor %s", _hover >= 0 ? _currentRoom->getLabel(_hover).c_str() : "-", cursor);
	}
	_currentRoom->setCursor(cursor);
}

bool ScoobyEngine::showRoom(const Common::String &name) {
	if (!_currentRoom || !_currentRoom->load(name))
		return false;
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
