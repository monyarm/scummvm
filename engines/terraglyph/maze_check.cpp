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

// The script-level check of Phantom's hedge maze and maze map (WP-074),
// against an independent reference written from the trace.

#include "common/debug.h"

#include "terraglyph/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/maze_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkMaze() {
	const char *const kHedge = "ROOM_Hedge_Maze";
	const char *const kMap = "ROOM_Maze_Map";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kHedge;
	if (!found) {
		debugC(3, kDebugScript, "maze check: no hedge maze");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	auto story = [&](int level) {
		newGameForCheck();
		setDifficulty(level);
		_sound->stop();
	};
	auto has = [&](const char *name) {
		return _sound->findClip(name) != nullptr ? 1 : 0;
	};

	// The classes.
	{
		RoomClass *a = createRoomClass(*this, kHedge);
		RoomClass *b = createRoomClass(*this, kMap);
		debugC(3, kDebugScript, "maze check classes: Hedge_Maze %s, Maze_Map %s",
		       dynamic_cast<HedgeMazeRoom *>(a) ? "hedge maze" : "-",
		       dynamic_cast<MazeMapRoom *>(b) ? "maze map" : "-");
		delete a;
		delete b;
	}

	// The music, the nine unicycle clips and the three idle lines.
	{
		Common::String out = Common::String::format("music %s found %d; unicycle", HedgeMazeRoom::kMusic, has(HedgeMazeRoom::kMusic));
		static const char *const kTag[9] = { "idle", "startL", "startR", "startU", "startD", "endU", "endD", "endL", "endR" };
		for (int i = 0; i < 9; i++)
			out += Common::String::format(" %s %d", kTag[i], has(HedgeMazeRoom::kUnicycle[i]));
		out += "; idle";
		for (int i = 0; i < 3; i++)
			out += Common::String::format(" %s %d", HedgeMazeRoom::kIdle[i], _sound->hasClipPrefix(HedgeMazeRoom::kIdle[i]) ? 1 : 0);
		out += Common::String::format("; map music %s found %d", MazeMapRoom::kMusic, has(MazeMapRoom::kMusic));
		debugC(3, kDebugScript, "maze check clips: %s", out.c_str());
	}

	// The four directional moves: the dir code, the step, the clips, the anims.
	{
		static const char *const kName[4] = { "back", "front", "r", "l" };
		Common::String out;
		for (int i = 0; i < 4; i++) {
			const HedgeMazeRoom::Move &m = HedgeMazeRoom::kMoves[i];
			out += Common::String::format("%s%s dir %d d %d %d start %s end %s anim %s/%s",
			                              i ? "; " : "", kName[i], m.code, m.dx, m.dy, m.startClip, m.endClip, m.overlay, m.exitAnim);
		}
		debugC(3, kDebugScript, "maze check moves: %s", out.c_str());
	}

	// The per-level maze table, loaded by enter into the grid.
	for (int l = 0; l < 3; l++) {
		story(l);
		showRoom(kHedge);
		HedgeMazeRoom *maze = dynamic_cast<HedgeMazeRoom *>(getRoomClass(kHedge));
		Common::String out;
		int cells = 0;
		for (uint i = 0; i < HedgeMazeRoom::kGrid; i++) {
			if (maze->getGridCell(i) >= 0) {
				out += Common::String::format("%s%u:%d", cells ? " " : "", i, maze->getGridCell(i));
				cells++;
			}
		}
		Common::String paths;
		int pc = 0;
		for (uint i = 0; i < HedgeMazeRoom::kGrid; i++) {
			if (maze->getPathCell(i) >= 0) {
				paths += Common::String::format("%s%u:%d", pc ? " " : "", i, maze->getPathCell(i));
				pc++;
			}
		}
		debugC(3, kDebugScript, "maze check level %d: %d cells %s; path %s; patch cell %d",
		       l, cells, out.c_str(), paths.c_str(), HedgeMazeRoom::patchCell(l));
	}

	// The start cells by entry source, and the garden-exit cells.
	for (int l = 0; l < 3; l++) {
		HedgeMazeRoom::Start g = HedgeMazeRoom::computeStart("ROOM_P06_Garden", l, false);
		HedgeMazeRoom::Start b = HedgeMazeRoom::computeStart("ROOM_P27_Below_Balcony", l, false);
		HedgeMazeRoom::Start c0 = HedgeMazeRoom::computeStart("ROOM_P23_Maze_Center", l, false);
		HedgeMazeRoom::Start c1 = HedgeMazeRoom::computeStart("ROOM_P23_Maze_Center", l, true);
		const int *exits;
		uint n;
		HedgeMazeRoom::Start repo;
		HedgeMazeRoom::gardenExits(l, exits, n, repo);
		Common::String ex;
		for (uint e = 0; e < n; e++)
			ex += Common::String::format("%s%d", e ? "," : "", exits[e]);
		debugC(3, kDebugScript,
		       "maze check starts %d: garden %d %d dir %d; balcony %d %d dir %d; center0 %d %d dir %d patch %d; center1 %d %d dir %d patch %d; garden-exits %s -> %d %d dir %d",
		       l, g.x, g.y, g.dir, b.x, b.y, b.dir, c0.x, c0.y, c0.dir, c0.patch, c1.x, c1.y, c1.dir, c1.patch,
		       n ? ex.c_str() : "none", repo.x, repo.y, repo.dir);
	}

	// The maze map's markers and their destination rooms.
	{
		Common::String out;
		for (int i = 0; i < 4; i++)
			out += Common::String::format("%s%s->%s", i ? ", " : "", MazeMapRoom::kMarkers[i].object, MazeMapRoom::kMarkers[i].room);
		debugC(3, kDebugScript, "maze check map: %s", out.c_str());
	}

	_checking = false;
}

} // End of namespace Terraglyph
