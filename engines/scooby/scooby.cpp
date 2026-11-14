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
	_gameDescription(gameDesc), _randomSource("Scooby"), _art(nullptr), _rooms(nullptr) {
	g_engine = this;

	// Gen 1 discs keep the game files in a "scooby" directory, and detection
	// accepts the disc root, so search that directory too.
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "scooby");
}

ScoobyEngine::~ScoobyEngine() {
	_room.free();
	delete _rooms;
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
	_rooms = new RoomRenderer(*_art, _objectIni);

	if (debugChannelSet(3, kDebugGraphics)) {
		checkAllCels(*_art);
		checkAllRooms();
	}

	// No game logic yet: show a room as it is set up on entry, and wait.
	// The console's "room" command switches rooms. A "start_room" key in the
	// game's config domain picks the first one, for testing.
	Common::String room = _art->getRoomName(0);
	if (ConfMan.hasKey("start_room"))
		room = ConfMan.get("start_room");
	showRoom(room);

	Common::Event e;
	Graphics::FrameLimiter limiter(g_system, 60);
	while (!shouldQuit()) {
		while (g_system->getEventManager()->pollEvent(e)) {
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

bool ScoobyEngine::showRoom(const Common::String &name) {
	Common::String order;
	if (!_rooms || !_rooms->render(name, _room, order))
		return false;

	// Rooms wider than the screen (1920 pixels) show their left edge until
	// scrolling exists.
	_system->fillScreen(0);
	_system->copyRectToScreen(_room.getPixels(), _room.pitch, 0, 0, MIN<int>(_room.w, 640), MIN<int>(_room.h, 480));

	if (debugChannelSet(1, kDebugGraphics))
		debugC(1, kDebugGraphics, "room %s drawn, %dx%d md5 %s", name.c_str(), _room.w, _room.h, hashSurface(_room).c_str());
	return true;
}

void ScoobyEngine::checkAllRooms() {
	uint bad = 0;
	Graphics::Surface surface;
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String name = _art->getRoomName(i);
		Common::String order;
		if (!_rooms->render(name, surface, order)) {
			debugC(3, kDebugGraphics, "room check %s BAD", name.c_str());
			bad++;
			continue;
		}
		debugC(3, kDebugGraphics, "room check %s %dx%d md5 %s", name.c_str(), surface.w, surface.h, hashSurface(surface).c_str());
		debugC(3, kDebugGraphics, "room check %s order %s", name.c_str(), order.c_str());
	}
	surface.free();
	debugC(3, kDebugGraphics, "room check: %u rooms, %u bad", _art->getRoomCount(), bad);
}

} // End of namespace Scooby
