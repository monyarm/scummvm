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

#ifndef SCOOBY_DOORS_ROOMS_H
#define SCOOBY_DOORS_ROOMS_H

#include "scooby/room_class.h"

namespace Scooby {

class Character;

/**
 * Phantom's Jester, as a room class keeps him (0x4122D0 constructor,
 * 0x412B80 on entry, 0x4122E0 each tick): he takes the balls found, gives
 * his clue once all three are, talks the gang through the crest's order,
 * and answers a click with his introduction or a line.
 */
class JesterControl {
public:
	/** 0x412B80: the room's Jester, and the balls he takes. */
	void enter(ScoobyEngine &engine);
	/** 0x4122E0, after the room's tick. */
	void tick(ScoobyEngine &engine);
	/** The sprite clicked since the last tick (game +0x20 bit 4). */
	void click(int sprite) { _clicked = sprite; }
	/** Input held and the cursor hidden for the clue and the thanks. */
	bool isHolding() const { return _giving || _thanks; }

	bool introDone = true; // [0], the room's JesterIntro
	bool isTalking() const { return _talk; }
	int getCount() const { return _count; }

private:
	void startScene(ScoobyEngine &engine, const Common::String &scene, bool *done);
	void say(ScoobyEngine &engine);

	Character *_jester = nullptr; // [4]
	bool _giving = false;   // [1]: the clue given, he gives when idle
	bool _thanks = false;   // [2]: given; the thanks when he is idle again
	bool _talk = false;     // [8]: talking the gang through the crest
	bool _d = false;        // [9]: the next 220D<digit>_ (done flag)
	bool _next = false;     // [10]: 220E_ next
	bool _e = false;        // [11]: the next 220E<digit>_ (done flag)
	bool _e5 = false;       // [12]: 220E5_ next
	bool _e6 = false;       // [13]: 220E6_ next (220E5_'s done flag)
	bool _end = false;      // [14]: the talk over (220E6_'s done flag)
	int _count = 0;         // [0x10]
	int _clicked = -1;
	Common::String _scene;  // the scene whose done flag is pending
	bool *_doneFlag = nullptr;
};

/**
 * Phantom's room of doors, ROOM_P11_Room_Of_Doors (0x415720, vtable
 * 0x45EA9C): the hub of the castle's ground floor. The dining room's door
 * is locked until the duster has been used; the way to the throne room
 * stops Jane's crying; each way has its lines; the Jester stands here once
 * the duster is used.
 */
class RoomOfDoorsRoom : public RoomClass {
public:
	explicit RoomOfDoorsRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x4157B0
	void enter(const Common::String &from) override;                               // 0x4157F0
	void roomLeft() override;                                                      // 0x4157D0
	void tick() override;                                                          // 0x415AB0
	void idle() override;                                                          // 0x4159D0
	bool click(int sprite) override;
	bool isBusy() const override { return _locked || _jester.isHolding(); }      // game +0x24
	bool leaveBy(const Common::String &exitObject) override;                       // 0x415C00
	void leaving(const Common::String &destination, Common::String &scene) override; // 0x415C00
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x415F80
	void saveKeys(Common::INIFile &profile) override;                              // 0x416140
	void loadKeys(const Common::INIFile &profile) override;                        // 0x4161C0

	bool isCrying() const { return _crying; }
	bool isThroneScene() const { return _throneScene; }
	bool isJesterIntro() const { return _jesterIntro; }
	void setJesterIntroForCheck(bool done) { _jesterIntro = done; }
	bool isLocked() const { return _locked; }
	JesterControl &getJester() { return _jester; }
	const Common::String &getScream() const { return _scream; }
	/** The duster used: ever taken (0x407060) and no longer in a slot (0x4070C0). */
	static bool isDusterUsed(ScoobyEngine &engine);

private:
	bool _locked = false;      // +0xB8: the dining room's door tried
	bool _velma = false;       // +0xB9: Velma tries it
	bool _crying = true;       // +0xBA, saved as Crying
	bool _throneScene = false; // +0xBB, saved as ThroneScene
	JesterControl _jester;     // +0xBC
	bool _jesterIntro = false; // +0xD0, saved as JesterIntro
	Common::String _scream;
};

/** The class for this room, or nullptr. */
RoomClass *createDoorsRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_DOORS_ROOMS_H
