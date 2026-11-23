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

#ifndef TERRAGLYPH_PASSAGE_ROOMS_H
#define TERRAGLYPH_PASSAGE_ROOMS_H

#include "terraglyph/room_class.h"

namespace Terraglyph {

/**
 * Phantom's first secret passage, ROOM_P15_Secret_Passage1 (0x416C40,
 * vtable 0x45EB1C), between the armory's slide and the storage room: its
 * walk-ins.
 */
class SecretPassage1Room : public RoomClass {
public:
	explicit SecretPassage1Room(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override; // 0x416CA0
};

/**
 * Phantom's second secret passage, ROOM_P19_Secret_Passage2 (0x416D10,
 * vtable 0x45EB5C): the gargoyle's wall of fire before the treasury. Fed
 * the level's snacks, a click puts it out through movie 39; until then,
 * a line and the snack box.
 */
class SecretPassage2Room : public RoomClass {
public:
	explicit SecretPassage2Room(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x416D70
	void enter(const Common::String &from) override;                               // 0x416F30
	void event(const Common::String &object, bool success) override;              // 0x416E90
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x416D90
	void saveKeys(Common::INIFile &profile) override;                              // 0x416F60
	void loadKeys(const Common::INIFile &profile) override;                        // 0x416FA0

	bool isFlameActive() const { return _flame; }
	void setFlameActive(bool on) { _flame = on; }

private:
	bool _flame = true; // +0xB8: GargoyleFlameActive
};

/**
 * Phantom's storage room, ROOM_P18_Storage (0x417650, vtable 0x45EBDC): two
 * hotspots' lines, and the rope, which Velma takes after a line.
 */
class StorageRoomP18 : public RoomClass {
public:
	explicit StorageRoomP18(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x4176B0
	void enter(const Common::String &from) override;                               // 0x417950
	void roomLeft() override;                                                      // 0x417970
	void tick() override;                                                          // 0x4177C0
	void event(const Common::String &object, bool success) override;              // 0x4176D0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x4179C0
	bool isBusy() const override { return _take; }                                 // game +0x24
	bool hidesCursor() const override { return _take; }                            // 0x435E80 on the cursor

	int getHotspotLine() const { return _hotspotLine; }
	bool isTakePending() const { return _take; }
	bool isTaking() const { return _taking; }
	int getRope() const { return _rope; }

private:
	void startScene(const Common::String &scene, bool block, bool *done);

	int _hotspotLine = 0;      // +0xB8
	bool _take = false;        // +0xBC: the rope's line over, Velma to take it
	bool _taking = false;      // +0xBD
	int _rope = -1;            // +0xC0
	bool *_doneFlag = nullptr; // the scene's done flag (0x40B400)
	Common::String _doneScene;
};

/**
 * Phantom's treasury, ROOM_P16_Treasury (0x4182E0, vtable 0x45EC9C): the
 * Princess clicked with the arrow, the final clue, and the accusation.
 */
class TreasuryRoom : public RoomClass {
public:
	explicit TreasuryRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void tick() override;           // 0x418340
	bool click(int sprite) override;

private:
	int _clicked = -1;
};

/** The class for this room, or nullptr. */
RoomClass *createPassageRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_PASSAGE_ROOMS_H
