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

#ifndef TERRAGLYPH_STORY_ROOMS_H
#define TERRAGLYPH_STORY_ROOMS_H

#include "terraglyph/rooms/room_class.h"

namespace Terraglyph {

/**
 * Showdown's Town Center (0x42CFD0, vtable 0x45F300): the game's first
 * room. The gang won't leave until Scooby has had a snack here, and the
 * hanging sign comes down for Scooby to take.
 */
class TownCenterRoom : public RoomClass {
public:
	explicit TownCenterRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x42D030
	void enter(const Common::String &from) override;                               // 0x42D270
	void tick() override;                                                          // 0x42D3F0
	bool isBusy() const override { return _takeSign; }                             // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42D800
	void event(const Common::String &object, bool success) override;              // 0x42D7B0
	bool leaveBy(const Common::String &exitObject) override;                       // 0x42D060
	void idle() override;                                                          // 0x42D340
	void saveKeys(Common::INIFile &profile) override;                              // 0x42D970
	void loadKeys(const Common::INIFile &profile) override;                        // 0x42D9B0

	bool isSignClicked() const { return _signClicked; }

private:
	bool isMoaning() const;
	int findSprite(const Common::String &object) const;

	int _exitScene = 0;     // +0xB8: which of the three exit scenes is next
	int _moanDelay = 0;     // +0xBC
	bool _signClicked = false; // +0xC0, saved as SignClicked
	int _hangingSign = -1;  // +0xC4: OBJ_P01_TC_HOTSPOT4
	int _sign = -1;         // +0xC8: OBJ_INV_SIGN, as a requires object
	int _hotspot2 = -1;     // +0xCC: OBJ_P01_TC_HOTSPOT2
	bool _takeSign = false; // +0xD0: the sign was clicked; input is held
	bool _taking = false;   // +0xD1: Scooby is taking it
	int _takes = -1;        // +0xD4: OBJ_SCOOBY_P01_TAKES_SIGN
};

/**
 * Showdown's Street 1 (0x42AE70, vtable 0x45F23C): the Sheriff, who
 * introduces himself, takes the sign and gives the gang his clue; the
 * pump and its trough, which bring up the strong box key; the brick,
 * washed in the trough for the final clue.
 */
class Street1Room : public RoomClass {
public:
	explicit Street1Room(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x42AED0
	void prepare() override;                                                       // 0x42BCD0
	void enter(const Common::String &from) override;                               // 0x42B960
	void tick() override;                                                          // 0x42AF60
	void roomLeft() override;                                                      // 0x42AF10
	bool click(int sprite) override;                                               // 0x42AF60, the Sheriff
	bool isBusy() const override { return _giving || _pumping || _takeKey; }        // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42BD10
	void event(const Common::String &object, bool success) override;              // 0x42C120
	void leaving(const Common::String &destination, Common::String &scene) override; // 0x42BB50
	void saveKeys(Common::INIFile &profile) override;                              // 0x42C2E0
	void loadKeys(const Common::INIFile &profile) override;                        // 0x42C360

	int getPumpAttempts() const { return _pumpAttempts; }
	bool isKeyVisible() const { return _keyVisible; }
	bool isIntroPlayed() const { return _introPlayed; }
	bool isPumping() const { return _pumping; }

private:
	void sheriffGivesClue();

	bool _sceneDone = false;     // +0xBC: Street_1.220D1_ is over
	bool _sheriffGives = false;  // +0xBD: the Sheriff will give, when idle
	bool _giving = false;        // +0xBE: he is giving; then the gang leaves
	int _pumpAttempts = 0;       // +0xC0, saved as PumpAttempts
	bool _brickWashed = false;   // +0xC4
	bool _introPlayed = false;   // +0xC5, saved as SheriffIntroPlayed
	int _hint = 0;               // +0xCC: which of the three hints is next
	bool _pumping = false;       // +0xD0
	bool _pumpStarted = false;   // +0xD1
	bool _takeKey = false;       // +0xD2
	bool _taking = false;        // +0xD3
	bool _keyVisible = false;    // +0xD4, saved as KeyVisible
	bool _waitScene = false;     // Street_1.220D1_ is playing
	int _hotspot2 = -1;          // +0xB8
	int _key = -1;               // +0xD8: OBJ_INV_STRONGBOXKEY
	int _trough = -1;            // +0xDC: OBJ_P03_STREET1_TROUGH
};

/**
 * Showdown's Street 2 (0x42C3E0, vtable 0x45F27C): Gummy and his donkey,
 * once Artie is met; the clock; Gummy's window.
 */
class Street2Room : public RoomClass {
public:
	explicit Street2Room(TerraglyphEngine &engine) : RoomClass(engine) {}

	void prepare() override;                                                       // 0x42CB40
	void enter(const Common::String &from) override;                               // 0x42C440
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42C7A0
	void event(const Common::String &object, bool success) override;              // 0x42CAE0
	void leaving(const Common::String &destination, Common::String &scene) override; // 0x42C600
};

/**
 * Showdown's Street 3 (0x42CB80, vtable 0x45F2C0): moans, a hotspot that
 * cuts the music for its own, and another with its scene.
 */
class Street3Room : public RoomClass {
public:
	explicit Street3Room(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x42CBE0
	void enter(const Common::String &from) override;                               // 0x42CE50
	void tick() override;                                                          // 0x42CD10
	void event(const Common::String &object, bool success) override;              // 0x42CC10
	void idle() override;                                                          // 0x42CCC0

private:
	int _moanDelay = 0;      // +0xB8
	bool _musicBack = true;  // +0xBC: the music group comes back when hotspot 2 is still
	int _hotspot2 = -1;      // +0xC0
	int _hotspot3 = -1;      // +0xC4
	int _hotspotScene = 0;   // +0xC8: which of the three hotspot scenes is next
};

/** The story room class the factory builds for @p room, or nullptr. */
RoomClass *createStoryRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_STORY_ROOMS_H
