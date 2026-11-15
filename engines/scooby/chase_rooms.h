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

#ifndef SCOOBY_CHASE_ROOMS_H
#define SCOOBY_CHASE_ROOMS_H

#include "scooby/room_class.h"

namespace Scooby {

/**
 * Showdown's chase branches, ROOM_P24_Chase_B1 to ROOM_P29_Chase_B6
 * (0x41E6B0, vtable 0x45EDA8): the gang runs to a fork in the path. An exit
 * clicked, or the room's countdown running out, plays the branch's run-off
 * on the OBJ_BRANCH sprite, and the room changes when it has played out.
 * B1 and B4 have no choice: they run off as they are entered.
 */
class ChaseBRoom : public RoomClass {
public:
	explicit ChaseBRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x41E780
	void arrive(const Common::String &from) override;                               // 0x41FA80 (+0x2C)
	void enter(const Common::String &from) override;                                // 0x41F650
	void tick() override;                                                           // 0x41EFC0
	bool isBusy() const override { return !_pending.empty(); }                      // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x41ED40
	void event(const Common::String &object, bool success) override;               // 0x41EBC0
	bool leaveBy(const Common::String &exitObject) override;                        // 0x41E7A0

	const Common::String &getPending() const { return _pending; }
	int getCountdown() const { return _countdown; }
	int getBranchSprite() const { return _branch; }
	/** The last sound the class played, for the checks. */
	const Common::String &getLastSound() const { return _lastSound; }

private:
	/** The exit handler's body (0x41E7A0) for @p destination. */
	void leave(const Common::String &destination);

	int _branch = -1;            // +0xB8: the OBJ_BRANCH sprite
	int _countdown = 720;        // +0xBC: ticks until the gang runs on by itself
	Common::String _pending;     // +0xC0: the room the run-off leads to
	bool _oilCan = false;        // +0xC8: the oil can was taken here
	int _oilCanSprite = -1;      // +0xCC: OBJ_INV_OILCAN, as a requires object
	Common::String _lastSound;
};

/**
 * Showdown's chase run-offs, ROOM_P33_Chase_C1 to ROOM_P39_Chase_C4
 * (0x41FBB0, vtable 0x45EDE8): a canned animation with its sound, no input,
 * and then the next branch.
 */
class ChaseCRoom : public RoomClass {
public:
	explicit ChaseCRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x420510
	void tick() override;                                                           // 0x41FC10
	bool isBusy() const override { return true; }                                   // game +0x24, every tick
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x420690

	int getCannedSprite() const { return _canned; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	int _canned = -1;      // +0xB8: the OBJ_CANNED sprite
	bool _leaving = false; // the exit was taken (the original takes it again every tick)
	Common::String _lastSound;
};

/**
 * Showdown's close-up, ROOM_P25A_Chase_B2_Closeup (0x4119F0, vtable
 * 0x45E890): ICDouble digging. The first visit meets him on Big Head and
 * goes on to the artifact; with the artifact built he gives his clue; with
 * his clue given, the safe's combination blows in, and Shaggy takes it.
 */
class CloseupRoom : public RoomClass {
public:
	explicit CloseupRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x411A50
	void enter(const Common::String &from) override;                                // 0x412020
	void roomLeft() override;                                                       // 0x411A70
	void tick() override;                                                           // 0x411AC0
	bool click(int sprite) override;                                                // 0x411AC0's click
	bool isBusy() const override { return _held; }                                  // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x412300
	void event(const Common::String &object, bool success) override;               // 0x411FC0
	void saveKeys(Common::INIFile &profile) override;                               // 0x412410
	void loadKeys(const Common::INIFile &profile) override;                         // 0x412450

	/** The artifact's room for this game: ROOM_Artifact_<type>_<09, 12 or 15> (0x412490). */
	Common::String getArtifactRoom() const;
	int getComboSprite() const { return _combo; }
	bool hasComboBlownIn() const { return _blownIn; }
	/** The last scene the class started, for the checks. */
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	bool _gives = false;        // +0xB8: ICDouble gives when next idle
	bool _thanks = false;       // +0xB9: then the gang thanks him and leaves
	int _combo = -1;            // +0xBC: OBJ_INV_SAFE_COMBO, as a requires object
	bool _comboSceneDone = false; // +0xC0: Chase.Branch_2.486_ is over
	bool _taking = false;       // +0xC1: Shaggy is taking the combination
	bool _blownIn = false;      // +0xC2: ComboBlewOn
	bool _owner = false;        // +0xC3: ICDouble had given his clue on entry
	int _countdown = 24;        // +0xC4: ticks before the combination blows in
	bool _comboScene = false;   // Chase.Branch_2.486_ is playing
	bool _held = false;         // game +0x24, as the class sets and clears it
	Common::String _lastScene;
	Common::String _lastSound;
};

/** The chase room class the factory builds for @p room, or nullptr. */
RoomClass *createChaseRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_CHASE_ROOMS_H
