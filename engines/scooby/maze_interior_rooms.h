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

#ifndef SCOOBY_MAZE_INTERIOR_ROOMS_H
#define SCOOBY_MAZE_INTERIOR_ROOMS_H

#include "common/str.h"

#include "scooby/room.h"
#include "scooby/room_class.h"

namespace Scooby {

/**
 * Phantom's maze interior (WP-075), the three rooms reached from the hedge
 * maze (WP-074): the centre, the fountain close-up off it, and the well.
 * The addresses are Phantom's; Showdown has none of these rooms.
 */

/**
 * ROOM_P23_Maze_Center (class 0x42A090, vtable 0x45F344): the hub at the heart
 * of the maze. A water sprite babbles (SFX_P23_MAZE_CENTER_WATER at volume 90);
 * the way to the fountain close-up (OBJ_P23_TO_P44) is open until the tokens are
 * grabbed, then it closes; the rusty shears lie here to take, and two hotspots
 * cycle their lines. Entering from the garden or from below the balcony sets the
 * flag the hedge maze reads to pick its start cell (game+0x289), and the room
 * saves its entrance list (the "Entrances" key). Music "1", no opening.
 */
class MazeCenterRoom : public RoomClass {
public:
	explicit MazeCenterRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                 // 0x42A580
	void roomLeft() override;                                                         // 0x42A5F0
	void tick() override;                                                             // 0x42A410
	void event(const Common::String &object, bool success) override;                 // 0x42A110
	void reset() override;                                                            // 0x42A0F0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;   // 0x42A280
	void saveKeys(Common::INIFile &profile) override;                                 // 0x42A200
	void loadKeys(const Common::INIFile &profile) override;                           // 0x42A240

	static const char *kWaterSfx;       // SFX_P23_MAZE_CENTER_WATER (no dot)
	enum { kWaterVolume = 90 };         // 0x5a
	static const char *kTakeShears;     // Maze_Center.Shaggy_Takes_Shears
	static const char *kShearsScene;    // Center_of_Maze.250B_
	static const char *kHotspot4Lines[2]; // Center_of_Maze.250X_, .250X1_
	static const char *kHotspotLines[2];  // Center_of_Maze.250_, .250C_

	// Entering from one of these sets the hedge-maze start flag (game+0x289,
	// named MazeCenterFromGarden): the garden sets it, below the balcony clears
	// it; -1 for any other source, which leaves it alone.
	static int entryFlagFor(const Common::String &from);

	// For the check: whether the Entrances key is present (the room's own state).
	bool hasEntrances() const { return _entrances; }

private:
	int findSprite(const Common::String &object) const;

	bool _takingShears = false; // +0xc0: the rusty shears are being taken
	bool _tookShears = false;   // +0xc1: Shaggy has reached them
	int _hotspot4 = 0;          // +0xb8: the HOTSPOT4 line cycle
	bool _entrances = false;    // the Entrances save key (the room has been set up)
	int _water = -1;
};

/**
 * ROOM_P44_Fountain_Closeup (class 0x429740, vtable 0x45F304): the close-up off
 * the centre, where a piranha guards the tokens. Fed the burger it lunges, eats,
 * and Shaggy grabs the tokens into the toolbar; the way back to the centre
 * (OBJ_P44_TO_P23) then opens and the exit list is built. A plain click on the
 * fountain cycles three lines. Music "1", opening Fountain.250D_.
 */
class FountainCloseupRoom : public RoomClass {
public:
	explicit FountainCloseupRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                 // 0x4297D0
	void roomLeft() override;                                                         // 0x42A040
	void tick() override;                                                             // 0x429800
	void event(const Common::String &object, bool success) override;                 // 0x429B50
	void reset() override;                                                            // 0x4297A0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;   // 0x429CC0

	static const char *kOpening;        // Fountain.250D_ (also the first click line)
	static const char *kClickLines[3];  // Fountain.250D_, .250D1_, .250D2_
	static const char *kExitLine;       // Fountain.250F_ (the return exit's scene)
	static const char *kWater;          // FOUNTAIN_CLOSEUP.WATER
	static const char *kSwimLoop;       // Fountain_Closeup.Fountain_Closeup.Piranha_Swims (the exe's doubled prefix)
	static const char *kGrabTokens;     // Fountain_Closeup.Shaggy_Takes_Tokens
	static const char *kLunge;          // Fountain_Closeup.Piranha_Lunge_Attack
	static const char *kEats;           // Fountain_Closeup.Piranha_Eats
	static const char *kPiranhaIdle;    // ANIM_PIRANHAIDLE
	static const char *kPiranhaFull;    // ANIM_PIRANHAFULL (when the burger is held)
	static const char *kEatStart;       // ANIM_PIRANHALUNGE_EAT_START

private:
	int findSprite(const Common::String &object) const;

	bool _fed = false;    // +0xca: the burger was fed, the token grab is pending
	bool _grabbed = false;// +0xc9: the tokens have been granted
	int _clickCycle = 0;  // +0xc4: the fountain-click line cycle
	int _piranha = -1;    // +0xb8
};

/**
 * ROOM_P22_Well_In_Maze (class 0x42C740, vtable 0x45F404): the well in the maze,
 * which links back to the royal gallery. The yellow ball (the Jester's) found
 * here plays movie 66 the first time, 4 after, and is marked used; the rope lets
 * the gang climb (OBJ_P22_WELL / OBJ_P22_WELL_ROPE / OBJ_P22_TO_P21, gated by the
 * used rope); a plain click on the well cycles three lines. Entering sets
 * WellVisited; a pending return bounces to the gallery through movie 54 (from the
 * gallery) or 57, unless the full bottle has been used. Music "1", no opening.
 */
class WellInMazeRoom : public RoomClass {
public:
	explicit WellInMazeRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                 // 0x42C810
	void tick() override;                                                            // 0x42C7F0
	void event(const Common::String &object, bool success) override;                 // 0x42C8E0
	void reset() override;                                                           // 0x42C7D0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x42CA20

	static const char *kClickLines[3];  // Well_in_Maze.290_, Well_In_Maze.330X1_, Well_In_Maze.330X2_
	enum {
		kMovieBallFirst = 66, // 0x42: the yellow ball, first time (game+0x25c clear)
		kMovieBallAgain = 4,  // the yellow ball, after
		kMovieWellRope = 59,  // 0x3b: the rope used on the well
		kMovieReturnGallery = 54, // 0x36: the pending return, came from the gallery
		kMovieReturnOther = 57    // 0x39: the pending return, from elsewhere
	};
	static const char *kGallery; // ROOM_P07_Royal_Gallery

private:
	int findSprite(const Common::String &object) const;

	int _clickCycle = 0;   // +0xcc: the well-click line cycle
	bool _returnPending = false; // +0xd0: a return to the gallery is queued
};

/** The maze interior's room class for @p room, or nullptr. */
RoomClass *createMazeInteriorRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_MAZE_INTERIOR_ROOMS_H
