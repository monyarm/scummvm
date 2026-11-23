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

#ifndef TERRAGLYPH_VAULT_ROOMS_H
#define TERRAGLYPH_VAULT_ROOMS_H

#include "terraglyph/room_class.h"

namespace Terraglyph {

/**
 * Showdown's bank vault, ROOM_P16_Bank_Vault (0x429220, vtable 0x45F128):
 * the safe, which ICDouble's combination opens with Daphne's help and which
 * leads to ROOM_Open_Safe; the strong boxes (hotspots 1-4), one of which
 * Street 1's key opens, through a movie, for the dentures; and the dust
 * (hotspot 6), which uncovers a scene.
 */
class BankVaultRoom : public RoomClass {
public:
	explicit BankVaultRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x429280
	void enter(const Common::String &from) override;                                // 0x4292A0
	void tick() override;                                                           // 0x429870
	bool isBusy() const override { return _doorClicked; }                           // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x4295E0
	void event(const Common::String &object, bool success) override;               // 0x429350
	void saveKeys(Common::INIFile &profile) override;                               // 0x4297F0
	void loadKeys(const Common::INIFile &profile) override;                         // 0x429830

	int getDoorSprite() const { return _door; }
	int getDustSprite() const { return _dust; }
	bool isDustVisible() const { return _dustVisible; }
	bool isDoorClicked() const { return _doorClicked; }
	/** The last scene and sound the class started, for the checks. */
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	/** An item taken and no longer in a slot: used (0x407060, 0x4070C0). */
	bool used(const char *item) const;
	void scene(const char *name, bool block);

	int _door = -1;            // +0xB8: OBJ_P16_DOOR, a requires object
	int _dust = -1;            // +0xBC: hotspot 6, a clickable
	bool _dustVisible = false; // +0xC0: DustVisible
	int _boxLine = 0;          // +0xC4: which of the two lines a box clicked with the arrow gets
	bool _doorClicked = false; // +0xC8: the safe's door was clicked
	bool _daphneDone = false;  // +0xC9: Daphne has tried it
	Common::String _lastScene, _lastSound;
};

/**
 * Showdown's open safe, ROOM_Open_Safe (0x429BE0, vtable 0x45F168): the gang
 * in the vault talk, their mouths moved by the lines' lip sync, with input
 * held; then the back alley. Once seen, it goes straight there.
 */
class OpenSafeRoom : public RoomClass {
public:
	explicit OpenSafeRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x429C40
	void tick() override;                                                           // 0x429F10, with +0x38 (0x429CB0)
	bool skip() override;                                                           // 0x429CB0's input slot 5
	// +0x3C (0x42A320) holds input and hides the cursor every tick. Only
	// input slot 5 (Escape, Space) gets through, to skip(): clicks are
	// swallowed here rather than taken as a skip, as a busy class's are.
	bool hidesCursor() const override { return true; }
	bool click(int sprite) override { return true; }
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42A040

	static const uint kGang = 5;
	int getGangSprite(uint i) const { return _gang[i]; }
	int getMouth() const { return _mouth; }
	const Common::String &getScene() const { return _scene; }
	const Common::String &getClip() const { return _clip; }
	int getLine() const { return _line; }
	int getCountdown() const { return _countdown; }

private:
	/** 0x429CB0, the scene's step (room vtable +0x38). */
	void nextLine();

	Common::String _clip;        // +0xB8: the line playing
	int _countdown = 0;          // +0xBC: ticks before the first line
	int _gang[kGang] = { -1, -1, -1, -1, -1 }; // +0xC0..+0xD0: Daphne, Fred, Velma, Shaggy, Scooby
	int _mouth = -1;             // +0xD4: the speaker's sprite
	int _line = 0;               // +0x1C
	Common::String _scene;       // +0x14
	bool _skipped = false;
};

/**
 * Showdown's bank, ROOM_P13_Bank (0x428660, vtable 0x45F0E8): the vault
 * door, which the broken clock opens. Clicked with the clock, the gang
 * hears a time (by level) and goes to the clock's dial; the time set there,
 * the door swings open on their way back. The moans while the story has
 * them, and the gang walking in.
 */
class BankRoom : public RoomClass {
public:
	explicit BankRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x4286C0
	void prepare() override;                                                        // 0x4286E0 (+0x30)
	void enter(const Common::String &from) override;                                // 0x428FE0
	void tick() override;                                                           // 0x4290D0
	bool isBusy() const override { return _opening > 0; }                           // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x428EC0
	void event(const Common::String &object, bool success) override;               // 0x428710

	/** The time drawn for @p level and draw @p r (0-9), as hour and minute (0x428710's tables). */
	static void getTime(int level, uint r, int &hour, int &minute);

	int getDoorSprite() const { return _door; }
	int getOpeningCountdown() const { return _opening; }
	int getMoanDelay() const { return _moanDelay; }
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	int _door = -1;          // +0xB8: OBJ_P13_DOOR, a requires object
	int _opening = 0;        // +0xBC: ticks until the door swings open
	int _moanDelay = 36;     // +0xC0
	int _doorLine = 0;       // +0xC4: which line the door clicked with the arrow gets
	Common::String _lastScene, _lastSound;
};

/**
 * The vault door's clock, ROOM_Vault_Clock (0x42A780, vtable 0x45F1E8): two
 * hands dragged round to the time the bank's scene named. Set, the vault
 * opens (VaultOpen) and the gang goes back to the bank.
 */
class VaultClockRoom : public RoomClass {
public:
	explicit VaultClockRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x42AA70
	void tick() override;                                                           // 0x42AAA0
	bool click(int sprite) override;                                                // game +0x20 bit 2, read by the tick
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x42A830

	/** The step a hand of @p steps points at for the mouse at @p p (0x42AAA0). */
	static uint getTarget(const Common::Point &p, uint steps);

	int getHourSprite() const { return _hour; }
	int getMinuteSprite() const { return _minute; }
	int getDrag() const { return _drag; }
	int getClicks() const { return _clicks; }

private:
	int _hour = -1;          // +0xB8: OBJ_VAULT_CLOCK_HOUR, 12 steps
	int _minute = -1;        // +0xBC: OBJ_VAULT_CLOCK_MIN, 60 steps
	int _drag = 0;           // +0xC0: 0 none, 1 the hour hand, 2 the minute hand
	uint32 _lastHour = 0;    // +0xC8
	uint32 _lastMinute = 0;  // +0xCC
	bool _pressed = false;   // the button went down since the last tick (game +0x20 bit 2)
	int _pressedOn = -1;     // the sprite it went down on (+0x08)
	bool _wasDown = false;   // to see it come up (game +0x20 bit 4)
	bool _leaving = false;   // the exit was taken (the original takes it again every tick)
	int _clicks = 0;         // the dial's clicks played, for the checks
};

/** The vault's room class the factory builds for @p room, or nullptr. */
RoomClass *createVaultRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_VAULT_ROOMS_H
