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

#ifndef SCOOBY_STAGE_ROOMS_H
#define SCOOBY_STAGE_ROOMS_H

#include "scooby/room_class.h"
#include "scooby/room_script.h"

namespace Scooby {

/**
 * Showdown's stage, ROOM_P05_Stage (0x417C50, vtable 0x45EA54): four
 * sandbags hang over it, one per BagDown flag, and Jenny is shut under it
 * until the gang opens the trapdoor. Back on the stage she gives the hammer
 * and her clue, then the gang thanks her and goes to the suspects and clues.
 */
class StageRoom : public RoomClass {
public:
	explicit StageRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x417CB0
	void enter(const Common::String &from) override;                                // 0x417F10
	void tick() override;                                                           // 0x417CD0
	bool click(int sprite) override;                                                // 0x417CD0, Jenny clicked
	bool isBusy() const override { return _thanks; }                                // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x4181B0

	int getSandbag(uint i) const { return _sandbags[i]; }
	bool isGiving() const { return _gives; }
	const Common::String &getLastScene() const { return _lastScene; }

private:
	bool _gives = false;       // +0xB8: Jenny gives when idle
	bool _thanks = false;      // +0xB9: then the thanks and the suspects
	int _sandbags[4] = { -1, -1, -1, -1 }; // +0xBC: the sandbags, by their digit
	bool _jennyClicked = false;
	Common::String _lastScene;
};

/**
 * Showdown's room under the stage, ROOM_P06_Stage_Trapdoor (0x418430,
 * vtable 0x45EA94): Jenny knocks and yells under the trapdoor. Four levers
 * raise and drop the four sandbags that weigh it down; which sandbags a
 * lever moves depends on its alias (LeverAlias0-3) and the level. With all
 * four raised the trapdoor opens, Jenny is met, and the gang comes out on
 * the stage through movie 4.
 */
class StageTrapdoorRoom : public RoomClass {
public:
	explicit StageTrapdoorRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x418510
	void prepare() override;                                                        // 0x418490 (+0x30)
	void enter(const Common::String &from) override;                                // 0x418870
	void tick() override;                                                           // 0x418540
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x418A10
	void clicked(uint sprite) override;                                             // 0x418F50, a lever's callback
	bool leaveBy(const Common::String &exitObject) override;                        // 0x4188B0 (+0x24)

	/** 0x418D50: raise sandbag @p i if it is down, drop it if it is up, as @p drop says. */
	void moveSandbag(uint i, bool drop);

	int getLever(uint i) const { return _levers[i]; }
	int getSandbag(uint i) const { return _sandbags[i]; }
	int getDoor() const { return _door; }
	int getExit() const { return _exit; }
	bool isDoorOpening() const { return _doorOpening; }
	int getKnockCountdown() const { return _knock; }
	const Common::String &getLastSound() const { return _lastSound; }
	const RoomScript::Exit &getLastExit() const { return _lastExit; }

private:
	void leave(RoomScript::Exit exit);

	int _levers[4] = { -1, -1, -1, -1 };   // +0xB8: the levers, by their digit
	int _sandbags[4] = { -1, -1, -1, -1 }; // +0xC8: the sandbags, by their digit
	int _door = -1;                        // +0xD8
	bool _doorOpening = false;             // +0xDC
	int _knock = -10;                      // +0xE0: ticks to the next knock or yell
	int _visits = 0;                       // +0xE4: which opening, levels 1 and 2
	int _exit = -1;                        // +0xE8: the way out
	bool _leaving = false;
	Common::String _lastSound;
	RoomScript::Exit _lastExit;
};

/** The stage's room class the factory builds for @p room, or nullptr. */
RoomClass *createStageRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_STAGE_ROOMS_H
