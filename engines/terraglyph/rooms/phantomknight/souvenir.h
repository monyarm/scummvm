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

#ifndef TERRAGLYPH_SOUVENIR_H
#define TERRAGLYPH_SOUVENIR_H

#include "terraglyph/rooms/class.h"

namespace Terraglyph {

/**
 * Phantom's souvenir shop, ROOM_P03_Souvenir_Shop (0x416FE0, vtable
 * 0x45EB9C): the tokens buy the empty bottle, which Fred fetches into the
 * toolbar. Its opening is the shop's introduction once, or while Jane is
 * screaming, a scream and the gang's reply.
 */
class SouvenirShopRoom : public RoomClass {
public:
	explicit SouvenirShopRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override { _introDone = false; }                                // 0x417040
	void enter(const Common::String &from) override;                               // 0x4172D0
	void roomLeft() override;                                                      // 0x417580
	void tick() override;                                                          // 0x417400
	bool isBusy() const override { return _bought; }                               // game +0x24
	void event(const Common::String &object, bool success) override;              // 0x417060
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x417170
	void saveKeys(Common::INIFile &profile) override;                              // 0x4175D0
	void loadKeys(const Common::INIFile &profile) override;                        // 0x417610

	int getBottle() const { return _bottle; }
	int getHotspot() const { return _hotspot; }
	bool isBought() const { return _bought; }
	bool isFetching() const { return _fetching; }
	bool isIntroDone() const { return _introDone; }
	void setIntroDone(bool done) { _introDone = done; }
	const Common::String &getScream() const { return _scream; }

private:
	int findSprite(const Common::String &object) const;
	void addBottle();

	int _bottle = -1;        // +0xB8: OBJ_INV_BOTTLE_EMPTY, the requires object
	int _hotspot = -1;       // +0xBC: OBJ_P03_SOUVENIR_STORE_HOTSPOT1
	bool _bought = false;    // +0xC0: the tokens given, the bottle not yet in the toolbar
	bool _fetching = false;  // +0xC1: Fred is fetching it
	bool _introDone = false; // +0xC2, saved as IntroDone
	Common::String _scream;
};

/** The class for this room, or nullptr. */
RoomClass *createSouvenirRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_SOUVENIR_H
