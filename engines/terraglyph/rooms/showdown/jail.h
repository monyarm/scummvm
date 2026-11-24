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

#ifndef TERRAGLYPH_JAIL_ROOMS_H
#define TERRAGLYPH_JAIL_ROOMS_H

#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"

namespace Terraglyph {

/**
 * Showdown's jail cell, ROOM_P07_Jail (0x412E60, vtable 0x45E910): Shaggy
 * and Scooby are locked in. Shaggy takes the blanket off the bunk; Scooby
 * breaks the bed, which uncovers a hole under it, and the two drop through
 * into the dark tunnel.
 */
class JailRoom : public RoomClass {
public:
	explicit JailRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x412EC0
	void enter(const Common::String &from) override;                                // 0x413600
	void roomLeft() override;                                                       // 0x413310
	void tick() override;                                                           // 0x412EF0
	void event(const Common::String &object, bool success) override;                // 0x413470
	bool leaveBy(const Common::String &exitObject) override;                        // 0x413370 (+0x24)
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x413690
	void saveKeys(Common::INIFile &profile) override;                               // 0x4138B0
	void loadKeys(const Common::INIFile &profile) override;                         // 0x4138F0
	// Game +0x24, set on every tick while Shaggy takes the blanket, Scooby
	// breaks the bed, or the two drop through the hole.
	bool isBusy() const override { return _blanketClicked || _bedClicked || _holeDone; }

	int getBlanket() const { return _blanket; }
	int getBed() const { return _bed; }
	int getHotspot4() const { return _hotspot4; }
	int getHole() const { return _hole; }
	int getStool() const { return _stool; }
	bool isBedBroken() const { return _bedBroken; }
	bool isHoleDone() const { return _holeDone; }
	bool isLeaving() const { return _leaving; }
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	void playSound(const char *name);

	int _hotspot3 = 0;            // +0xB8: hotspot 3's next line
	bool _leaving = false;        // +0xBC: Shaggy leaves by the exit handler
	bool _blanketClicked = false; // +0xBD
	bool _takingBlanket = false;  // +0xBE
	int _blanket = -1;            // +0xC0
	int _bed = -1;                // +0xC4: hotspot 2
	int _hotspot4 = -1;           // +0xC8
	bool _bedBroken = false;      // +0xCC, saved as BedBroken
	bool _bedClicked = false;     // +0xCD
	bool _breakingBed = false;    // +0xCE
	bool _holeDone = false;       // +0xCF: 150D_'s done flag
	bool _falling = false;        // +0xD0
	int _hole = -1;               // +0xD4
	int _stool = -1;              // +0xD8
	bool _holeScene = false;      // 150D_ started, its done flag not yet set
	Common::String _lastScene;
	Common::String _lastSound;
};

/** The jail's room class the factory builds for @p room, or nullptr. */
RoomClass *createJailRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_JAIL_ROOMS_H
