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

#include "common/debug.h"

#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/rooms/phantomknight/maze.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const int kLoop = 1;
static const int kOnce = 3;

// The cell mask's bits, which are also the move's +0xc0 dir codes.
enum { kUp = 1, kDown = 4, kRight = 2, kLeft = 8 };

const char *HedgeMazeRoom::kMusic = "Hedge_Maze.Music";
const char *HedgeMazeRoom::kUnicycle[9] = {
	"Hedge_Maze.Unicycle_Idle", "Hedge_Maze.Unicycle_Start_Left", "Hedge_Maze.Unicycle_Start_Right",
	"Hedge_Maze.Unicycle_Start_Up", "Hedge_Maze.Unicycle_Start_Down", "Hedge_Maze.Unicycle_End_Up",
	"Hedge_Maze.Unicycle_End_Down", "Hedge_Maze.Unicycle_End_Left", "Hedge_Maze.Unicycle_End_Right"
};
const char *HedgeMazeRoom::kIdle[3] = { "Hedge_Maze.240A_", "Hedge_Maze.240A1_", "Hedge_Maze.240A2_" };

// The four directional buttons (claim 0x42BB00), each with the bit it reads, the
// step it takes (0x42C0F0/200/310/420), the unicycle clip it rides off on and the
// one it lands on (enter's switch), and its passage/wall anims.
const HedgeMazeRoom::Move HedgeMazeRoom::kMoves[4] = {
	{ "OBJ_HEDGE_MAZE_BACK", kUp, 0, -1, "Hedge_Maze.Unicycle_Start_Up", "Hedge_Maze.Unicycle_End_Down",
	  "ANIM_HEDGE_MAZE_BACKOVERLAY", "ANIM_HEDGE_MAZE_BACKEXIT" },
	{ "OBJ_HEDGE_MAZE_FRONT", kDown, 0, 1, "Hedge_Maze.Unicycle_Start_Down", "Hedge_Maze.Unicycle_End_Up",
	  "ANIM_HEDGE_MAZE_FRONTOVERLAY", "ANIM_HEDGE_MAZE_FRONTEXIT" },
	{ "OBJ_HEDGE_MAZE_R", kRight, 1, 0, "Hedge_Maze.Unicycle_Start_Right", "Hedge_Maze.Unicycle_End_Left",
	  "ANIM_HEDGE_MAZE_ROVERLAY", "ANIM_HEDGE_MAZE_REXIT" },
	{ "OBJ_HEDGE_MAZE_L", kLeft, -1, 0, "Hedge_Maze.Unicycle_Start_Left", "Hedge_Maze.Unicycle_End_Right",
	  "ANIM_HEDGE_MAZE_LOVERLAY", "ANIM_HEDGE_MAZE_LEXIT" },
};

// The per-level cell tables (cell = Y + X*10), the binary's resolved code
// constants from enter 0x42AEB0, derived mechanically from the decompile.
static const HedgeMazeRoom::Cell kMask0[] = {
	{0,6}, {1,5}, {2,3}, {3,6}, {4,1}, {10,9}, {11,0}, {12,13}, {13,15}, {14,3},
	{20,6}, {21,14}, {22,6}, {23,13}, {24,15}, {30,12}, {31,9}, {32,12}, {33,5}, {34,9}
};
static const HedgeMazeRoom::Cell kMask1[] = {
	{1,6}, {2,7}, {3,5}, {4,3}, {5,6}, {6,1}, {10,3}, {11,14}, {12,9}, {13,6}, {14,13}, {15,15}, {16,3},
	{20,10}, {21,10}, {22,6}, {23,11}, {24,6}, {25,13}, {26,15}, {30,12}, {31,11}, {32,14}, {33,9},
	{34,14}, {35,7}, {36,11}, {41,8}, {42,8}, {44,8}, {45,12}, {46,9}
};
static const HedgeMazeRoom::Cell kMask2[] = {
	{0,4}, {1,5}, {2,7}, {3,1}, {4,6}, {5,7}, {6,5}, {7,3}, {8,6}, {9,1}, {10,7}, {11,3}, {12,14}, {13,3},
	{14,14}, {15,9}, {16,6}, {17,13}, {18,15}, {19,3}, {20,14}, {21,9}, {22,10}, {23,10}, {24,10}, {25,6},
	{26,11}, {27,6}, {28,13}, {29,15}, {30,12}, {31,7}, {32,9}, {33,12}, {34,11}, {35,14}, {36,9}, {37,14},
	{38,7}, {39,11}, {40,6}, {41,15}, {42,7}, {43,3}, {44,8}, {45,10}, {46,2}, {47,10}, {48,14}, {49,11},
	{50,12}, {51,13}, {52,13}, {53,13}, {54,5}, {55,13}, {56,9}, {57,8}, {58,12}, {59,9}
};
static const HedgeMazeRoom::Cell kPath0[] = { {0,1}, {1,5}, {2,2}, {10,4}, {13,5}, {14,2}, {24,3} };
static const HedgeMazeRoom::Cell kPath1[] = {
	{2,1}, {3,5}, {4,2}, {10,2}, {11,1}, {12,4}, {15,2}, {20,6}, {21,6}, {25,3}, {26,5}, {30,3}, {31,4}
};
static const HedgeMazeRoom::Cell kPath2[] = {
	{5,1}, {7,2}, {10,5}, {11,2}, {12,1}, {13,2}, {14,1}, {15,4}, {17,3}, {18,2}, {20,1}, {21,4},
	{28,3}, {29,5}, {30,3}, {32,4}, {33,3}
};

void HedgeMazeRoom::levelMask(int level, const Cell *&cells, uint &count) {
	switch (level) {
	case 0: cells = kMask0; count = ARRAYSIZE(kMask0); break;
	case 1: cells = kMask1; count = ARRAYSIZE(kMask1); break;
	default: cells = kMask2; count = ARRAYSIZE(kMask2); break;
	}
}

void HedgeMazeRoom::levelPath(int level, const Cell *&cells, uint &count) {
	switch (level) {
	case 0: cells = kPath0; count = ARRAYSIZE(kPath0); break;
	case 1: cells = kPath1; count = ARRAYSIZE(kPath1); break;
	default: cells = kPath2; count = ARRAYSIZE(kPath2); break;
	}
}

// The cell enter forces to mask 0xb when the center path is taken (+0x440).
int HedgeMazeRoom::patchCell(int level) {
	return level == 0 ? 24 : level == 1 ? 26 : 32;
}

HedgeMazeRoom::Start HedgeMazeRoom::computeStart(const Common::String &from, int level, bool centerFlag) {
	// enter's ELSE branch: the cell the gang starts in by which maze location it
	// came from. From the garden it rides up (dir 1); below the balcony, down
	// (dir 4); from the center, two cases by game+0x289, always on the center path.
	if (from == "ROOM_P06_Garden") {
		const int y[] = { 4, 6, 9 };
		return Start{ 2, y[level], kUp, 0 };
	}
	if (from == "ROOM_P27_Below_Balcony")
		return Start{ 1, 0, kDown, 0 };
	if (from == "ROOM_P23_Maze_Center") {
		if (!centerFlag) {
			static const Start s[] = { { 0, 2, kLeft, 1 }, { 0, 4, kLeft, 1 }, { 3, 3, kUp, 1 } };
			return s[level];
		}
		static const Start s[] = { { 1, 3, kDown, 1 }, { 1, 5, kDown, 1 }, { 2, 4, kLeft, 1 } };
		return s[level];
	}
	// Not an outside entry: the position the room keeps (init 2, 4).
	return Start{ 2, 4, kUp, 0 };
}

void HedgeMazeRoom::gardenExits(int level, const int *&cells, uint &count, Start &reposition) {
	static const int l1[] = { 44 };
	static const int l2[] = { 41, 40, 58 };
	switch (level) {
	case 0: cells = nullptr; count = 0; reposition = Start{ 2, 4, kUp, 0 }; break;
	case 1: cells = l1; count = ARRAYSIZE(l1); reposition = Start{ 2, 6, kUp, 0 }; break;
	default: cells = l2; count = ARRAYSIZE(l2); reposition = Start{ 2, 9, kUp, 0 }; break;
	}
}

void HedgeMazeRoom::loadLevel(int level, bool centerPath) {
	for (uint i = 0; i < kGrid; i++)
		_grid[i] = _path[i] = -1;
	const Cell *m;
	uint n;
	levelMask(level, m, n);
	for (uint i = 0; i < n; i++)
		_grid[m[i].cell] = m[i].value;
	levelPath(level, m, n);
	for (uint i = 0; i < n; i++)
		_path[m[i].cell] = m[i].value;
	if (centerPath)
		_grid[patchCell(level)] = 0xb; // enter's +0x440 patch
}

void HedgeMazeRoom::playClip(const Common::String &name) {
	_engine.getSound()->play(name);
}

void HedgeMazeRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42BB00: the four directional buttons and the six path indicators are
	// clickables the room drives; everything else falls to the base path.
	for (int i = 0; i < 4; i++) {
		if (object == kMoves[i].object) {
			claim.build = Room::Claim::kClickable;
			return;
		}
	}
	static const char *const kPaths[] = {
		"OBJ_HEDGE_MAZE_PATH_EW", "OBJ_HEDGE_MAZE_PATH_NS", "OBJ_HEDGE_MAZE_PATH_NE",
		"OBJ_HEDGE_MAZE_PATH_NW", "OBJ_HEDGE_MAZE_PATH_SE", "OBJ_HEDGE_MAZE_PATH_SW"
	};
	for (int i = 0; i < 6; i++)
		if (object == kPaths[i])
			claim.build = Room::Claim::kClickable;
}

void HedgeMazeRoom::drawCell() {
	// enter's per-cell draw: each button shows a passage (EXIT) where the cell's
	// mask bit is set, a wall (OVERLAY) where it is not.
	Room &room = *_engine.getRoom();
	const int mask = (_x * kCols + _y >= 0 && _x * kCols + _y < kGrid) ? _grid[_x * kCols + _y] : -1;
	for (int i = 0; i < 4; i++) {
		if (_button[i] < 0)
			continue;
		const bool open = mask >= 0 && (mask & kMoves[i].code);
		room.setSequence(_button[i], kMoves[i].object, open ? kMoves[i].exitAnim : kMoves[i].overlay, kOnce, 0);
	}
}

void HedgeMazeRoom::enter(const Common::String &from) {
	// 0x42AEB0, after the base entry. The original's enter also routes the maze
	// map item (active in the toolbar) to ROOM_Maze_Map; here the navigation
	// setup: the start cell by entry source, the level table, the cell's draw.
	Room &room = *_engine.getRoom();
	const int level = _engine.getDifficulty();

	if (from == "ROOM_P06_Garden" || from == "ROOM_P23_Maze_Center" || from == "ROOM_P27_Below_Balcony") {
		// From the centre the start cell depends on how the centre was reached
		// (MazeCenterFromGarden, the centre's game+0x289); it is ignored for the
		// other sources.
		const bool centerFlag = from == "ROOM_P23_Maze_Center" && _engine.getGameFlag("MazeCenterFromGarden");
		const Start s = computeStart(from, level, centerFlag);
		_x = s.x;
		_y = s.y;
		_dir = s.dir;
		_centerPath = s.patch != 0;
	}
	loadLevel(level, _centerPath);

	// Locate the four directional buttons among the placements, then draw the
	// current cell's passages and walls.
	for (int i = 0; i < 4; i++)
		_button[i] = -1;
	for (uint sp = 0; sp < room.getSpriteCount(); sp++) {
		if (room.isEngineSprite(sp))
			continue;
		const Common::String &obj = room.getObjectName(sp);
		for (int i = 0; i < 4; i++)
			if (obj == kMoves[i].object)
				_button[i] = sp;
	}
	drawCell();
	const int cell = _x * kCols + _y;
	debugC(1, kDebugScript, "hedge maze: enter from '%s' level %d cell (%d,%d)=%d mask %d; buttons %d %d %d %d",
	       from.c_str(), level, _x, _y, cell, cell >= 0 && cell < kGrid ? _grid[cell] : -1,
	       _button[0], _button[1], _button[2], _button[3]);
}

void HedgeMazeRoom::roomLeft() {
	// 0x42BAE0.
	for (int i = 0; i < 4; i++)
		_button[i] = -1;
}

void HedgeMazeRoom::tick() {
	// 0x42AD00: the unicycle's rare idle mutterings.
	for (int i = 0; i < 3; i++)
		if (_engine.getRandomNumber(2999) == 0)
			_engine.getSound()->play(kIdle[i]);
}

bool HedgeMazeRoom::click(int sprite) {
	// A directional button: ride to the adjacent cell if it is a passage. Moving
	// into a garden-exit cell leaves the maze to the garden.
	for (int i = 0; i < 4; i++) {
		if (sprite != _button[i])
			continue;
		const int cell = _x * kCols + _y;
		const int mask = (cell >= 0 && cell < kGrid) ? _grid[cell] : -1;
		if (mask < 0 || !(mask & kMoves[i].code))
			return true; // a wall: no move
		_x += kMoves[i].dx;
		_y += kMoves[i].dy;
		_dir = kMoves[i].code;
		playClip(kMoves[i].startClip);

		const int level = _engine.getDifficulty();
		const int *exits;
		uint n;
		Start repo;
		gardenExits(level, exits, n, repo);
		const int here = _x * kCols + _y;
		for (uint e = 0; e < n; e++) {
			if (here == exits[e]) {
				_x = repo.x;
				_y = repo.y;
				_dir = repo.dir;
				debugC(1, kDebugScript, "hedge maze: %s into garden-exit cell %d, to ROOM_P06_Garden", kMoves[i].object, here);
				_engine.requestRoom("ROOM_P06_Garden");
				return true;
			}
		}
		playClip(kMoves[i].endClip);
		drawCell();
		debugC(1, kDebugScript, "hedge maze: %s to cell (%d,%d)=%d mask %d", kMoves[i].object, _x, _y, here,
		       here >= 0 && here < kGrid ? _grid[here] : -1);
		return true;
	}
	return false;
}

const MazeMapRoom::Marker MazeMapRoom::kMarkers[4] = {
	{ "OBJ_MAZEMAP_GARDEN", "ROOM_P06_Garden" },
	{ "OBJ_MAZEMAP_WELL", "ROOM_P22_Well_In_Maze" },
	{ "OBJ_MAZEMAP_CENTER", "ROOM_P23_Maze_Center" },
	{ "OBJ_MAZEMAP_BALCONY", "ROOM_P27_Below_Balcony" },
};
const char *MazeMapRoom::kMusic = "1";

void MazeMapRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x42A850: each of the four location markers is a clickable (handler
	// 0x42A9A0) that jumps straight to its maze room.
	for (int i = 0; i < 4; i++)
		if (object == kMarkers[i].object)
			claim.build = Room::Claim::kClickable;
}

bool MazeMapRoom::click(int sprite) {
	Room &room = *_engine.getRoom();
	const Common::String &obj = room.getObjectName(sprite);
	for (int i = 0; i < 4; i++) {
		if (obj == kMarkers[i].object) {
			debugC(1, kDebugScript, "maze map: %s clicked, to %s", kMarkers[i].object, kMarkers[i].room);
			_engine.requestRoom(kMarkers[i].room);
			return true;
		}
	}
	return false;
}

RoomClass *createMazeRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == "ROOM_Hedge_Maze")
		return new HedgeMazeRoom(engine);
	if (room == "ROOM_Maze_Map")
		return new MazeMapRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
