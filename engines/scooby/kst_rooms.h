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

#ifndef SCOOBY_KST_ROOMS_H
#define SCOOBY_KST_ROOMS_H

#include "common/array.h"

#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"

namespace Scooby {

/**
 * Phantom's Knights of the Snack Table, ROOM_P31_KST (0x419700, vtable
 * 0x45EDEC): the timed set piece Phantom gives in place of Showdown's Pie
 * Noon (its asserts still name Pie Noon). Knights walk up to a snack table
 * and sit; Shaggy fills mugs and serves them; the gang must serve the level's
 * quota of knights (five less the difficulty) before the way back to the
 * dining room opens. A counter shows the tally.
 *
 * The room is built from marker placements (claimPlacement 0x41A970): the
 * knights' seats, the benches' start/end/floor rows, the drinks' spawn
 * points, three table overlays, two counter digits and Shaggy's post. The
 * knights (0x418CA0), Shaggy (0x4192F0) and the drinks each run in the tree
 * walk; the room's tick (0x419E40) drives the serving loop and, once the
 * quota is met, exits to ROOM_P12_Dining_Room.
 */
class KstRoom : public RoomClass, public Room::Actor {
public:
	explicit KstRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x419A80
	void roomLeft() override;                                                        // 0x419CC0
	void tick() override;                                                            // 0x419E40
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x41A970
	void actorUpdate(uint sprite) override;
	// Input is held while the narrator's intro line plays (+0x1A8).
	bool isBusy() const override { return _narrating; }

	enum { kKnightSlots = 6, kMarkers = 3 };

	// The marker arrays (0x41A970), each sorted ascending by y (the primary
	// array keeps the y, the parallel one the x); the knights' x is the
	// placement x less 40, the gang entering from off the left. Bench-floor
	// keeps only its y.
	uint getKnightCount() const { return _knightCount; }
	float getKnightX(uint i) const { return _knightX[i]; }
	float getKnightY(uint i) const { return _knightY[i]; }
	float getBenchEndX(uint i) const { return _benchEndX[i]; }
	float getBenchEndY(uint i) const { return _benchEndY[i]; }
	float getBenchFloorY(uint i) const { return _benchFloorY[i]; }
	float getDrinkX(uint i) const { return _drinkX[i]; }
	float getDrinkY(uint i) const { return _drinkY[i]; }
	float getBenchStartX(uint i) const { return _benchStartX[i]; }
	float getBenchStartY(uint i) const { return _benchStartY[i]; }
	float getShaggyX() const { return _shaggyX; }
	float getShaggyY() const { return _shaggyY; }

	// The sprites the claim built.
	int getOverlay(uint i) const { return i < 3 ? _overlays[i] : -1; }
	int getTens() const { return _tens; }
	int getOnes() const { return _ones; }

	// The level's quota and the exit.
	int getToServe() const { return _toServe; }
	const Common::String &getNarrator() const { return _narrator; }

	/** 0x4192F0 / 0x418CA0: a knight walking to its seat and drinking. */
	struct Knight {
		int sprite = -1;      // the body
		int arm = -1;         // the arm (+0x80)
		uint slot = 0;        // +0x68: its seat (0..2, slot % 3)
		int state = 4;        // +0x60: 0 drinks, 1 walks, 2 starts, 3 finishes, 4 parked
		float x = 0, y = 0;   // +0x0C, +0x10
		float target = 0;     // +0x64: the seat's reach
	};

private:
	void startRound();                 // 0x41A5C0
	void serve();                      // Shaggy drops a mug (0x419680)
	void showCounter();
	void placeSprite(int sprite, float x, float y);
	/** The sorted insert of 0x41A970: @p primary kept ascending, @p parallel following. */
	void insertMarker(float *primary, float *parallel, uint &count, float key, float val);
	int layer0() const;
	void playSound(const Common::String &name);

	// The marker arrays (claimPlacement): each primary holds its sorted y, the
	// parallel its x.
	float _knightY[kMarkers] = {}, _knightX[kMarkers] = {};        // +0x114, +0x108 (x less 40)
	uint _knightCount = 0;                                         // +0x18C
	float _benchEndY[kMarkers] = {}, _benchEndX[kMarkers] = {};    // +0x144, +0x138
	uint _benchEndCount = 0;                                       // +0x184
	float _benchFloorY[kMarkers] = {};                             // +0x150 (y only)
	uint _benchFloorCount = 0;                                     // +0x188
	float _drinkY[kMarkers] = {}, _drinkX[kMarkers] = {};          // +0x168, +0x15C
	uint _drinkCount = 0;                                          // +0x190
	float _benchStartY[kMarkers] = {}, _benchStartX[kMarkers] = {}; // +0x12C, +0x120
	uint _benchStartCount = 0;                                     // +0x180
	float _shaggyX = 0, _shaggyY = 0;                              // Shaggy's post
	bool _hasShaggy = false;

	// The sprites the claim built.
	int _overlays[3] = { -1, -1, -1 };  // +0x174, +0x178, +0x17C
	int _tens = -1, _ones = -1;         // +0xB8, +0xBC: the counter digits, tens = smaller x
	uint _numbers = 0;

	// The round.
	int _shaggy = -1;                   // +0xC4: Shaggy's sprite
	Knight _knights[kKnightSlots];      // +0xF0..+0x104
	int _toServe = 0;                   // +0xE8: knights still to serve this level
	int _round = 1;                     // +0x198
	int _spawnTimer = 0;                // +0x1A0
	bool _narrating = false;            // the narrator's intro holds input
	Common::String _narrator;           // +0x1A8: Knights_of_the_Snack_Table.350X_1_NARRATOR
	Common::String _sounds;
};

/** Knights of the Snack Table's room class for @p room, or nullptr. */
RoomClass *createKstRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_KST_ROOMS_H
