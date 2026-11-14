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
#include "scooby/console.h"
#include "common/config-manager.h"
#include "common/debug-channels.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/framelimiter.h"
#include "graphics/pixelformat.h"

namespace Scooby {

ScoobyEngine *g_engine;

ScoobyEngine::ScoobyEngine(OSystem *syst, const ScoobyGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Scooby"), _art(nullptr) {
	g_engine = this;

	// Gen 1 discs keep the game files in a "scooby" directory, and detection
	// accepts the disc root, so search that directory too.
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "scooby");
}

ScoobyEngine::~ScoobyEngine() {
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

	// Nothing to show yet: pump events until the user quits.
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

} // End of namespace Scooby
