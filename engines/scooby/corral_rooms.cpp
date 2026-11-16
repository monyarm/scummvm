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

#include "scooby/corral_rooms.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kCorral = "ROOM_P30_Horseshoe_Corral";
static const char *const kChaseB5 = "ROOM_P28_Chase_B5";

// Sequence modes (0x430AA0).
static const int kStopped = 0;
static const int kLoop = 1;
static const int kOnce = 3;
static const int kOnceBack = 4;

void CorralRoom::playSound(const Common::String &name) {
	const bool found = _engine.getSound()->play(name);
	_lastSound = Common::String::format("%s found %d", name.c_str(), found ? 1 : 0);
}

int CorralRoom::meterTicks() const {
	// 9 - 3 x the level (game +0x160) ticks a step.
	return 9 - 3 * _engine.getDifficulty();
}

void CorralRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41DD00. The piles sprites under the first layer, stopped: Scooby's
	// at depth 3, Rascal's at 0. By their sequence: METERA and METERARROW
	// only give their place (the arrow's is the meter's); the counter's box
	// a sprite at depth -10000, looping; its two numbers sprites under the
	// second layer (game +0x5C) at depth -9999, looping, the left one the
	// tens. A third number isn't claimed.
	if (object == "OBJ_HC_SCOOBYPILE" || object == "OBJ_HC_RASCALPILE") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = object == "OBJ_HC_SCOOBYPILE" ? 3 : 0;
	} else if (claim.placedAnim == "ANIM_HC_METERA") {
		claim.build = Room::Claim::kSkip;
	} else if (claim.placedAnim == "ANIM_HC_METERARROW") {
		claim.build = Room::Claim::kSkip;
		_meterX = claim.x;
		_meterY = claim.y;
	} else if (claim.placedAnim == "ANIM_HC_COUNTERBOX") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = -10000;
	} else if (claim.placedAnim == "ANIM_HC_COUNTERNUMBERS" && _numbers < 2) {
		claim.build = Room::Claim::kSprite;
		claim.layer = 1;
		claim.depth = -9999;
		_numbers++;
	}
}

void CorralRoom::enter(const Common::String &from) {
	// 0x41DA80, after the base entry: the toolbar off (0x407CC0, in the
	// toolbar's hidden list); Scooby with the meter, Rascal, the three pegs;
	// the counter; then Scooby's turn, or Chase B5 if his pile is full.
	Room &room = *_engine.getRoom();
	_arcs.clear();
	_lastSound.clear();
	_scoobyPile = _rascalPile = -1;
	_tens = _ones = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		if (obj == "OBJ_HC_SCOOBYPILE" && !room.isEngineSprite(s)) {
			_scoobyPile = s;
			room.setPlayer(s, kStopped, 0);
		} else if (obj == "OBJ_HC_RASCALPILE" && !room.isEngineSprite(s)) {
			_rascalPile = s;
			room.setPlayer(s, kStopped, 0);
		} else if (obj == "OBJ_HC_COUNTER" && room.getAnim(s) == "ANIM_HC_COUNTERNUMBERS" && room.getDepth(s) == -9999) {
			if (_tens < 0) {
				_tens = s;
			} else if (_ones < 0) {
				_ones = s;
				if (room.getOffsetX(_ones) < room.getOffsetX(_tens))
					SWAP(_tens, _ones);
			}
		}
	}
	const int layer0 = room.getLayerSprite(0) >= 0 ? room.getLayerSprite(0) : 0;
	// Scooby (0x41E290): OBJ_HC_SCOOBY idle, looping, at depth 2; the meter
	// (OBJ_HC_METER on its arrow) at depth -10, stopped, at the arrow's place.
	_scooby = room.addEngineSprite(layer0, "OBJ_HC_SCOOBY", "ANIM_HC_SCOOBYIDLE", 2, 0, 0, 1, "corral Scooby");
	_meter = room.addEngineSprite(layer0, "OBJ_HC_METER", "ANIM_HC_METERARROW", -10, _meterX, _meterY, 1, "corral meter");
	room.setPlayer(_meter, kStopped, 0);
	room.setActor(_scooby, this);
	_scoobyState = 0;
	_scoobyDone = false;
	_strength = 0;
	_forward = true;
	// Rascal (0x41D770): his head idle once at depth -1, his body looping
	// one behind it.
	_rascal = room.addEngineSprite(layer0, "OBJ_HC_RASCAL", "ANIM_HC_RASCALHEAD_IDLE", -1, 0, 0, 1, "corral Rascal");
	room.setPlayer(_rascal, kOnce, 0);
	_rascalBody = room.addEngineSprite(layer0, "OBJ_HC_RASCAL", "ANIM_HC_RASCALBODY", -2, 0, 0, 1, "corral Rascal body");
	room.setActor(_rascal, this);
	_rascalState = 0;
	_rascalIdle = 0;
	_rascalDone = false;
	// The pegs, idle and looping, at depth -100.
	for (int n = 1; n <= 3; n++)
		_pegs[n - 1] = room.addEngineSprite(layer0, Common::String::format("OBJ_HC_PEG%d", n), Common::String::format("ANIM_HC_PEG%dIDLE", n), -100, 0, 0, 1,
		                                    Common::String::format("corral peg %d", n));
	showCounter();
	_turn = 0;
	if (_scoobyPile < 0 || room.getStep(_scoobyPile) != 10)
		setScooby(1);
	else
		_engine.requestRoom(kChaseB5);
}

void CorralRoom::roomLeft() {
	// 0x41DCE0, after the base leave: the counter's numbers forgotten.
	_tens = _ones = -1;
	_numbers = 0;
}

void CorralRoom::showCounter() {
	// 0x41E1A0's second half: the snacks, tens and ones, each number stopped
	// on its digit's step.
	Room &room = *_engine.getRoom();
	const uint snacks = _engine.getInventory()->getSnacks();
	if (_tens >= 0)
		room.setPlayer(_tens, kStopped, (snacks / 10) % 10);
	if (_ones >= 0)
		room.setPlayer(_ones, kStopped, snacks % 10);
}

void CorralRoom::setScooby(int state) {
	// 0x41E540.
	Room &room = *_engine.getRoom();
	switch (state) {
	case 0:
		room.startAnim(_scooby, "ANIM_HC_SCOOBYIDLE", kLoop, -1);
		break;
	case 1:
		// The meter sweeps forward from its step.
		room.setPlayer(_meter, kOnce, room.getStep(_meter), meterTicks());
		_forward = true;
		break;
	case 2:
		_scoobyDone = false;
		room.startAnim(_scooby, "ANIM_HC_SCOOBYTHROW", kOnce, -1);
		break;
	case 3:
		// A horseshoe off his pile.
		if (_scoobyPile >= 0)
			room.setPlayer(_scoobyPile, kStopped, room.getStep(_scoobyPile) + 1);
		break;
	case 5:
		room.setPlayer(_meter, kStopped, -1);
		room.startAnim(_scooby, "ANIM_HC_SCOOBYWATCH", kOnce, -1);
		break;
	default:
		break;
	}
	_scoobyState = state;
}

void CorralRoom::updateScooby() {
	// 0x41E3A0.
	Room &room = *_engine.getRoom();
	switch (_scoobyState) {
	case 1:
		// The meter sweeps back and forth; the button held (game +0x20 bit 1)
		// stops it, and its step is the strength.
		if (room.getMode(_meter) == 0) {
			if (_forward) {
				_forward = false;
				room.setPlayer(_meter, kOnceBack, -1, meterTicks());
			} else {
				_forward = true;
				room.setPlayer(_meter, kOnce, -1, meterTicks());
			}
		}
		if (_engine.isMouseDown()) {
			room.setPlayer(_meter, kStopped, -1);
			_strength = room.getStep(_meter);
			setScooby(2);
		}
		break;
	case 2:
		if (room.getStep(_scooby) == 2)
			setScooby(3);
		// fall through
	case 3:
		if (room.getStep(_scooby) == 15) {
			throwArc(false, _strength);
			setScooby(4);
		}
		break;
	case 4:
		if (room.getMode(_scooby) == 0)
			setScooby(5);
		break;
	case 5:
		if (room.getMode(_scooby) == 0 && _scoobyDone) {
			setScooby(0);
			turnOver();
		}
		break;
	default:
		break;
	}
}

void CorralRoom::setRascal(int state) {
	// 0x41D920.
	Room &room = *_engine.getRoom();
	if (state == 2) {
		_rascalDone = false;
		room.startAnim(_rascal, "ANIM_HC_RASCALHEAD_TOSS", kOnce, -1);
	} else if (state == 3) {
		// His horseshoe, at a strength of 1 to 9, and one off his pile.
		throwArc(true, _engine.getRandomNumber(8) + 1);
		if (_rascalPile >= 0)
			room.setPlayer(_rascalPile, kStopped, room.getStep(_rascalPile) + 1);
	} else if (state == 4) {
		room.startAnim(_rascal, "ANIM_HC_RASCALHEAD_IDLE", kOnce, -1);
	}
	_rascalState = state;
}

void CorralRoom::updateRascal() {
	// 0x41D860.
	Room &room = *_engine.getRoom();
	switch (_rascalState) {
	case 0:
		// Waiting: his idle again every 48 to 107 ticks.
		if (room.getMode(_rascal) == 0) {
			if (_rascalIdle == 0)
				_rascalIdle = _engine.getRandomNumber(59) + 48;
			else if (--_rascalIdle == 0)
				room.setPlayer(_rascal, kOnce, -1);
		}
		break;
	case 1:
		if (room.getMode(_rascal) == 0)
			setRascal(2);
		break;
	case 2:
		if (room.getStep(_rascal) == 7)
			setRascal(3);
		break;
	case 3:
		if (room.getMode(_rascal) == 0)
			setRascal(4);
		break;
	case 4:
		if (room.getMode(_rascal) == 0 && _rascalDone) {
			setRascal(0);
			turnOver();
		}
		break;
	default:
		break;
	}
}

void CorralRoom::throwArc(bool rascal, int strength, int digits) {
	// 0x41D210: a sprite under the first layer at depth -2.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	Arc arc;
	arc.rascal = rascal;
	const int layer0 = room.getLayerSprite(0) >= 0 ? room.getLayerSprite(0) : 0;
	if (rascal) {
		// OBJ_HC_RASCAL_ARCS on ANIM_HC_RASCAL_ARCS<strength> (8 is 9), once;
		// 1 to 3 ring that peg. Its sound Horseshoe_Corral.Rascal.Arc<n>.
		if (strength == 8)
			strength = 9;
		arc.anim = Common::String::format("ANIM_HC_RASCAL_ARCS%d", strength);
		arc.sprite = room.addEngineSprite(layer0, "OBJ_HC_RASCAL_ARCS", arc.anim, -2, 0, 0, 1, "corral arc");
		room.startAnim(arc.sprite, arc.anim, kOnce, -1);
		arc.peg = strength < 4 ? strength : 0;
		arc.sound = Common::String::format("Horseshoe_Corral.Rascal.Arc%d", strength);
		playSound(arc.sound);
	} else {
		// ANIM_HC_ARCS<strength>x<nn>: nn drawn from rand() % 20, then rand()
		// modulo the draw before, until it or its H (a ringer) is in the art,
		// or the draw is 0.
		uint n = 20;
		Common::String name;
		while (true) {
			n = digits >= 0 ? (uint)digits : _engine.getRandomNumber(n - 1);
			name = Common::String::format("ANIM_HC_ARCS%dx%.2d", strength, n);
			if (room.hasAnim(name))
				break;
			name += "H";
			if (room.hasAnim(name) || n == 0 || digits >= 0)
				break;
		}
		debugC(1, kDebugScript, "Using arc: %s", name.c_str());
		arc.anim = name;
		arc.sprite = room.addEngineSprite(layer0, "OBJ_HC_ARCS", name, -2, 0, 0, 1, "corral arc");
		// The fourth strength's third arc is drawn in front of everything.
		if (strength == 4 && n == 2)
			room.setDepth(arc.sprite, 10000);
		room.startAnim(arc.sprite, name, kOnce, -1);
		arc.peg = (strength >= 1 && strength <= 3 && name.lastChar() == 'H') ? strength : 0;
		arc.sound = Common::String::format("Horseshoe_Corral.Arc_0%dx%.2d", strength, n);
		if (!sound.findClip(arc.sound))
			arc.sound += "H";
		playSound(arc.sound);
	}
	room.setActor(arc.sprite, this);
	_arcs.push_back(arc);
}

void CorralRoom::updateArc(Arc &arc) {
	// 0x41D5B0. Landed: on a peg, the sprite becomes the peg with the
	// horseshoe on it (OBJ_HC_PEG<n>, its HORSESHOE_HIT or RASCAL_HIT once)
	// and Rascal's peg sound plays; then 36 ticks. Those over: both throwers
	// told, a snack for Scooby's ringer (0x407E00, at most 99), and the
	// sprite paused where it is.
	Room &room = *_engine.getRoom();
	if (arc.state == 0) {
		if (room.getMode(arc.sprite) != 0)
			return;
		if (arc.peg) {
			const Common::String obj = Common::String::format("OBJ_HC_PEG%d", arc.peg);
			const Common::String anim = Common::String::format(arc.rascal ? "ANIM_HC_PEG%dRASCAL_HIT" : "ANIM_HC_PEG%dHORSESHOE_HIT", arc.peg);
			room.setSequence(arc.sprite, obj, anim, kOnce, -1);
			playSound(Common::String::format("Horseshoe_Corral.Rascal.Peg%d.Hit", arc.peg));
		}
		arc.countdown = 36;
		arc.state = 1;
	} else if (arc.state != 1) {
		return;
	}
	if (room.getMode(arc.sprite) == 0 && arc.countdown-- == 0) {
		_scoobyDone = true;
		_rascalDone = true;
		if (!arc.rascal && arc.peg) {
			Inventory &inventory = *_engine.getInventory();
			inventory.setSnacks(inventory.getSnacks() + 1);
			showCounter();
		}
		room.setPaused(arc.sprite, true);
		arc.state = 2;
	}
}

void CorralRoom::turnOver() {
	// 0x41E140: Scooby's throw over, Rascal's turn; Rascal's over, Scooby's,
	// or Chase B5 once Scooby's pile is at its last step (10).
	Room &room = *_engine.getRoom();
	if (_turn == 0) {
		_turn = 1;
		setRascal(1);
	} else if (_turn == 1) {
		_turn = 0;
		if (_scoobyPile < 0 || room.getStep(_scoobyPile) != 10)
			setScooby(1);
		else
			_engine.requestRoom(kChaseB5);
	}
}

void CorralRoom::actorUpdate(uint sprite) {
	if ((int)sprite == _scooby) {
		updateScooby();
		return;
	}
	if ((int)sprite == _rascal) {
		updateRascal();
		return;
	}
	for (uint i = 0; i < _arcs.size(); i++) {
		if (_arcs[i].sprite == (int)sprite) {
			updateArc(_arcs[i]);
			return;
		}
	}
}

RoomClass *createCorralRoom(ScoobyEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kCorral)
		return new CorralRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
