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

#ifndef TERRAGLYPH_PIENOON_ROOMS_H
#define TERRAGLYPH_PIENOON_ROOMS_H

#include "common/array.h"

#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"

namespace Terraglyph {

/**
 * Showdown's Pie Noon, ROOM_P32_Pie_Noon (0x41B840, vtable 0x45EC3C): a pie
 * fight on the saloon's stage until the clock chimes. Scooby walks along the
 * front after the mouse and throws a pie each press; the ghost walks along
 * the back, throws back, and hides in smoke. The rest of the gang run
 * across three lanes in between. A pie that reaches the ghost is a snack.
 * Then the saloon again.
 *
 * Scooby (0x41CD50, vtable 0x45EC7C), the ghost (0x41A550, 0x45EBDC) and
 * each runner (0x419C50, 0x45EBA0) update themselves in the tree walk; each
 * pie (0x41B0F0, 0x45EC0C) is updated by the room's tick.
 */
class PieNoonRoom : public RoomClass, public Room::Actor {
public:
	explicit PieNoonRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x41B8E0
	void roomLeft() override;                                                       // 0x41C130
	void tick() override;                                                           // 0x41BF10
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x41C190
	void actorUpdate(uint sprite) override;
	// The cursor faded every tick (0x435150 with 0); input stays on.
	bool hidesCursor() const override { return true; }

	enum { kShaggy = 0, kFred = 1, kVelma = 2, kDaphne = 3, kRunners = 4, kPies = 10, kLanes = 3 };

	/** A runner (0x45EBA0): one of the gang crossing a lane. */
	struct Runner {
		int sprite = -1;
		Common::String name;      // +0x68
		int lane = 0;             // +0x74
		bool right = false;       // +0x70: runs to the right
		int state = 0;            // +0x5C: 0 running, 1 hit
		float x = 0, y = 0;       // +0x0C, +0x10
		float speed = 0;          // +0x78
		float target = 0;         // +0x7C
		int flash = 0;            // +0x80
		uint hitTicks = 0;        // +0x84
		bool hitX = false;        // +0x88
	};
	/** A pie (0x45EC0C). */
	struct Pie {
		int sprite = -1;
		int state = 0;            // +0x60: 0 flying, 1 landed
		float u = 0;              // +0x64: across, 0 to 1
		float z = 0;              // +0x68: its depth on the stage, as a y
		float size = 0;           // +0x6C: its step's cel width
		bool ghost = false;       // +0x70: thrown by the ghost
		int runner = -1;          // +0x74: the runner it hit
		uint step = 0;            // +0x4C
		float x = 0, y = 0;
	};

	// The stage's geometry, from the placements (0x41C190) and entry.
	float getScoobyMin() const { return _scoobyMin; }
	float getScoobyMax() const { return _scoobyMax; }
	float getScoobyY() const { return _scoobyY; }
	float getGhostX(uint i) const { return _ghostX[i]; }
	float getGhostY() const { return _ghostY; }
	float getStartX(uint i) const { return _startX[i]; }
	float getStartY(uint i) const { return _startY[i]; }
	float getLaneA(uint l) const { return _laneA[l]; }
	float getLaneB(uint l) const { return _laneB[l]; }
	float getLaneY(uint l) const { return _laneY[l]; }
	int getLaneState(uint l) const { return _laneState[l]; }
	int getLaneTimer(uint l) const { return _laneTimer[l]; }
	float getSlope(uint i) const { return _slope[i]; }
	/** 0x41C980, 0x41C9A0, 0x41C9C0, 0x41C9E0: the stage's edges at depth @p z. */
	double leftInner(float z) const;
	double rightInner(float z) const;
	double leftOuter(float z) const;
	double rightOuter(float z) const;

	int getScooby() const { return _scooby; }
	int getScoobyState() const { return _scoobyState; }
	float getScoobyX() const { return _scoobyX; }
	bool isScoobyRight() const { return _scoobyRight; }
	int getThrows() const { return _throws; }
	int getGhost() const { return _ghost; }
	int getGhostState() const { return _ghostState; }
	float getGhostXNow() const { return _gx; }
	bool isGhostRight() const { return _ghostRight; }
	bool isGhostHidden() const { return _ghostHidden; }
	int getGhostCountdown() const { return _ghostCountdown; }
	int getSmoke() const { return _smoke; }
	int getLookout() const { return _lookout; }
	const Runner *getRunner(uint i) const { return _runners[i].sprite >= 0 && _runnerAlive[i] ? &_runners[i] : nullptr; }
	const Pie *getPie(uint i) const { return _pieAlive[i] ? &_pies[i] : nullptr; }
	int getClock() const { return _clock; }
	bool isIntro() const { return _intro; }
	bool isChiming() const { return _chime; }
	int getOverlay(uint i) const { return _overlays[i]; }
	int getTens() const { return _tens; }
	int getOnes() const { return _ones; }
	const Common::String &getLastSound() const { return _lastSound; }
	const Common::String &getSounds() const { return _sounds; }
	void clearSounds() { _sounds.clear(); }

	/** For a check: a runner as 0x41CC40 makes one, in @p lane, running right or not. */
	void spawnRunner(uint which, uint lane, bool right);
	/** For a check: a pie as 0x41CA00 throws one from (x, y). */
	int throwPie(float x, float y);
	/** For a check: the ghost to @p state (0x41AA40). */
	void setGhost(int state);
	/** 0x41CD20: Scooby may throw (the intro over, no chime). */
	bool mayThrow() const { return _laneTimer[0] >= 0 && !_chime; }
	/** For a check: the splat sound's number (0x475F08), kept for the game. */
	static void setSplat(int n);

private:
	void playSound(const Common::String &name);
	void addRunner(uint which);
	void runRunner(uint which);     // 0x41A310
	void hitRunner(uint which);     // 0x41A110
	bool runnerHitTest(uint which, float px, float py, float pz); // 0x419EB0
	void updateRunner(uint which);  // 0x419DE0
	void updateGhost();             // 0x41A650
	bool ghostHitTest(float px, float py); // 0x41A9D0
	bool updatePie(Pie &pie);       // 0x41B2C0
	void updateScooby();            // 0x41CE30
	void hitScooby();               // 0x41D1B0
	bool scoobyHitTest(double px, double py); // 0x41D160
	float pieY(float z) const;
	void addSnack();                // 0x41CB50
	void showCounter();
	void placeSprite(int sprite, float x, float y);
	int layer0() const;

	// The placements' places (0x41C190).
	float _scoobyMax = -10000, _scoobyMin = 10000; // +0xC8, +0xCC
	float _scoobyY = 0;              // +0xD0
	float _ghostX[4] = {};           // +0xD4, sorted
	float _ghostY = 0;               // +0xE4
	uint _ghostCount = 0;            // +0xE8
	float _startX[5] = {}, _startY[5] = {}; // +0xEC, +0x100: sorted by y
	uint _startCount = 0;            // +0x114
	float _slope[4] = {};            // +0xB8-+0xC4
	float _laneB[3] = {}, _laneA[3] = {}, _laneY[3] = {}; // +0x118, +0x124, +0x130
	int _laneState[3] = {};          // +0x13C: 0 left, 1 right, 2 either
	int _laneTimer[3] = {};          // +0x148
	Pie _pies[kPies];                // +0x154
	bool _pieAlive[kPies] = {};
	Runner _runners[kRunners];       // +0x184 Shaggy, +0x188 Fred, +0x18C Velma, +0x190 Daphne
	bool _runnerAlive[kRunners] = {};
	int _tens = -1, _ones = -1;      // +0x194, +0x198
	int _numbers = 0;
	int _overlays[4] = { -1, -1, -1, -1 }; // +0x19C FR, FL, BR, BL
	bool _intro = false;             // +0x1AC
	int _clock = -1;                 // +0x1B0
	bool _chime = false;             // +0x1B4
	bool _tock = false;              // +0x1B8: the next is a tick when set
	uint _lastStep = 0;              // +0x1C4
	// Scooby (+0x17C).
	int _scooby = -1;
	float _scoobyX = 0;
	bool _latch = false;             // +0x60
	int _scoobyState = 3;            // +0x64
	bool _scoobyRight = false;       // +0x68
	int _throws = 1;                 // +0x6C
	// The ghost (+0x180).
	int _ghost = -1;
	float _gx = 0, _gy = 0;
	float _gspeed = 0;               // +0x5C
	int _ghostCountdown = 0;         // +0x60
	int _ghostState = 8;             // +0x68
	bool _ghostRight = true;         // +0x6C
	bool _ghostHidden = false;       // +0x70
	int _smoke = -1;                 // +0x74
	int _lookout = 0;                // +0x78
	int _goodJob = 0;                // +0x7C
	Common::String _lastSound;
	Common::String _sounds;
};

/** Pie Noon's room class the factory builds for @p room, or nullptr. */
RoomClass *createPieNoonRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_PIENOON_ROOMS_H
