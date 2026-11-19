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

#ifndef SCOOBY_MAZE_ROOMS_H
#define SCOOBY_MAZE_ROOMS_H

#include "common/str.h"

#include "scooby/room.h"
#include "scooby/room_class.h"

namespace Scooby {

/**
 * Phantom's hedge maze, ROOM_Hedge_Maze (0x42AA90, vtable 0x45F3C4): the gang
 * rides a unicycle from cell to cell through a grid maze. Each cell carries a
 * 4-bit mask of which of its four sides (up/down/right/left) is a passage
 * rather than a wall; the four directional buttons show a passage (EXIT anim)
 * or a wall (OVERLAY anim) accordingly, and clicking a passage rides to the
 * adjacent cell. The maze map item (OBJ_INV_MAZE_MAP) opens ROOM_Maze_Map.
 *
 * The maze position is (X=+0xc4, Y=+0xc8); the cell index is Y + X*10. The
 * per-level mask and path-indicator tables (enter 0x42AEB0) are the binary's
 * resolved code constants; the directional move handlers (0x42C0F0/200/310/420)
 * step the position and play the unicycle clips.
 */
class HedgeMazeRoom : public RoomClass {
public:
	explicit HedgeMazeRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x42AEB0
	void roomLeft() override;                                                        // 0x42BAE0
	void tick() override;                                                            // 0x42AD00
	bool click(int sprite) override;
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x42BB00

	enum { kGrid = 100, kCols = 10 };

	// A direction button (claim's OBJ_HEDGE_MAZE_BACK/FRONT/R/L): its mask bit
	// (== its dir code), the (dX, dY) it steps, the unicycle clip it plays off
	// and the one it lands on, and its passage/wall anims.
	struct Move {
		const char *object;
		int code;        // +0xc0: 1 up, 4 down, 2 right, 8 left
		int dx, dy;
		const char *startClip, *endClip;
		const char *overlay, *exitAnim;
	};
	static const Move kMoves[4]; // back, front, r, l

	// The maze geometry the check reads (populated by enter / computeStart).
	int getGridCell(uint i) const { return i < kGrid ? _grid[i] : -1; }
	int getPathCell(uint i) const { return i < kGrid ? _path[i] : -1; }
	int getX() const { return _x; }
	int getY() const { return _y; }
	int getDir() const { return _dir; }

	// The per-level table (cell -> mask), the path table, and the +0x440 patch
	// cell, straight from the binary; and the start cell for an entry source.
	struct Cell { int cell, value; };
	static void levelMask(int level, const Cell *&cells, uint &count);
	static void levelPath(int level, const Cell *&cells, uint &count);
	static int patchCell(int level);            // forced to 0xb when the center path is taken
	struct Start { int x, y, dir, patch; };
	static Start computeStart(const Common::String &from, int level, bool centerFlag);
	// Moving into one of these cells exits to ROOM_P06_Garden (top of enter).
	static void gardenExits(int level, const int *&cells, uint &count, Start &reposition);

	// The clips, for the check.
	static const char *kMusic;
	static const char *kUnicycle[9]; // idle, startL, startR, startU, startD, endU, endD, endL, endR
	static const char *kIdle[3];

private:
	void loadLevel(int level, bool centerPath);
	void drawCell();                 // the four buttons' passage/wall anims for the current cell
	void playClip(const Common::String &name);

	int _grid[kGrid];
	int _path[kGrid];
	int _x = 2, _y = 4, _dir = 1;    // +0xc4, +0xc8, +0xc0
	bool _centerPath = false;        // +0x440
	int _button[4] = { -1, -1, -1, -1 }; // the back/front/r/l sprites (+0x408..+0x414)
};

/**
 * Phantom's maze map, ROOM_Maze_Map (0x42A7D0, vtable 0x45F384): the close-up
 * of the maze map item. Four clickable markers jump the game straight to a
 * maze location (claim 0x42A850, handler 0x42A9A0).
 */
class MazeMapRoom : public RoomClass {
public:
	explicit MazeMapRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	bool click(int sprite) override;
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42A850

	struct Marker { const char *object, *room; };
	static const Marker kMarkers[4];
	static const char *kMusic; // "1"

private:
	int _marker[4] = { -1, -1, -1, -1 };
};

/** The maze's room class for @p room, or nullptr. */
RoomClass *createMazeRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_MAZE_ROOMS_H
