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

#ifndef TERRAGLYPH_ARMORY_ROOMS_H
#define TERRAGLYPH_ARMORY_ROOMS_H

#include "terraglyph/rooms/room_class.h"

namespace Terraglyph {

/**
 * Phantom's king's armory, ROOM_P20_Kings_Armory and its slide,
 * ROOM_P20_Kings_Armory_Slide: one class (0x40EBC0, vtable 0x45E710), an
 * object for each. Scooby fetches the duster. The full bottle on the
 * armour or the scratch plays movie 7 into the slide room, and the armory
 * leads there from then on. The intro plays once, or while Jane screams,
 * a scream and one of two intros each visit.
 */
class KingsArmoryRoom : public RoomClass {
public:
	KingsArmoryRoom(TerraglyphEngine &engine, const Common::String &name) : RoomClass(engine), _name(name) {}

	void reset() override;                                                         // 0x40EC20
	void enter(const Common::String &from) override;                               // 0x40EC40
	void roomLeft() override;                                                      // 0x40F150
	void tick() override;                                                          // 0x40EFE0
	bool isBusy() const override { return _duster; }                               // game +0x24
	void event(const Common::String &object, bool success) override;              // 0x40EDD0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x40EF10
	void saveKeys(Common::INIFile &profile) override;                              // 0x40F1A0
	void loadKeys(const Common::INIFile &profile) override;                        // 0x40F1E0

	bool isIntroDone() const { return _introDone; }
	void setIntroDone(bool done) { _introDone = done; }
	bool isTakingDuster() const { return _duster; }
	const Common::String &getScream() const { return _scream; }

private:
	Common::String _name;      // +0x08: the room this object is for
	int _duster1 = -1;         // +0xB8: OBJ_INV_DUSTER
	int _scratchLine = 0;      // +0xC0
	bool _duster = false;      // +0xC4: Scooby to take the duster
	bool _taking = false;      // +0xC5
	bool _introDone = false;   // +0xC6, saved as IntroDone
	Common::String _scream;
};

/** The class for this room, or nullptr. */
RoomClass *createArmoryRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_ARMORY_ROOMS_H
