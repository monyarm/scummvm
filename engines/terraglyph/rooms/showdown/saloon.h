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

#ifndef TERRAGLYPH_SALOON_H
#define TERRAGLYPH_SALOON_H

#include "terraglyph/rooms/class.h"
#include "terraglyph/rooms/script.h"

namespace Terraglyph {

/**
 * Showdown's saloon, ROOM_P12_Saloon (0x414430, vtable 0x45E990): Artie,
 * gagged and in three chains. Clicked, Velma takes his gag off. Three keys
 * are hidden behind six of the room's hotspots; each one found and clicked
 * takes a chain off him (ROOM_Artie). Until he is free, the gang won't
 * leave. Free, he gives his clue. The piano, the mechanical bull (the
 * Buffalo nickel) and the chandelier's pump handle are here too.
 */
class SaloonRoom : public RoomClass {
public:
	static const uint kKeys = 6;

	explicit SaloonRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x414490
	void enter(const Common::String &from) override;                                // 0x414540
	void roomLeft() override;                                                       // 0x414820
	void tick() override;                                                           // 0x414910
	bool click(int sprite) override;                                                // 0x414910, Artie and Shaggy clicked
	void event(const Common::String &object, bool success) override;                // 0x415660
	void idle() override;                                                           // 0x4148D0
	bool leaveBy(const Common::String &exitObject) override;                        // 0x415C30
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x414FC0
	bool wantsGroup(int &depth) override { depth = -1001; return true; }            // 0x414FC0's RENDER_ENTITY
	void saveKeys(Common::INIFile &profile) override;                               // 0x416000
	void loadKeys(const Common::INIFile &profile) override;                         // 0x4161D0
	// Game +0x24, held while Velma takes the gag off and until the thanks.
	bool isBusy() const override { return _ungag || _thanks; }

	/** The keys hidden (+0xCD), for a check: @p mask's bits 0-5. */
	void hideKeys(uint mask);

	bool isMasked() const { return _masked; }
	int getChained() const { return _chained; }
	bool isPianoPlayed() const { return _shaggyPiano; }
	bool isBullRidden() const { return _bullRidden; }
	bool isKeyHidden(uint k) const { return _keyHidden[k]; }
	bool isKeyVisible(uint k) const { return _keyVisible[k]; }
	bool isKeyFound(uint k) const { return _keyFound[k]; }
	bool isOpen(uint k) const { return _open[k]; }
	int getHotspot(uint k) const { return _hotspots[k]; }
	int getKey(uint k) const { return _keys[k]; }
	int getPiano() const { return _piano; }
	int getMoan() const { return _moan; }
	bool isMusicWaiting() const { return _music; }
	bool isGiving() const { return _gives; }
	bool isUngagging() const { return _ungag; }
	bool isVelmaStarted() const { return _velmaStarted; }
	bool isVelmaSpoken() const { return _velmaSpoke; }
	int getBullTries() const { return _bullTries; }
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }
	const RoomScript::Exit &getLastExit() const { return _lastExit; }

private:
	/** 0x415C30's own part: true when it keeps the gang here, with a line. */
	bool keepsHere(const Common::String &destination);
	/** The class's own exit (its vtable +0x24 with these keys). */
	void leave(const char *destination, const char *scene);
	bool scene(const char *name, bool block);
	void playSound(const Common::String &name);

	int _piano = -1;            // +0xB8: hotspot 1
	int _moan = 36;             // +0xBC: ticks to the next moan
	bool _music = false;        // +0xC0: music group 3 again once the piano stops
	bool _masked = true;        // +0xC1: ArtieMasked
	int _chained = 3;           // +0xC4: ArtieChained
	bool _shaggyPiano = false;  // +0xC8: ShaggyPiano
	// +0xCC + 16k for hotspots 3-8 (k 0-5):
	bool _keyVisible[kKeys] = {}; // +0xCC: KeyVisible
	bool _keyHidden[kKeys] = {};  // +0xCD: KeyHidden
	int _hotspots[kKeys] = { -1, -1, -1, -1, -1, -1 }; // +0xD0
	int _keys[kKeys] = { -1, -1, -1, -1, -1, -1 };     // +0xD4
	bool _keyFound[kKeys] = {};   // +0xD8: the key shows once its hotspot stops
	bool _open[kKeys] = {};       // +0x12C: ClickableOpen
	bool _gives = false;        // +0x132: Artie gives his clue when idle
	bool _thanks = false;       // +0x133: then the thanks and the suspects
	bool _bullRidden = false;   // +0x138: BullRidden
	int _bullTries = 0;         // +0x13C: the bull clicked without the nickel
	bool _ungag = false;        // +0x140: Velma takes the gag off
	bool _velmaStarted = false; // +0x141
	bool _velmaSpoke = false;   // +0x142
	bool _artieClicked = false;
	bool _shaggyNickel = false;
	Common::String _lastScene;
	Common::String _lastSound;
	RoomScript::Exit _lastExit;
};

/**
 * Showdown's ROOM_Artie (0x40D4E0, vtable 0x45E68C): one of Artie's chains
 * coming off, 3 then 2 then 1 (ChainsLocked). Its sound plays after a
 * second; a second after it stops, back to the saloon, through movie 6
 * once the last is off.
 */
class ArtieRoom : public RoomClass {
public:
	explicit ArtieRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x40D540
	void enter(const Common::String &from) override;                                // 0x40D680
	void tick() override;                                                           // 0x40D560
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x40D6D0
	void saveKeys(Common::INIFile &profile) override;                               // 0x40D820
	void loadKeys(const Common::INIFile &profile) override;                         // 0x40D860
	// Game +0x24 and the cursor faded (0x435150, 0x12) the whole time.
	bool isBusy() const override { return true; }

	int getChainsLocked() const { return _chainsLocked; }
	int getCountdown() const { return _countdown; }
	int getChains() const { return _chains; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	int _chainsLocked = 3;      // +0xB8: ChainsLocked
	int _chains = -1;           // +0xBC
	int _countdown = 36;        // +0xC0
	Common::String _lastSound;
};

/** The saloon's room classes the factory builds for @p room, or nullptr. */
RoomClass *createSaloonRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_SALOON_H
