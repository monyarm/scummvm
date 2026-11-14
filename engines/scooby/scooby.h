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
#include "engines/engine.h"
#include "graphics/surface.h"

#include "scooby/detection.h"

namespace Scooby {

class ArtArchive;
class Room;

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
	void drawRoom();
	void updateHover(int x, int y);
	int _hover;     // sprite under the mouse, or -1
	int _lastHover;

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
	 * Draws a room as it is set up on entry: its layers and every placement
	 * at step 0, in the original's draw order
	 */
	bool showRoom(const Common::String &name);

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	bool hasFeature(EngineFeature f) const override {
		return (f == kSupportsReturnToLauncher);
	}
};

extern ScoobyEngine *g_engine;
#define SHOULD_QUIT ::Scooby::g_engine->shouldQuit()

} // End of namespace Scooby

#endif // SCOOBY_H
