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

#ifndef TERRAGLYPH_BRAD_ROOMS_H
#define TERRAGLYPH_BRAD_ROOMS_H

#include "common/array.h"

#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_class.h"

namespace Terraglyph {

/**
 * Phantom's Brad game, ROOM_Brad_SPOOKY, ROOM_Brad_SPOOKIER and
 * ROOM_Brad_SPOOKTACULAR by level (0x41E9E0, vtable 0x45EF50): Brad is
 * buried under a pile of armour, its pieces hanging on springs from their
 * places and pushing on their neighbours. Only the next piece in the
 * level's order comes off: dragged far enough its own way, it breaks loose
 * and flies off the screen. Tugging another too far brings Brad's
 * complaints. The last piece gone: BradPuzzle, and back to the arena.
 */
class BradGameRoom : public RoomClass, public Room::Actor {
public:
	static const uint kMaxPieces = 10;

	/** A piece of armour (0x41DAB0, vtable 0x45EED8). */
	struct Piece {
		int sprite = -1;
		float x = 0, y = 0;         // the sprite's +0x0C, +0x10
		float ax = 0, ay = 0;       // +0x6C, +0x70: where it hangs
		float share = 0;            // +0x5C: its place in the order, (i + 1) / count
		float rest = 0;             // +0x68: its way out from the pile's middle
		float offX = 0, offY = 0;   // +0x74, +0x78: the mouse from it, dragged
		float vx = 0, vy = 0;       // +0x7C, +0x80
		float prevX = 0, prevY = 0; // +0xA4, +0xA8: last tick's place
		bool dragging = false;      // +0x64
		bool loose = false;         // +0x65: broken off
		bool strained = false;      // +0x84: pulled past 20 when not its turn
		bool gone = false;          // off the screen (+0x30)
		Common::Array<int> links;   // +0x88: its neighbours, last linked first
	};

	explicit BradGameRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x41EA40
	void enter(const Common::String &from) override;                               // 0x41EE80
	void roomLeft() override;                                                      // 0x41F3A0
	void tick() override;                                                          // 0x41F3C0
	bool click(int sprite) override;
	bool isBusy() const override { return _won; }                                  // game +0x24
	bool hidesCursor() const override { return !_introDone || _won; }              // 0x435E80 / 0x435EF0 on the cursor
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x41EA60
	void actorUpdate(uint sprite) override;                                        // the pieces' +0x0C, 0x41DD60

	// For the check.
	uint getCount() const { return _count; }
	uint getNext() const { return _next; }
	int getOrder(uint i) const { return _order[i]; }
	Piece &getPiece(uint i) { return _pieces[i]; }
	int getHeads() const { return _heads; }
	int getMouths() const { return _mouths; }
	bool isIntroDone() const { return _introDone; }
	bool isWon() const { return _won; }
	int getExitTimer() const { return _exitTimer; }
	int getExcitedTimer() const { return _excitedTimer; }
	const Common::String &getVoice() const { return _voice; }
	const Common::String &getNextVoice() const { return _nextVoice; }
	int getWrongCount() const { return _wrongCount; }
	/** 0x41F550: the piece whose turn it is, or -1. */
	int current() const { return _next < _count ? _order[_next] : -1; }
	/** Move piece @p i to (@p x, @p y) at once, its sprite with it. */
	void placeForCheck(uint i, float x, float y);

private:
	void link(int a, int b);
	void restAngles();
	void pieceGone(int i);
	void complain(int i);
	void rattle(int i);
	void sync(const Piece &p);
	int findPiece(int sprite) const;

	Piece _pieces[kMaxPieces];   // +0xBC
	uint _count = 0;             // +0xB8
	int _order[kMaxPieces] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }; // +0xE4
	uint _next = 0;              // +0x10C
	int _lastWrong = -1;         // +0x110
	int _wrongCount = 0;         // +0x114
	Common::String _voice;       // +0x118: the line last played
	Common::String _nextVoice;   // +0x11C
	int _heads = -1;             // +0x120
	int _mouths = -1;            // +0x124
	int _excitedTimer = 0;       // +0x128
	bool _introDone = false;     // +0x12C
	int _exitTimer = 0;          // +0x130
	bool _won = false;
	bool _pressed = false;       // the button went down on a sprite (game +0x20 bit 2, +0x08)
	int _pressedOn = -1;
};

/** The class for this room, or nullptr. */
RoomClass *createBradRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_BRAD_ROOMS_H
