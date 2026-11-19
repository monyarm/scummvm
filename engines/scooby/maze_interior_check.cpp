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

// The script-level check of Phantom's maze interior (WP-075) — the centre, the
// fountain close-up and the well — against an independent reference written
// from the trace. Showdown has none of these rooms.

#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/maze_interior_rooms.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *buildName(Room::Claim::Build b) {
	switch (b) {
	case Room::Claim::kDefault: return "default";
	case Room::Claim::kSkip: return "skip";
	case Room::Claim::kRequires: return "requires";
	case Room::Claim::kClickable: return "clickable";
	case Room::Claim::kSprite: return "sprite";
	case Room::Claim::kInventory: return "inventory";
	case Room::Claim::kExit: return "exit";
	}
	return "?";
}

void ScoobyEngine::checkMazeInterior() {
	const char *const kMazeCenter = "ROOM_P23_Maze_Center";
	const char *const kFountain = "ROOM_P44_Fountain_Closeup";
	const char *const kWell = "ROOM_P22_Well_In_Maze";

	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kMazeCenter;
	if (!found) {
		debugC(3, kDebugScript, "maze interior check: no maze interior");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	// sfx and effect clips ship under their exact name; the numbered scene lines
	// ship with a speaker suffix (Center_of_Maze.250B_1_VELMA), so they match by
	// prefix — reading exactly would say "found 0" of a name that is right (the
	// WP-074 trap), so the voice lines are probed by prefix.
	auto has = [&](const char *name) { return _sound->findClip(name) != nullptr ? 1 : 0; };
	auto pfx = [&](const char *name) { return _sound->hasClipPrefix(name) ? 1 : 0; };

	// The classes the factory builds.
	{
		RoomClass *a = createRoomClass(*this, kMazeCenter);
		RoomClass *b = createRoomClass(*this, kFountain);
		RoomClass *c = createRoomClass(*this, kWell);
		debugC(3, kDebugScript, "maze interior check classes: Maze_Center %s, Fountain %s, Well %s",
		       dynamic_cast<MazeCenterRoom *>(a) ? "centre" : a ? "other" : "-",
		       dynamic_cast<FountainCloseupRoom *>(b) ? "fountain" : b ? "other" : "-",
		       dynamic_cast<WellInMazeRoom *>(c) ? "well" : c ? "other" : "-");
		delete a;
		delete b;
		delete c;
	}

	// A claim measured after setting the engine's inventory/flags.
	auto claimOf = [&](RoomClass *rc, const char *object) {
		Room::Claim claim;
		rc->claimPlacement(object, claim);
		return claim;
	};

	// -----------------------------------------------------------------------
	// The maze centre.
	{
		RoomClass *rc = createRoomClass(*this, kMazeCenter);
		debugC(3, kDebugScript, "maze interior check Maze_Center clips: water %s vol %d found %d; shears %s %d, scene %s %d; hotspot4 %s %d %s %d; hotspot %s %d %s %d",
		       MazeCenterRoom::kWaterSfx, (int)MazeCenterRoom::kWaterVolume, has(MazeCenterRoom::kWaterSfx),
		       MazeCenterRoom::kTakeShears, has(MazeCenterRoom::kTakeShears), MazeCenterRoom::kShearsScene, pfx(MazeCenterRoom::kShearsScene),
		       MazeCenterRoom::kHotspot4Lines[0], pfx(MazeCenterRoom::kHotspot4Lines[0]), MazeCenterRoom::kHotspot4Lines[1], pfx(MazeCenterRoom::kHotspot4Lines[1]),
		       MazeCenterRoom::kHotspotLines[0], pfx(MazeCenterRoom::kHotspotLines[0]), MazeCenterRoom::kHotspotLines[1], pfx(MazeCenterRoom::kHotspotLines[1]));

		newGameForCheck();
		const Room::Claim water = claimOf(rc, "OBJ_P23_MAZE_CENTER_WATER");
		const Room::Claim shearsLoose = claimOf(rc, "OBJ_INV_RUSTY_SHEARS");
		const Room::Claim p44None = claimOf(rc, "OBJ_P23_TO_P44");
		_inventory->addItem("OBJ_INV_RUSTY_SHEARS", "ANIM_TOOLBARRUSTY_SHEARS", "ANIM_CURSORRUSTY_SHEARS");
		const Room::Claim shearsHeld = claimOf(rc, "OBJ_INV_RUSTY_SHEARS");
		_inventory->addItem("OBJ_INV_TOKENS", "ANIM_TOOLBARTOKENS", "ANIM_CURSORTOKENS");
		const Room::Claim p44Tokens = claimOf(rc, "OBJ_P23_TO_P44");
		debugC(3, kDebugScript, "maze interior check Maze_Center claims: WATER %s d%d; RUSTY_SHEARS loose %s held %s; TO_P44 none %s tokens %s",
		       buildName(water.build), water.depth, buildName(shearsLoose.build), buildName(shearsHeld.build),
		       buildName(p44None.build), buildName(p44Tokens.build));

		// The hedge-maze start flag by entry source, and the Entrances save key.
		MazeCenterRoom *mc = dynamic_cast<MazeCenterRoom *>(rc);
		Common::INIFile yes;
		yes.setKey("Entrances", kMazeCenter, "Yes");
		mc->loadKeys(yes);
		const bool read = mc->hasEntrances();
		mc->reset();
		const bool afterReset = mc->hasEntrances();
		Common::INIFile none;
		mc->loadKeys(none);
		debugC(3, kDebugScript, "maze interior check Maze_Center state: garden %d, balcony %d, maze %d; Entrances read %d, reset %d, missing %d",
		       MazeCenterRoom::entryFlagFor("ROOM_P06_Garden"), MazeCenterRoom::entryFlagFor("ROOM_P27_Below_Balcony"),
		       MazeCenterRoom::entryFlagFor("ROOM_Hedge_Maze"), read ? 1 : 0, afterReset ? 1 : 0, mc->hasEntrances() ? 1 : 0);
		delete rc;
	}

	// -----------------------------------------------------------------------
	// The fountain close-up.
	{
		RoomClass *rc = createRoomClass(*this, kFountain);
		debugC(3, kDebugScript, "maze interior check Fountain clips: opening %s %d; cycle %s %d %s %d %s %d; exit %s %d; water %s %d; swim %s %d; grab %s %d; lunge %s %d; eats %s %d",
		       FountainCloseupRoom::kOpening, pfx(FountainCloseupRoom::kOpening),
		       FountainCloseupRoom::kClickLines[0], pfx(FountainCloseupRoom::kClickLines[0]), FountainCloseupRoom::kClickLines[1], pfx(FountainCloseupRoom::kClickLines[1]),
		       FountainCloseupRoom::kClickLines[2], pfx(FountainCloseupRoom::kClickLines[2]), FountainCloseupRoom::kExitLine, pfx(FountainCloseupRoom::kExitLine),
		       FountainCloseupRoom::kWater, has(FountainCloseupRoom::kWater), FountainCloseupRoom::kSwimLoop, has(FountainCloseupRoom::kSwimLoop),
		       FountainCloseupRoom::kGrabTokens, has(FountainCloseupRoom::kGrabTokens), FountainCloseupRoom::kLunge, has(FountainCloseupRoom::kLunge),
		       FountainCloseupRoom::kEats, has(FountainCloseupRoom::kEats));

		newGameForCheck();
		const Room::Claim piranhaIdle = claimOf(rc, "OBJ_PIRANHA");
		const Room::Claim tokensLoose = claimOf(rc, "OBJ_INV_TOKENS");
		const Room::Claim toP23 = claimOf(rc, "OBJ_P44_TO_P23");
		const Room::Claim grab = claimOf(rc, "OBJ_SHAGGY_P44_GRAB_TOKENS");
		_inventory->addItem("OBJ_INV_BURGER", "ANIM_TOOLBARBURGER", "ANIM_CURSORBURGER");
		_inventory->removeItem("OBJ_INV_BURGER"); // used: in the history, out of the slots
		const Room::Claim piranhaFull = claimOf(rc, "OBJ_PIRANHA");
		_inventory->addItem("OBJ_INV_TOKENS", "ANIM_TOOLBARTOKENS", "ANIM_CURSORTOKENS");
		const Room::Claim tokensHeld = claimOf(rc, "OBJ_INV_TOKENS");
		debugC(3, kDebugScript, "maze interior check Fountain claims: PIRANHA idle %s d%d anim %s, full anim %s; TOKENS loose %s d%d cursor %s held %s; TO_P23 %s d%d; GRAB %s d%d hidden %d",
		       buildName(piranhaIdle.build), piranhaIdle.depth, piranhaIdle.anim.c_str(), piranhaFull.anim.c_str(),
		       buildName(tokensLoose.build), tokensLoose.depth, tokensLoose.required.c_str(), buildName(tokensHeld.build),
		       buildName(toP23.build), toP23.depth, buildName(grab.build), grab.depth, grab.hidden ? 1 : 0);
		delete rc;
	}

	// -----------------------------------------------------------------------
	// The well.
	{
		RoomClass *rc = createRoomClass(*this, kWell);
		debugC(3, kDebugScript, "maze interior check Well clips: cycle %s %d %s %d %s %d",
		       WellInMazeRoom::kClickLines[0], pfx(WellInMazeRoom::kClickLines[0]), WellInMazeRoom::kClickLines[1], pfx(WellInMazeRoom::kClickLines[1]),
		       WellInMazeRoom::kClickLines[2], pfx(WellInMazeRoom::kClickLines[2]));

		newGameForCheck();
		const Room::Claim ballFresh = claimOf(rc, "OBJ_INV_BALL_YELLOW");
		const Room::Claim p21Dry = claimOf(rc, "OBJ_P22_TO_P21");
		const Room::Claim ropeDry = claimOf(rc, "OBJ_P22_WELL_ROPE");
		const Room::Claim wellDry = claimOf(rc, "OBJ_P22_WELL");
		setGameFlag("YellowBallTaken", true);
		const Room::Claim ballUsed = claimOf(rc, "OBJ_INV_BALL_YELLOW");
		_inventory->addItem("OBJ_INV_ROPE", "ANIM_TOOLBARROPE", "ANIM_CURSORROPE");
		_inventory->removeItem("OBJ_INV_ROPE"); // used
		const Room::Claim p21Wet = claimOf(rc, "OBJ_P22_TO_P21");
		const Room::Claim ropeWet = claimOf(rc, "OBJ_P22_WELL_ROPE");
		const Room::Claim wellWet = claimOf(rc, "OBJ_P22_WELL");
		debugC(3, kDebugScript, "maze interior check Well claims: BALL_YELLOW fresh %s used %s; TO_P21 dry %s rope-used %s; WELL_ROPE dry %s rope-used %s d%d; WELL dry %s rope-used %s",
		       buildName(ballFresh.build), buildName(ballUsed.build), buildName(p21Dry.build), buildName(p21Wet.build),
		       buildName(ropeDry.build), buildName(ropeWet.build), ropeWet.depth, buildName(wellDry.build), buildName(wellWet.build));
		debugC(3, kDebugScript, "maze interior check Well movies: ball first %d again %d; well+rope %d; return gallery %d other %d; links %s",
		       (int)WellInMazeRoom::kMovieBallFirst, (int)WellInMazeRoom::kMovieBallAgain, (int)WellInMazeRoom::kMovieWellRope,
		       (int)WellInMazeRoom::kMovieReturnGallery, (int)WellInMazeRoom::kMovieReturnOther, WellInMazeRoom::kGallery);
		delete rc;
	}

	_checking = false;
}

} // End of namespace Scooby
