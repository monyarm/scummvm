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

#ifndef SCOOBY_MARIA_ROOMS_H
#define SCOOBY_MARIA_ROOMS_H

#include "common/array.h"

#include "scooby/room_class.h"

namespace Scooby {

/**
 * Phantom's Maria game, ROOM_Maria (0x428770, vtable 0x45F2C0): cups and
 * plates lie on the floor; a click picks one up, the mouse carries it, and
 * a second click lets it go, thrown at the speed it was moving. Caught in
 * one of Maria's two pans it stays; she tilts by the difference in weight,
 * and past four everything falls off. Every item in the pans and the pans
 * equal: MariaPuzzle, and back to the dining room.
 */
class MariaGameRoom : public RoomClass {
public:
	/** An item (0x4279D0, vtable 0x45F280). */
	struct Item {
		int sprite = -1;
		Common::String name;   // +0x60: MARIAGAME_<colour><kind>
		Common::String sound;  // +0xA0: Cup or Plate
		int weight = 0;        // +0x9C: 2 for B, 1 for C, 4 for P
		float x = 0, y = 0;    // the sprite's +0x0C, +0x10
		float vx = 0, vy = 0;  // +0x68, +0x6C
		float offX = 0, offY = 0;   // +0x70, +0x74: where it was taken
		float prevX = 0, prevY = 0; // +0x78, +0x7C: last tick's place
		float floor = 0;       // +0x80
		bool resting = true;   // +0x84
		int stopStep = -1;     // +0x88: the step its sequence stops on
		bool dragging = false; // +0x8C
		int pan = 0;           // +0x90: 1 or 2 when headed for a pan
		float panX = 0, panY = 0; // +0x94, +0x98
		bool spinning = false; // +0xA8: its Maria.Spin
	};

	explicit MariaGameRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                               // 0x428890
	void tick() override;                                                          // 0x428CE0
	bool click(int sprite) override;
	bool isBusy() const override { return _won; }                                  // game +0x24
	bool hidesCursor() const override;
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x428F60

	// For the check.
	Common::Array<Item> &getItems() { return _items; }
	const Common::Array<int> &getPan(int pan) const { return pan == 1 ? _left : _right; }
	int getMaria() const { return _maria; }
	int getTotal() const { return _total; }
	bool isRebalancing() const { return _rebalance; }
	int getExitTimer() const { return _exitTimer; }
	int getExcitedTimer() const { return _excitedTimer; }
	/** Clear the items (their sprites hidden) and add one, as 0x4279D0 builds it. */
	void clearItemsForCheck();
	int addItemForCheck(float floorY, int colour, int kind, float x);
	/** 0x429180: put item @p i in pan @p pan (1 or 2). */
	void placeOnPan(int i, int pan) { place(i, pan); }
	/** 0x429270: whether item @p i may be taken (the top of a pan, or no pan). */
	bool isPickable(int i) const;
	/** 0x429040: the pan at (@p x, @p y), 0 for none, and where an item lands in it. */
	int panAt(float x, float y, float &tx, float &ty) const;

private:
	void generate();
	int makeItem(float floorY, int colour, int kind, float x);
	void updateItem(int i, bool click, int clicked);
	bool takeFromPan(int i);
	void place(int i, int pan);
	void knockOff(int i);
	void pendDepth(int i, int depth);
	void rebalance();
	void playSound(const Common::String &name);
	void syncSprite(const Item &it);

	Common::Array<Item> _items;
	Common::Array<int> _left;      // +0xF4, count +0x16C: pan 1
	Common::Array<int> _right;     // +0x130, count +0x170: pan 2
	Common::Array<int> _pendItem;  // +0x174, count +0x1EC
	Common::Array<int> _pendDepth; // +0x1B0
	int _maria = -1;               // +0x1F0
	int _excitedTimer = 0;         // +0x1F4
	bool _rebalance = false;       // +0x1F8
	int _exitTimer = 0;            // +0x24C
	int _total = 0;                // +0x250
	int _spinCount = 0;            // 0x474D5C: items spinning
	bool _won = false;
	bool _click = false;
	int _clicked = -1;
};

/** The class for this room, or nullptr. */
RoomClass *createMariaRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_MARIA_ROOMS_H
