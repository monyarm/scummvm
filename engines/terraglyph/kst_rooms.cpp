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
#include "common/util.h"

#include "terraglyph/kst_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const int kStopped = 0;
static const int kLoop = 1;
static const int kOnce = 3;

// The knight's x enters 40 off the placement, from off the left (0x45EE50).
static const float kKnightXOffset = 40.0f;
// A full mug spawns at (550, 40) and steps down the counter (0x41A5C0, 0x45EE48).
static const float kDrinkSpawnX = 550.0f;
static const float kDrinkSpawnY = 40.0f;
static const float kDrinkStepX = 35.0f;
// The knight's reach toward its seat (0x45ED74).
static const float kReachScale = 0.005f;

int KstRoom::layer0() const {
	const int l = _engine.getRoom()->getLayerSprite(0);
	return l >= 0 ? l : 0;
}

void KstRoom::playSound(const Common::String &name) {
	const bool found = _engine.getSound()->play(name);
	if (!_sounds.empty())
		_sounds += ", ";
	_sounds += Common::String::format("%s found %d", name.c_str(), found ? 1 : 0);
}

void KstRoom::placeSprite(int sprite, float x, float y) {
	if (sprite >= 0)
		_engine.getRoom()->setOffset(sprite, (int)x, (int)y);
}

void KstRoom::insertMarker(float *primary, float *parallel, uint &count, float key, float val) {
	// 0x41A970's insertion: the primary array stays sorted ascending by its key
	// (the placement's y), the parallel (the placement's x) following it.
	if (count >= kMarkers) {
		warning("More than 3 objects of the same type specified in Knights Table Room");
		return;
	}
	for (uint k = 0; k < count; k++) {
		if (key < primary[k]) {
			SWAP(key, primary[k]);
			if (parallel)
				SWAP(val, parallel[k]);
		}
	}
	primary[count] = key;
	if (parallel)
		parallel[count] = val;
	count++;
}

void KstRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41A970. Shaggy's post, the knights' seats and the benches' rows are
	// marker placements the room keeps and does not build; the three table
	// overlays and the two counter digits are sprites.
	const float x = (float)claim.x, y = (float)claim.y;
	const Common::String &anim = claim.placedAnim;
	if (object == "OBJ_KST_SHAGGY") {
		_shaggyX = x;
		_shaggyY = y;
		_hasShaggy = true;
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_KST_KNIGHT1") {
		// The seat's y sorts; its x (less 40, off the left) follows.
		insertMarker(_knightY, _knightX, _knightCount, y, x - kKnightXOffset);
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_KST_OVERLAY1" || object == "OBJ_KST_OVERLAY2" || object == "OBJ_KST_OVERLAY3") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
	} else if (object == "OBJ_KST_MUG") {
		if (anim == "ANIM_KST_MUGEMPTY")
			insertMarker(_benchEndY, _benchEndX, _benchEndCount, y, x);
		else if (anim == "ANIM_KST_MUGEMPTY_BREAKS")
			insertMarker(_benchFloorY, nullptr, _benchFloorCount, y, x);
		else if (anim == "ANIM_KST_MUGFULL")
			insertMarker(_drinkY, _drinkX, _drinkCount, y, x);
		else if (anim == "ANIM_KST_MUGFULL_BREAKS")
			insertMarker(_benchStartY, _benchStartX, _benchStartCount, y, x);
		claim.build = Room::Claim::kSkip;
	} else if (anim == "ANIM_KST_COUNTERNUMBERS") {
		if (_numbers < 2) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 10; // past the ten layer slots: the root
			claim.depth = 100;
			_numbers++;
		} else {
			warning("Not enough character start positions specified in Pie Noon");
		}
	}
	// OBJ_KST_COUNTER's box, OBJ_KST_EXIT and OBJ_KST_FLAME fall to the base path.
}

void KstRoom::startRound() {
	// 0x41A5C0: six knight slots, parked until the serving loop walks them in;
	// a full mug for each knight still owed, stacked down the counter from 550.
	Room &room = *_engine.getRoom();
	_round = 1;
	for (uint i = 0; i < kKnightSlots; i++) {
		Knight &k = _knights[i];
		k = Knight();
		k.slot = i % 3;
		k.state = 4; // parked, off-screen
	}
	// The full mugs the gang still has to pour (the trace keeps them in a list;
	// the room shows them stacked on the counter).
	float dx = kDrinkSpawnX;
	for (int d = 0; d < _toServe; d++) {
		room.addEngineSprite(layer0(), "OBJ_KST_MUG", "ANIM_KST_MUGFULL", 100, (int)dx, (int)kDrinkSpawnY, 1, "kst drink");
		dx -= kDrinkStepX;
	}
}

void KstRoom::enter(const Common::String &from) {
	// 0x419A80, after the base entry.
	Room &room = *_engine.getRoom();
	_sounds.clear();
	if (!_hasShaggy)
		warning("Shaggy's post not specified in Knights Table Room");
	if (_knightCount != kMarkers)
		warning("3 knight markers should be placed in Knights Table Room");
	if (_benchEndCount != kMarkers)
		warning("3 bench end markers should be placed in Knights Table Room");
	if (_benchFloorCount != kMarkers)
		warning("3 bench floor markers should be placed in Knights Table Room");
	if (_drinkCount != kMarkers)
		warning("3 drink spawn markers should be placed in Knights Table Room");
	if (_benchStartCount != kMarkers)
		warning("3 bench start markers should be placed in Knights Table Room");

	// The sprites the claim built: the three overlays and the two counter digits.
	_tens = _ones = -1;
	for (uint i = 0; i < 3; i++)
		_overlays[i] = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		const Common::String &anim = room.getAnim(s);
		if (room.isEngineSprite(s))
			continue;
		if (obj == "OBJ_KST_OVERLAY1")
			_overlays[0] = s;
		else if (obj == "OBJ_KST_OVERLAY2")
			_overlays[1] = s;
		else if (obj == "OBJ_KST_OVERLAY3")
			_overlays[2] = s;
		else if (anim == "ANIM_KST_COUNTERNUMBERS" && room.getDepth(s) == 100) {
			if (_tens < 0) {
				_tens = s;
			} else if (_ones < 0) {
				_ones = s;
				if (room.getOffsetX(_ones) < room.getOffsetX(_tens))
					SWAP(_tens, _ones);
			}
		}
	}
	if (_ones < 0)
		warning("2 numbers should be placed in Knights Table Room");

	// The level's quota: five knights, less the difficulty.
	_toServe = 5 - _engine.getDifficulty();
	showCounter();
	startRound();

	// Shaggy at his post (0x4192F0: ANIM_KST_SHAGGYIDLE), ready to serve.
	_shaggy = -1;
	if (_hasShaggy) {
		_shaggy = room.addEngineSprite(layer0(), "OBJ_KST_SHAGGY", "ANIM_KST_SHAGGYIDLE", (int)_shaggyY, (int)_shaggyX, (int)_shaggyY, 1, "kst Shaggy");
		room.setPlayer(_shaggy, kOnce, 0);
	}

	// The narrator's intro, only the first visit; the ambience under it.
	_narrator = "Knights_of_the_Snack_Table.350X_1_NARRATOR";
	_narrating = false;
	_spawnTimer = _engine.getRandomNumber(99) + 50;
	playSound("Knights.Ambience");
}

void KstRoom::roomLeft() {
	// 0x419CC0, after the base leave: the markers to be read again, the round torn down.
	_knightCount = _benchEndCount = _benchFloorCount = _drinkCount = _benchStartCount = 0;
	_hasShaggy = false;
	_tens = _ones = -1;
	_numbers = 0;
	_shaggy = -1;
	for (uint i = 0; i < 3; i++)
		_overlays[i] = -1;
	for (uint i = 0; i < kKnightSlots; i++)
		_knights[i] = Knight();
}

void KstRoom::showCounter() {
	Room &room = *_engine.getRoom();
	const int n = _toServe;
	if (_tens >= 0)
		room.setPlayer(_tens, kStopped, (n / 10) % 10);
	if (_ones >= 0)
		room.setPlayer(_ones, kStopped, n % 10);
}

void KstRoom::serve() {
	// 0x419680: Shaggy drops the mug onto the knight and says Zoinks.
	if (_shaggy >= 0)
		_engine.getRoom()->setSequence(_shaggy, "OBJ_KST_SHAGGY", "ANIM_KST_SHAGGYDROP_MUG", kOnce, 0);
	playSound("Knights.Zoinks");
	if (_toServe > 0)
		_toServe--;
	showCounter();
}

void KstRoom::tick() {
	// 0x419E40, after the base tick. The serving loop: knights walk up to the
	// benches and drink; Shaggy serves them; once the level's quota is met, the
	// way back to the dining room opens.
	if (_narrating) {
		if (!_engine.getSound()->isClipPlaying(_narrator))
			_narrating = false;
		return;
	}
	if (_toServe <= 0) {
		_engine.requestRoom("ROOM_P12_Dining_Room");
		return;
	}
	// Walk a knight into a free seat on the spawn timer.
	if (_spawnTimer > 0) {
		_spawnTimer--;
		return;
	}
	Room &room = *_engine.getRoom();
	for (uint i = 0; i < kKnightSlots; i++) {
		Knight &k = _knights[i];
		if (k.state != 4)
			continue;
		const uint slot = k.slot;
		if (slot >= _knightCount)
			break;
		k.state = 1; // walking
		// 0x419110: the knight enters at its x (off the left) on its seat's row,
		// and walks toward the bench-end's x.
		k.x = _knightX[slot];
		k.y = _benchEndY[slot];
		k.target = _benchEndX[slot];
		k.sprite = room.addEngineSprite(layer0(), "OBJ_KST_KNIGHT1", "ANIM_KST_KNIGHT1WALKS", (int)k.y, (int)k.x, (int)k.y, 1, "kst knight");
		room.setActor(k.sprite, this);
		_spawnTimer = _engine.getRandomNumber(0x47) + 0x24;
		break;
	}
}

void KstRoom::actorUpdate(uint sprite) {
	// A knight walks toward its seat, then drinks; Shaggy serves it.
	for (uint i = 0; i < kKnightSlots; i++) {
		Knight &k = _knights[i];
		if (k.sprite != (int)sprite || k.state == 4)
			continue;
		Room &room = *_engine.getRoom();
		if (k.state == 1) {
			if (ABS(k.target - k.x) <= 4.0f) {
				k.x = k.target;
				k.state = 2; // reached the seat, starts drinking
				room.setSequence(k.sprite, "OBJ_KST_KNIGHT1", "ANIM_KST_KNIGHT1STARTS_DRINKS", kOnce, 0);
				serve();
			} else {
				k.x += (k.target - k.x) * kReachScale * 20.0f;
			}
			placeSprite(k.sprite, k.x, k.y);
		} else if (k.state == 2 && room.isStopped(k.sprite)) {
			k.state = 4; // served, parked again
			room.detachSprite(k.sprite);
			k.sprite = -1;
		}
		return;
	}
}

RoomClass *createKstRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == "ROOM_P31_KST")
		return new KstRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
