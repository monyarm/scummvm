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

#ifndef TERRAGLYPH_GUMMY_ROOMS_H
#define TERRAGLYPH_GUMMY_ROOMS_H

#include "terraglyph/room_class.h"
#include "terraglyph/room_script.h"

namespace Terraglyph {

/**
 * Showdown's whistle puzzle, ROOM_Gummy (0x40FC40, vtable 0x45E7D0): Gummy
 * whistles a tune in his sleep, and the bells play it back note by note.
 * Ringing the five bells in the tune's order solves it (GummyPuzzle), meets
 * Gummy, and sends the gang back to the railroad station.
 */
class GummyRoom : public RoomClass {
public:
	explicit GummyRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x4102C0
	void tick() override;                                                           // 0x40FD10
	void event(const Common::String &object, bool success) override;                // 0x4109B0
	void clicked(uint sprite) override;                                             // 0x410A00, a bell's callback
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x410760
	// Game +0x24, set while Gummy whistles and the bells play the tune.
	bool isBusy() const override { return _state == 1 || (_state == 3 && (_demo || _replay)); }
	// The cursor faded once the tune is rung (0x435150), until the room changes.
	bool hidesCursor() const override { return _exitCountdown < kNever; }

	/** 0x4102C0's choice: tune @p k (0-2) of the level's three, its whistle, notes and length. */
	void chooseTune(int level, int k);

	int getState() const { return _state; }
	const Common::String &getTune() const { return _tune; }
	const Common::String &getWhistle() const { return _whistle; }
	int getNote(uint bell) const { return _notes[bell]; }
	uint getLength() const { return _length; }
	uint getDemoCount() const { return _demoCount; }
	uint getIndex() const { return _index; }
	bool isReplay() const { return _replay; }
	bool isDemo() const { return _demo; }
	int getExitCountdown() const { return _exitCountdown; }
	int getSnore() const { return _snore; }
	int getBlink() const { return _blink; }
	int getBell(uint i) const { return _bells[i]; }
	uint getPending(uint i) const { return _pending[i]; }
	int getGummy() const { return _gummy; }
	int getDonkey() const { return _donkey; }
	const Common::String &getCurrent() const { return _current; }
	const Common::String &getDemoLog() const { return _demoLog; }
	const Common::String &getLastSound() const { return _lastSound; }

	static const int kNever = 1000000;

private:
	void playSound(const Common::String &name, bool stopFirst);

	int _bells[5] = { -1, -1, -1, -1, -1 }; // +0xB8: the bells, by their digit
	int _donkey = -1;           // +0xCC
	int _gummy = -1;            // +0xD0
	Common::String _whistle;    // +0xD4: the tune Gummy whistles
	bool _replay = false;       // +0xD8: Gummy whistles it again
	bool _demo = false;         // +0xD9: the bells play it back
	Common::String _current;    // +0xDC: the sound waited for
	uint _length = 0;           // +0xE0: the tune's notes
	Common::String _tune;       // +0xE4: its notes, as digits
	uint _index = 0;            // +0xEC: the next note
	uint _demoCount = 0;        // +0xF0: the notes the bells play back
	int _notes[5] = { 0, 0, 0, 0, 0 }; // +0xF4: each bell's note
	int _exitCountdown = kNever; // +0x108: ticks to the station once rung
	int _blink = 0;             // +0x10C: ticks to the donkey's next blink
	int _snore = 0;             // +0x110: ticks to Gummy's next snore
	bool _sceneDone = false;    // +0x114: 220B2_'s done flag
	int _state = 0;             // +0x118
	uint _pending[5] = { 0, 0, 0, 0, 0 }; // each bell's +0x80: rings waiting for it to be free (0x401840)
	bool _scenePlaying = false; // 220B2_ started, its done flag not yet set
	bool _leaving = false;
	Common::String _demoLog;
	Common::String _lastSound;
};

/** Gummy's room class the factory builds for @p room, or nullptr. */
RoomClass *createGummyRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_GUMMY_ROOMS_H
