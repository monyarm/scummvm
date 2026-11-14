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

#ifndef SCOOBY_INVENTORY_H
#define SCOOBY_INVENTORY_H

#include "common/array.h"
#include "common/formats/ini-file.h"
#include "common/hashmap.h"
#include "common/str.h"

namespace Common {
class RandomSource;
}

namespace Scooby {

class Room;
class RoomScript;
class Sound;

/**
 * The Gen 1 inventory (Inventory.cpp, game +0xAC) and the toolbar it draws:
 * the Mystery Machine at the foot of the screen, towing four item slots,
 * the magnifying glass, the snack box, Scrappy and the quit button. With
 * it, the game-level cursor (game +0x238), the inventory and requires
 * objects of the room, and the line a gang member says when an item is
 * used on nothing (0x4098B0).
 */
class Inventory {
public:
	static const uint kSlotCount = 4;
	static const uint kHistorySize = 32;

	/** The toolbar's states (0x407AF0). */
	enum State {
		kStateOpen = 0,    // at the right edge, the slots in view
		kStateParked = 1,  // at x 0, the slots off the left edge
		kStateMovingIn = 2,
		kStateMovingOut = 3,
		kStateBusy = 4     // hidden (0x407CC0)
	};

	Inventory(Room &room, Sound &sound, RoomScript &script, const Common::INIFile &objectIni);

	/** A new game (0x406530): no items, one snack, the arrow cursor. */
	void reset();

	/** The source for the wrong-item line's speaker (rand() % 4); null picks the first present. */
	void setRandom(Common::RandomSource *random) { _random = random; }

	/**
	 * After the room loads (0x40B920, 0x402AD0, 0x409510): build the
	 * toolbar, park it, hide the room's items already taken, and put the
	 * arrow back.
	 */
	void enterRoom();

	/** A click this tick on @p sprite (-1 for none), handled by tick(). */
	void click(int sprite);

	/**
	 * The object updates, in the sprite tree's order: the toolbar, then the
	 * room's inventory and requires objects. Run after Room::tick().
	 */
	void tickObjects();

	/** The game's update (0x4098B0), after the room's: the wrong-item line. */
	void tickGame();

	/** The game cursor (+0x238): ANIM_CURSORARROW, or the item in use. */
	const Common::String &getCursor() const { return _cursor; }
	bool isDefaultCursor() const;
	void setCursor(const Common::String &anim) { _cursor = anim; }

	/**
	 * Whether @p sprite is one of the toolbar's, or a room's inventory or
	 * requires object, and so is the inventory's to roll over and click.
	 */
	bool handles(int sprite) const;
	/** The sprite's rollover cursor (its vtable +0x18), or empty. */
	Common::String getRollover(int sprite) const;

	/** 0x406DD0: put an item in the first free slot. */
	bool addItem(const Common::String &name, const Common::String &toolbarAnim, const Common::String &useCursor);
	/** 0x406DD0 with the item's object.ini keys. */
	bool addItem(const Common::String &name);
	/** 0x406C80: take an item out of its slot. */
	bool removeItem(const Common::String &name);
	/** 0x407060: whether the item has ever been taken (the history). */
	bool hasItem(const Common::String &name) const;
	const Common::String &getSlot(uint slot) const { return _slots[slot].name; }

	/** The van's sprite, and the sprite showing a slot's item (-1 if none). */
	int getVanSprite() const { return _van; }
	int getItemSprite(uint slot) const { return _slots[slot].item; }

	State getState() const;
	int getVanX() const { return _x; }
	/** The van's x when the toolbar is open. */
	int getOpenX() const;

	/** The room the inventory asks for (a pick-up movie, the quit button). */
	bool getNextRoom(Common::String &room, int &movie) const;
	/** Input stops for a pick-up movie (0x402B00). */
	bool isInputBlocked() const { return _inputBlocked; }

	/** What happened since the last call, for the checks: events, sounds, lines. */
	Common::String takeLog();
	/** The toolbar's sprites as "label anim x,y wxh", front to back. */
	Common::String describeToolbar() const;

private:
	enum Role {
		kRoleNone,
		kRoleVan,
		kRoleBlockMoving,   // +0x1C, in front while the toolbar moves
		kRoleBlockOpen,     // +0x20, behind the open toolbar
		kRoleFingerprint,
		kRoleItem,
		kRoleButton,        // magnifying glass (7), quit (6), snack digits (8)
		kRoleSnackBox,
		kRoleScrappy,
		kRolePickup,        // a room's inventoryobject
		kRoleRequires       // a room's requiresobject
	};

	struct Slot {
		Common::String name;
		Common::String toolbarAnim;
		Common::String useCursor;
		int fingerprint;
		int item;
	};

	struct Pickup {
		Common::String name;
		Common::String toolbarAnim;
		Common::String useCursor;
		Common::String sound;
		int movie;
		bool pending; // +0x88
	};

	int addToolbarSprite(int parent, const char *anim, int index, Role role, int depth = kToolbarDepth);
	void setVanX(int x);
	void setRole(int sprite, Role role, int index);
	void startVan(const char *anim, int mode);
	void openToolbar();               // 0x407C00
	void clickToolbar(int index);     // 0x406A70
	void updateToolbar();             // 0x407120
	void updatePickup(int sprite, Pickup &p, bool clicked);  // 0x402B00
	void clickRequires(int sprite);   // 0x402F20
	void roomEvent(const Common::String &object, bool success);
	void playSound(const Common::String &name);
	void showSnacks();                // 0x40D3B0
	Common::String getIniString(const Common::String &section, const char *key) const;

	static const int kToolbarDepth = 5000;

	Room &_room;
	Sound &_sound;
	RoomScript &_script;
	const Common::INIFile &_ini;

	Slot _slots[kSlotCount];
	Common::String _history[kHistorySize];
	uint _snacks;                 // +0x30
	Common::String _cursor;       // game +0x238
	bool _busy;                   // +0x1B4
	int _x;                       // the van's x (+0xC of +0x4)
	int _total;                   // +0x24: the towed sprites' widths, plus gaps

	int _van, _vanWidth, _vanY, _blockMoving, _blockOpen;
	int _tens, _units;
	Common::HashMap<int, Role> _roles;
	Common::HashMap<int, int> _indexes;    // a toolbar sprite's index (+0x5C)
	Common::HashMap<int, Pickup> _pickups;

	int _clickSprite;
	bool _clicked;
	int _heldTicks;               // 0x475EFC
	bool _gangHere;               // game +0x240
	bool _openOnEntry;            // game +0x248
	bool _quitting;               // +0x1B5
	bool _inputBlocked;
	Common::RandomSource *_random;
	Common::String _nextRoom;
	int _movie;
	Common::String _log;
};

} // End of namespace Scooby

#endif // SCOOBY_INVENTORY_H
