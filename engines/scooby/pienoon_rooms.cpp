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

#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/pienoon_rooms.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kPieNoon = "ROOM_P32_Pie_Noon";

// Sequence modes (0x430AA0).
static const int kStopped = 0;
static const int kLoop = 1;
static const int kLoopBack = 2;
static const int kOnce = 3;

// The runners' names (+0x68), in the room's slots' order.
static const char *const kNames[PieNoonRoom::kRunners] = { "SHAGGY", "FRED", "VELMA", "DAPHNE" };

// The splat sound's number (0x475F08), 0 to 4 in turn; kept for the game.
static int g_splat = 0;

void PieNoonRoom::setSplat(int n) {
	g_splat = n;
}

int PieNoonRoom::layer0() const {
	const int l = _engine.getRoom()->getLayerSprite(0);
	return l >= 0 ? l : 0;
}

void PieNoonRoom::playSound(const Common::String &name) {
	const bool found = _engine.getSound()->play(name);
	_lastSound = Common::String::format("%s found %d", name.c_str(), found ? 1 : 0);
	if (!_sounds.empty())
		_sounds += ", ";
	_sounds += _lastSound;
	debugC(1, kDebugScript, "Pie Noon plays %s", _lastSound.c_str());
}

void PieNoonRoom::placeSprite(int sprite, float x, float y) {
	// A sprite is drawn at its place truncated (__ftol).
	if (sprite >= 0)
		_engine.getRoom()->setOffset(sprite, (int)x, (int)y);
}

double PieNoonRoom::leftInner(float z) const {
	return ((double)z - _ghostY) * _slope[0] + _ghostX[1];
}

double PieNoonRoom::rightInner(float z) const {
	return ((double)z - _ghostY) * _slope[1] + _ghostX[2];
}

double PieNoonRoom::leftOuter(float z) const {
	return ((double)z - _ghostY) * _slope[2] + _ghostX[0];
}

double PieNoonRoom::rightOuter(float z) const {
	return ((double)z - _ghostY) * _slope[3] + _ghostX[3];
}

void PieNoonRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41C190. Scooby's placements give his row and the ends of his walk;
	// the ghost's, its row and its four places, by x; the gang's, the five
	// start places, by y (0x41C880). None of them is built. The overlays
	// sprites under the first layer, kept by their sequence's last two
	// letters (FR, FL, BR, BL); the clock a sprite under it, stopped on step
	// level x 15; the counter's two numbers sprites at depth 9999 under no
	// layer, the left one the tens.
	const float x = (float)claim.x, y = (float)claim.y;
	if (object == "OBJ_PIE_NOON_SCOOBY") {
		_scoobyY = y;
		if (_scoobyMax < x)
			_scoobyMax = x;
		if (x < _scoobyMin)
			_scoobyMin = x;
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_PIE_NOON_GHOST") {
		_ghostY = y;
		if (_ghostCount > 3)
			warning("Start position overflow creating Pie Noon start positions.");
		float v = x;
		uint k = 0;
		for (; k < _ghostCount && k < 4; k++) {
			if (v < _ghostX[k])
				SWAP(v, _ghostX[k]);
		}
		if (k < 4)
			_ghostX[k] = v;
		_ghostCount++;
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_PIE_NOON_SHAGGY" || object == "OBJ_PIE_NOON_DAPHNE" || object == "OBJ_PIE_NOON_VELMA" || object == "OBJ_PIE_NOON_FRED") {
		if (_startCount >= 5)
			warning("Start position overflow creating Pie Noon start positions.");
		float sx = x, sy = y;
		uint k = 0;
		for (; k < _startCount && k < 5; k++) {
			if (sy < _startY[k]) {
				SWAP(sx, _startX[k]);
				SWAP(sy, _startY[k]);
			}
		}
		if (k < 5) {
			_startX[k] = sx;
			_startY[k] = sy;
		}
		_startCount++;
		claim.build = Room::Claim::kSkip;
	} else if (object == "OBJ_PIE_NOON_OVERLAY") {
		const Common::String &a = claim.placedAnim;
		if (a.hasSuffix("FR") || a.hasSuffix("FL") || a.hasSuffix("BR") || a.hasSuffix("BL")) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 0;
			claim.depth = 0;
		}
	} else if (object == "OBJ_PIE_NOON_CLOCK") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
	} else if (claim.placedAnim == "ANIM_PIE_NOON_COUNTERNUMBERS") {
		if (_numbers < 2) {
			claim.build = Room::Claim::kSprite;
			claim.layer = 10; // past the ten layer slots: the root
			claim.depth = 9999;
			_numbers++;
		} else {
			warning("Only 2 numbers should be placed in Pie Noon");
		}
	}
}

void PieNoonRoom::enter(const Common::String &from) {
	// 0x41B8E0, after the base entry.
	Room &room = *_engine.getRoom();
	_sounds.clear();
	_lastSound.clear();
	if (_startCount < 5)
		warning("Not enough character start positions specified in Pie Noon");
	if (_scoobyMin == _scoobyMax)
		warning("Not enough scooby start positions specified in Pie Noon");
	if (_ghostCount < 4)
		warning("Not enough ghost start positions specified in Pie Noon");

	// The sprites the claim made.
	_tens = _ones = -1;
	_clock = -1;
	for (uint i = 0; i < 4; i++)
		_overlays[i] = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		const Common::String &anim = room.getAnim(s);
		if (room.isEngineSprite(s))
			continue;
		if (obj == "OBJ_PIE_NOON_CLOCK")
			_clock = s;
		else if (obj == "OBJ_PIE_NOON_OVERLAY")
			_overlays[anim.hasSuffix("FR") ? 0 : anim.hasSuffix("FL") ? 1 : anim.hasSuffix("BR") ? 2 : 3] = s;
		else if (anim == "ANIM_PIE_NOON_COUNTERNUMBERS" && room.getDepth(s) == 9999) {
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
		warning("2 numbers should be placed in Pie Noon");
	if (_clock >= 0)
		room.setPlayer(_clock, kStopped, _engine.getDifficulty() * 15);

	// The stage's edges: lines from the ghost's row to Scooby's.
	const double depth = (double)_scoobyY - _ghostY;
	_slope[0] = (float)(((double)_scoobyMin - _ghostX[1]) / depth);
	_slope[1] = (float)(((double)_scoobyMax - _ghostX[2]) / depth);
	_slope[2] = (float)(((double)_scoobyMin - _ghostX[0]) / depth);
	_slope[3] = (float)(((double)_scoobyMax - _ghostX[3]) / depth);
	// The lanes: the back from the first two start places, the middle from
	// the next two, the front carried on to the fifth's row.
	_laneA[2] = MIN(_startX[0], _startX[1]);
	_laneB[2] = MAX(_startX[0], _startX[1]);
	_laneY[2] = _startY[0];
	if (_startX[3] <= _startX[2]) {
		_laneA[1] = _startX[3];
		_laneB[1] = _startX[2];
	} else {
		_laneA[1] = _startX[2];
		_laneB[1] = _startX[3];
	}
	_laneY[1] = _startY[2];
	_laneY[0] = _startY[4];
	const double span = (double)_startY[2] - _startY[0];
	const double reach = (double)_startY[4] - _startY[0];
	_laneA[0] = (float)((((double)_laneA[1] - _laneA[2]) / span) * reach + _laneA[2]);
	_laneB[0] = (float)(reach * (((double)_laneB[1] - _laneB[2]) / span) + _laneB[2]);

	// The ghost (0x41A550) under the first layer, half way along its row,
	// facing right; then Scooby (0x41CD50) under no layer, at his row's depth,
	// at the left end of his walk.
	_ghost = room.addEngineSprite(layer0(), "OBJ_PIE_NOON_GHOST", "ANIM_PIE_NOON_GHOSTIDLE_LEFT", 0, 0, 0, 1, "pie noon ghost");
	_gy = _ghostY;
	_gx = (float)(_ghostX[0] + ((double)_ghostX[3] - _ghostX[0]) * 0.5f);
	_ghostRight = true;
	_ghostHidden = false;
	_smoke = -1;
	_ghostState = 8;
	_gspeed = (float)(((double)_ghostX[3] - _ghostX[0]) * 0.005f);
	setGhost(0);
	_lookout = 0;
	_goodJob = 0;
	placeSprite(_ghost, _gx, _gy);
	room.setActor(_ghost, this);

	_scooby = room.addEngineSprite(0, "OBJ_PIE_NOON_SCOOBY", "ANIM_PIE_NOON_SCOOBYWALK_LEFT_ARM", (int)_scoobyY, 0, 0, 1, "pie noon Scooby");
	room.setPlayer(_scooby, kStopped, 0);
	_scoobyX = _scoobyMin;
	_latch = false;
	_scoobyState = 3;
	_scoobyRight = false;
	_throws = 1;
	placeSprite(_scooby, _scoobyX, _scoobyY);
	room.setActor(_scooby, this);

	for (uint i = 0; i < kPies; i++)
		_pieAlive[i] = false;
	for (uint i = 0; i < kRunners; i++)
		_runnerAlive[i] = false;
	for (uint l = 0; l < kLanes; l++) {
		_laneState[l] = 2;
		_laneTimer[l] = -216;
	}
	showCounter();
	playSound("Pie_Noon.Music");
	playSound("Pie_Noon.Intro");
	// The overlays just in front of the middle and back lanes.
	if (_overlays[0] >= 0)
		room.setDepth(_overlays[0], (int)_laneY[1] + 1);
	if (_overlays[1] >= 0)
		room.setDepth(_overlays[1], (int)_laneY[1] + 1);
	if (_overlays[2] >= 0)
		room.setDepth(_overlays[2], (int)_laneY[2] + 1);
	if (_overlays[3] >= 0)
		room.setDepth(_overlays[3], (int)_laneY[2] + 1);
	_intro = true;
	_lastStep = _clock >= 0 ? room.getStep(_clock) : 0;
	_tock = true;
	_chime = false;
}

void PieNoonRoom::roomLeft() {
	// 0x41C130, after the base leave: the places to be read again.
	_scoobyMax = -10000;
	_scoobyMin = 10000;
	_scooby = -1;
	_startCount = 0;
	_ghostCount = 0;
	for (uint i = 0; i < kRunners; i++)
		_runnerAlive[i] = false;
	_tens = _ones = -1;
	_numbers = 0;
}

void PieNoonRoom::showCounter() {
	Room &room = *_engine.getRoom();
	const uint snacks = _engine.getInventory()->getSnacks();
	if (_tens >= 0)
		room.setPlayer(_tens, kStopped, (snacks / 10) % 10);
	if (_ones >= 0)
		room.setPlayer(_ones, kStopped, snacks % 10);
}

void PieNoonRoom::addSnack() {
	// 0x41CB50: a snack (0x407E00, at most 99), and the counter.
	Inventory &inventory = *_engine.getInventory();
	inventory.setSnacks(inventory.getSnacks() + 1);
	showCounter();
}

// ---------------------------------------------------------------------------
// The runners

void PieNoonRoom::addRunner(uint which) {
	// 0x41CC40: a lane drawn (rand() % 3); if it has been free over 10 ticks,
	// a runner there, running as the lane says: 0 left, 1 right, 2 either.
	const uint lane = _engine.getRandomNumber(2);
	if (_laneTimer[lane] <= 10)
		return;
	bool right = false;
	if (_laneState[lane] == 1)
		right = true;
	else if (_laneState[lane] == 2)
		right = _engine.getRandomNumber(1) == 0;
	spawnRunner(which, lane, right);
	_laneTimer[lane] = 0;
	_laneState[lane] = right ? 1 : 0;
}

void PieNoonRoom::spawnRunner(uint which, uint lane, bool right) {
	// 0x419C50: under the first layer; running; from one end of its lane to
	// the other in 100 ticks; at its lane's row and depth.
	Room &room = *_engine.getRoom();
	Runner &r = _runners[which];
	r = Runner();
	r.name = kNames[which];
	r.lane = lane;
	r.right = right;
	r.sprite = room.addEngineSprite(layer0(), "OBJ_PIE_NOON_" + r.name, Common::String::format("ANIM_PIE_NOON_%sRUN_%s%d", r.name.c_str(), right ? "RIGHT" : "LEFT", lane + 1),
	                                0, 0, 0, 1, "pie noon " + r.name);
	_runnerAlive[which] = true;
	runRunner(which);
	if (!right) {
		r.target = _laneA[lane];
		r.x = _laneB[lane];
	} else {
		r.x = _laneA[lane];
		r.target = _laneB[lane];
	}
	r.speed = (float)(((double)r.target - r.x) * 0.01f);
	r.y = _laneY[lane];
	room.setDepth(r.sprite, (int)r.y);
	placeSprite(r.sprite, r.x, r.y);
	room.setActor(r.sprite, this);
}

void PieNoonRoom::runRunner(uint which) {
	// 0x41A310: OBJ_PIE_NOON_<NAME> on its RUN_<side><lane>, looping; its
	// _Hit.X stopped; running.
	Room &room = *_engine.getRoom();
	Runner &r = _runners[which];
	room.setSequence(r.sprite, "OBJ_PIE_NOON_" + r.name, Common::String::format("ANIM_PIE_NOON_%sRUN_%s%d", r.name.c_str(), r.right ? "RIGHT" : "LEFT", r.lane + 1),
	                 kLoop, -1);
	r.hitTicks = 0;
	r.hitX = false;
	_engine.getSound()->stopClip("Pie_Noon." + r.name + "_Hit.X");
	r.state = 0;
}

void PieNoonRoom::hitRunner(uint which) {
	// 0x41A110: its HIT, looping, for 12 ticks from the last pie; a hit line
	// (1 or 2), or after 72 ticks of being hit its _Hit.X, once.
	Room &room = *_engine.getRoom();
	Runner &r = _runners[which];
	room.startAnim(r.sprite, Common::String::format("ANIM_PIE_NOON_%sHIT_%s%d", r.name.c_str(), r.right ? "RIGHT" : "LEFT", r.lane + 1), kLoop, -1);
	r.flash = 12;
	r.state = 1;
	if (!r.hitX && r.hitTicks < 72) {
		playSound(Common::String::format("Pie_Noon.%s_Hit.%d", r.name.c_str(), _engine.getRandomNumber(1) + 1));
	} else if (r.hitTicks > 72) {
		r.hitX = true;
		playSound("Pie_Noon." + r.name + "_Hit.X");
		r.hitTicks = 1;
	}
}

bool PieNoonRoom::runnerHitTest(uint which, float px, float py, float pz) {
	// 0x419EB0: the pie within a tenth of the stage's width of the runner's
	// row, inside its frame's first cel, and on its pixels: hit. Inside the
	// cel but off them, a first time, its _Miss.
	Room &room = *_engine.getRoom();
	Runner &r = _runners[which];
	const float ly = _laneY[r.lane];
	const double w = (float)rightInner(ly) - leftInner(ly);
	const int near = (int)((double)ly - pz);
	if (!(ABS(near) < (int)(w * 0.1f)))
		return false;
	int fx, fy, fw, fh;
	if (!room.getFrameRect(r.sprite, fx, fy, fw, fh))
		return false;
	// The cel's place from the runner's own (float) place.
	const float x0 = (float)(fx - room.getOffsetX(r.sprite)) + r.x;
	const float y0 = (float)(fy - room.getOffsetY(r.sprite)) + r.y;
	const float x1 = (float)fw + x0;
	const float y1 = (float)fh + y0;
	if (!(x0 < px && y0 < py && px < x1 && py < y1))
		return false;
	if (room.hitsPoint(r.sprite, (int)px, (int)py)) {
		hitRunner(which);
		return true;
	}
	if (r.hitTicks == 0)
		playSound("Pie_Noon." + r.name + "_Miss");
	return false;
}

void PieNoonRoom::updateRunner(uint which) {
	// 0x419DE0. Running: along by its speed; past the lane's end, gone (its
	// slot free, the sprite paused). Hit: until the room's countdown runs
	// out, counting; then running again.
	Room &room = *_engine.getRoom();
	Runner &r = _runners[which];
	if (r.state == 0) {
		const float x = r.speed + r.x;
		r.x = x;
		placeSprite(r.sprite, r.x, r.y);
		if ((0.0f < r.speed && r.target < x) || (r.speed < 0.0f && x < r.target)) {
			_runnerAlive[which] = false;
			room.setPaused(r.sprite, true);
		}
	} else if (r.state == 1) {
		if (r.flash != 0)
			r.hitTicks++;
		else
			runRunner(which);
	}
}

// ---------------------------------------------------------------------------
// The ghost

void PieNoonRoom::setGhost(int state) {
	// 0x41AA40. Its side, and the other.
	if (_ghostState == state)
		return;
	_ghostState = state;
	Room &room = *_engine.getRoom();
	const char *side = _ghostRight ? "RIGHT" : "LEFT";
	const char *other = _ghostRight ? "LEFT" : "RIGHT";
	switch (state) {
	case 0:
		_ghostHidden = false;
		_ghostCountdown = 25;
		room.startAnim(_ghost, Common::String::format("ANIM_PIE_NOON_GHOSTIDLE_%s", side), kLoop, -1);
		break;
	case 1: {
		_ghostCountdown = 15;
		room.startAnim(_ghost, Common::String::format("ANIM_PIE_NOON_GHOSTHIT_%s", side), kLoop, -1);
		_engine.getSound()->stopClip("Pie_Noon.Ghost_Hit");
		playSound("Pie_Noon.Ghost_Hit");
		// Every hit: a Good Job line (1-8).
		const int v = _goodJob--;
		if (v == 0) {
			playSound(Common::String::format("Pie_Noon.Good_Job.%d", _engine.getRandomNumber(7) + 1));
			_engine.getRandomNumber(1); // its pan, left or right
			_goodJob = 0;
		}
		break;
	}
	case 2:
		if (_scooby >= 0 && _scoobyState != 0 && _scoobyState != 1 && _laneTimer[0] >= 0) {
			room.startAnim(_ghost, Common::String::format("ANIM_PIE_NOON_GHOSTTHROW_%s_TO_%s", side, other), kOnce, -1);
			_ghostRight = !_ghostRight;
			break;
		}
		_ghostState = 5;
		// fall through
	case 5:
		room.startAnim(_ghost, Common::String::format("ANIM_PIE_NOON_GHOSTTRANS_%s_TO_%s", side, other), kOnce, -1);
		_ghostRight = !_ghostRight;
		break;
	case 4:
		room.startAnim(_ghost, Common::String::format("ANIM_PIE_NOON_GHOSTRUN_%s_TO_%s", other, side), kLoop, -1);
		break;
	case 7:
		_gx = (float)((double)_engine.getRandomNumber(32767) * 3.0518509447574615e-05f);
		_gx = (float)(((double)_ghostX[3] - _ghostX[0]) * _gx);
		_gx = _gx + _ghostX[0];
		placeSprite(_ghost, _gx, _gy);
		// fall through
	case 6:
		// Smoke at its place, a row in front of it, once.
		_smoke = room.addEngineSprite(layer0(), "OBJ_PIE_NOON_SMOKE", "ANIM_PIE_NOON_SMOKECYCLE", (int)(_gy + 1.0f), (int)_gx, (int)_gy, 1, "pie noon smoke");
		room.setPlayer(_smoke, kOnce, 0);
		playSound("Pie_Noon.Smoke_Cycle");
		break;
	case 8:
		_ghostHidden = true;
		_ghostCountdown = 20;
		break;
	default:
		break;
	}
	room.setHidden(_ghost, _ghostHidden);
}

void PieNoonRoom::updateGhost() {
	// 0x41A650.
	Room &room = *_engine.getRoom();
	if (_smoke >= 0 && room.getMode(_smoke) == 0) {
		room.detachSprite(_smoke);
		_smoke = -1;
	}
	switch (_ghostState) {
	case 0: {
		// Idle: after its countdown, smoke (1 in 7), a walk (2 in 7), a
		// vanishing (1 in 7, if no smoke is up) or a throw.
		if (_ghostCountdown-- != 0)
			return;
		const uint r = _engine.getRandomNumber(6);
		if (r == 0)
			setGhost(5);
		else if (r == 1 || r == 2)
			setGhost(4);
		else if (r == 3 && _smoke < 0)
			setGhost(6);
		else
			setGhost(2);
		return;
	}
	case 1:
		// Hit: after its countdown, a walk or a throw.
		if (_ghostCountdown-- == 0)
			setGhost(_engine.getRandomNumber(1) ? 4 : 2);
		return;
	case 2:
		// Throwing: on its step 2, the pie, Ghost_Throw, and every 6 to 10
		// throws a Lookout line.
		if (room.getStep(_ghost) == 2) {
			throwPie(_gx, _gy);
			setGhost(3);
			playSound("Pie_Noon.Ghost_Throw");
			const int v = _lookout--;
			if (v == 0) {
				playSound(Common::String::format("Pie_Noon.Lookout.%d", _engine.getRandomNumber(2) + 1));
				_engine.getRandomNumber(1); // its pan
				_lookout = _engine.getRandomNumber(4) + 5;
			}
		}
		// fall through
	case 3:
		if (room.getMode(_ghost) == 0)
			setGhost(0);
		return;
	case 4: {
		// Walking: past an end of its row, a throw or a turn; else idle, 1
		// in 90.
		_gspeed = _ghostRight ? ABS(_gspeed) : -ABS(_gspeed);
		_gx = _gx + _gspeed;
		placeSprite(_ghost, _gx, _gy);
		if ((!_ghostRight && _gx < _ghostX[0]) || (_ghostRight && _ghostX[3] < _gx)) {
			setGhost(_engine.getRandomNumber(1) == 0 ? 2 : 5);
		} else if (_engine.getRandomNumber(89) == 0) {
			setGhost(0);
		}
		return;
	}
	case 5:
		if (room.getMode(_ghost) == 0)
			setGhost(_engine.getRandomNumber(1) ? 0 : 4);
		return;
	case 6:
		if (_smoke >= 0 && room.getStep(_smoke) == 5)
			setGhost(8);
		return;
	case 7:
		if (_smoke >= 0 && room.getStep(_smoke) == 5)
			setGhost(0);
		return;
	case 8:
		// Hidden: after its countdown, once its smoke is gone, elsewhere.
		if (_ghostCountdown == 0) {
			if (_smoke < 0)
				setGhost(7);
		} else {
			_ghostCountdown--;
		}
		return;
	default:
		return;
	}
}

bool PieNoonRoom::ghostHitTest(float px, float py) {
	// 0x41A9D0: shown, and on its pixels.
	return !_ghostHidden && _engine.getRoom()->hitsPoint(_ghost, (int)px, (int)py);
}

// ---------------------------------------------------------------------------
// The pies

float PieNoonRoom::pieY(float z) const {
	// A pie flies six tenths of the stage's width above its depth.
	return (float)((double)z - (double)(float)(rightInner(z) - (double)(float)leftInner(z)) * 0.6f);
}

int PieNoonRoom::throwPie(float x, float y) {
	// 0x41CA00 -> 0x41B0F0: in the first free slot of ten, under the first
	// layer. Thrown from the back half (the ghost's), PIEG_THROW, across the
	// ghost's row; else PIETHROW, across Scooby's walk.
	Room &room = *_engine.getRoom();
	for (uint i = 0; i < kPies; i++) {
		if (_pieAlive[i])
			continue;
		Pie &p = _pies[i];
		p = Pie();
		p.sprite = room.addEngineSprite(layer0(), "OBJ_PIE_NOON_PIE", "ANIM_PIE_NOON_PIETHROW", 0, 0, 0, 1, "pie noon pie");
		room.setPlayer(p.sprite, kStopped, 0);
		if (y <= (_ghostY + _scoobyY) * 0.5f) {
			p.ghost = true;
			room.startAnim(p.sprite, "ANIM_PIE_NOON_PIEG_THROW", kStopped, 0);
			p.u = (x - _ghostX[0]) / (_ghostX[3] - _ghostX[0]);
		} else {
			p.u = (x - _scoobyMin) / (_scoobyMax - _scoobyMin);
		}
		p.z = y;
		p.step = 0;
		room.setStep(p.sprite, 0);
		const double lo = leftOuter(p.z);
		p.x = (float)(((double)(float)rightOuter(p.z) - lo) * p.u + (double)(float)lo);
		p.y = pieY(p.z);
		p.size = (float)room.getStepCelWidth(p.sprite, 0);
		placeSprite(p.sprite, p.x, p.y);
		_pieAlive[i] = true;
		return i;
	}
	return -1;
}

bool PieNoonRoom::updatePie(Pie &p) {
	// 0x41B2C0, from the room's tick. False: delete it.
	Room &room = *_engine.getRoom();
	if (p.state != 0) {
		if (p.state == 1) {
			if (room.getMode(p.sprite) == 0)
				return false;
			// Splattered on a runner: its HIT kept up.
			if (p.runner >= 0 && _runnerAlive[p.runner])
				_runners[p.runner].flash = 12;
		}
		return true;
	}
	// Along a tenth of the stage's width at its depth: forward (the ghost's)
	// or back (Scooby's); a step bigger or smaller as it nears or leaves.
	const float w = (float)((double)(float)rightInner(p.z) - leftInner(p.z));
	if (!p.ghost)
		p.z = (float)((double)p.z - (double)w * 0.1f);
	else
		p.z = (float)((double)w * 0.1f + p.z);
	if ((!p.ghost && (double)w * 0.2f < p.size) || (p.ghost && p.size < (double)w * 0.2f)) {
		if (++p.step == 10)
			p.step = 9;
		room.setStepPast(p.sprite, p.step);
		p.size = (float)room.getStepCelWidth(p.sprite, p.step);
	}
	room.setDepth(p.sprite, (int)p.z);
	const double lo = leftOuter(p.z);
	p.x = (float)(((double)(float)rightOuter(p.z) - lo) * p.u + (double)(float)lo);
	p.y = pieY(p.z);
	placeSprite(p.sprite, p.x, p.y);
	if (_scoobyY < p.z) {
		// At the front: Scooby, at five places across its width.
		const double offsets[5] = { 0.0, (double)p.size * 0.25f, -(double)p.size * 0.25f, (double)p.size * 0.5f, -(double)p.size * 0.5f };
		for (uint k = 0; k < 5; k++) {
			if (_scooby >= 0 && scoobyHitTest((double)p.x + offsets[k], p.y)) {
				p.state = 1;
				hitScooby();
				room.startAnim(p.sprite, "ANIM_PIE_NOON_PIEG_HIT", kOnce, -1);
				room.setDepth(p.sprite, room.getDepth(_scooby) - 1);
				return true;
			}
		}
		return false;
	}
	if (p.z < _ghostY) {
		// At the back: the ghost, a snack.
		if (ghostHitTest(p.x, p.y)) {
			setGhost(1);
			addSnack();
			p.state = 1;
			room.startAnim(p.sprite, "ANIM_PIE_NOON_PIEHIT3", kOnce, -1);
			room.setDepth(p.sprite, room.getDepth(_ghost) + 1);
			return true;
		}
		return false;
	}
	// In between: the runners, Shaggy, Fred, Daphne, Velma.
	static const uint kOrder[kRunners] = { kShaggy, kFred, kDaphne, kVelma };
	for (uint k = 0; k < kRunners; k++) {
		const uint which = kOrder[k];
		if (!_runnerAlive[which] || !runnerHitTest(which, p.x, p.y, p.z))
			continue;
		p.state = 1;
		p.runner = which;
		playSound(Common::String::format("Pie_Noon.Splat.%d", g_splat++));
		if (g_splat > 4)
			g_splat = 0;
		static const char *const kHits[3] = { "ANIM_PIE_NOON_PIEHIT1", "ANIM_PIE_NOON_PIEHIT2", "ANIM_PIE_NOON_PIEHIT3" };
		room.startAnim(p.sprite, kHits[_runners[which].lane], kOnce, -1);
		room.setDepth(p.sprite, room.getDepth(_runners[which].sprite) + (p.ghost ? -2 : 2));
		return true;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Scooby

bool PieNoonRoom::scoobyHitTest(double px, double py) {
	// 0x41D160: on his pixels.
	return _engine.getRoom()->hitsPoint(_scooby, (int)px, (int)py);
}

void PieNoonRoom::hitScooby() {
	// 0x41D1B0: his HIT once, and Scooby_Hit.
	_scoobyState = 0;
	_engine.getRoom()->startAnim(_scooby, _scoobyRight ? "ANIM_PIE_NOON_SCOOBYHIT_R" : "ANIM_PIE_NOON_SCOOBYHIT_L", kOnce, -1);
	playSound("Pie_Noon.Scooby_Hit");
}

void PieNoonRoom::updateScooby() {
	// 0x41CE30.
	Room &room = *_engine.getRoom();
	if (!_engine.isMouseDown())
		_latch = false;
	float mx = (float)(uint)(int)(int16)_engine.getMousePos().x;
	if (_scoobyMax < mx)
		mx = _scoobyMax;
	if (mx < _scoobyMin)
		mx = _scoobyMin;
	switch (_scoobyState) {
	case 0:
		// Hit, then licking the pie off.
		if (room.getMode(_scooby) == 0) {
			_scoobyState = 1;
			room.startAnim(_scooby, _scoobyRight ? "ANIM_PIE_NOON_SCOOBYLICK_R" : "ANIM_PIE_NOON_SCOOBYLICK_L", kOnce, -1);
			playSound("Pie_Noon.Scooby_Lick");
		}
		return;
	case 1:
		if (room.getMode(_scooby) != 0)
			return;
		_scoobyState = 3;
		room.startAnim(_scooby, _scoobyRight ? "ANIM_PIE_NOON_SCOOBYWALK_RIGHT_ARM" : "ANIM_PIE_NOON_SCOOBYWALK_LEFT_ARM", kStopped, -1);
		return;
	case 2:
		// Thrown: the pie in the other hand.
		if (room.getMode(_scooby) != 0)
			return;
		_scoobyState = 3;
		_scoobyRight = !_scoobyRight;
		room.startAnim(_scooby, _scoobyRight ? "ANIM_PIE_NOON_SCOOBYWALK_RIGHT_ARM" : "ANIM_PIE_NOON_SCOOBYWALK_LEFT_ARM", kStopped, -1);
		return;
	case 3:
		// After the mouse, 15 a tick at most, his walk stepping a tick a
		// step; a press (once per press) throws.
		if (mx == _scoobyX) {
			room.setPlayer(_scooby, kStopped, -1);
		} else {
			float d = mx - _scoobyX;
			if (15.0 <= ABS(mx - _scoobyX))
				d = _scoobyX < mx ? 15.0f : -15.0f;
			_scoobyX = d + _scoobyX;
			placeSprite(_scooby, _scoobyX, _scoobyY);
			if (room.getMode(_scooby) == 0)
				room.setPlayer(_scooby, _scoobyRight ? kLoopBack : kLoop, room.getStep(_scooby), 1);
		}
		if (!_latch && _engine.isMouseDown() && mayThrow()) {
			throwPie(_scoobyX, _scoobyY);
			_latch = true;
			_scoobyState = 2;
			room.startAnim(_scooby, _scoobyRight ? "ANIM_PIE_NOON_SCOOBYTHROW_R_TO_L" : "ANIM_PIE_NOON_SCOOBYTHROW_L_TO_R", kOnce, -1);
			if (++_throws == 4)
				_throws = 1;
			playSound(Common::String::format("Pie_Noon.Scooby_Throw.%i", _throws));
		}
		return;
	default:
		return;
	}
}

// ---------------------------------------------------------------------------
// The room

void PieNoonRoom::actorUpdate(uint sprite) {
	if ((int)sprite == _scooby) {
		updateScooby();
	} else if ((int)sprite == _ghost) {
		updateGhost();
	} else {
		for (uint i = 0; i < kRunners; i++) {
			if (_runners[i].sprite == (int)sprite && _runnerAlive[i]) {
				updateRunner(i);
				return;
			}
		}
	}
}

void PieNoonRoom::tick() {
	// 0x41BF10, after the base tick.
	Room &room = *_engine.getRoom();
	// The gang not on stage, Velma, Fred, Daphne, Shaggy, may run on.
	static const uint kSpawn[kRunners] = { kVelma, kFred, kDaphne, kShaggy };
	for (uint k = 0; k < kRunners; k++) {
		if (!_runnerAlive[kSpawn[k]])
			addRunner(kSpawn[k]);
	}
	// 0x41A4B0: each runner's hit countdown, Shaggy, Daphne, Fred, Velma.
	static const uint kFlash[kRunners] = { kShaggy, kDaphne, kFred, kVelma };
	for (uint k = 0; k < kRunners; k++) {
		Runner &r = _runners[kFlash[k]];
		if (_runnerAlive[kFlash[k]] && r.flash != 0)
			r.flash--;
	}
	// The pies; those done deleted.
	for (uint i = 0; i < kPies; i++) {
		if (_pieAlive[i] && !updatePie(_pies[i])) {
			room.detachSprite(_pies[i].sprite);
			_pieAlive[i] = false;
		}
	}
	// Each lane's time since its last runner; past 100 ticks, either way.
	for (uint l = 0; l < kLanes; l++) {
		const int v = _laneTimer[l]++;
		if (v > 100)
			_laneState[l] = 2;
	}
	// The clock: once the first lane's wait is over, a step every 36 ticks
	// from step 1; back on step 0, stopped, and the chime.
	if (_clock >= 0) {
		if (_intro) {
			if (_laneTimer[0] >= 0) {
				room.setPlayer(_clock, kLoop, 1, 36);
				_intro = false;
			}
		} else if (room.getStep(_clock) == 0 && !_chime) {
			room.setPlayer(_clock, kStopped, -1);
			playSound("Pie_Noon.Clock_Chime");
			_chime = true;
		}
		// Each step a tick or a tock.
		if (room.getStep(_clock) != _lastStep) {
			playSound(_tock ? "Pie_Noon.Clock_Tick" : "Pie_Noon.Clock_Tock");
			_tock = !_tock;
			_lastStep = room.getStep(_clock);
		}
	}
	// The chime over: the saloon.
	if (_chime && !_engine.getSound()->isClipPlaying("Pie_Noon.Clock_Chime"))
		_engine.requestRoom("ROOM_P12_Saloon");
}

RoomClass *createPieNoonRoom(ScoobyEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kPieNoon)
		return new PieNoonRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
