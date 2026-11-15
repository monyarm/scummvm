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

#ifndef SCOOBY_ROOM_CLASS_H
#define SCOOBY_ROOM_CLASS_H

#include "common/array.h"
#include "common/str.h"

namespace Scooby {

class ScoobyEngine;

/**
 * A room's own class. The game's room factory (Showdown 0x42E0E0, game
 * vtable +0x14) builds one object per room name when the game starts, and
 * keeps it: a class's state lasts across visits. Rooms without a class of
 * their own get the base class (vtable 0x45E5FC), which is the engine's
 * default behaviour. A class runs after the base behaviour it extends.
 */
class RoomClass {
public:
	explicit RoomClass(ScoobyEngine &engine) : _engine(engine) {}
	virtual ~RoomClass() {}

	/** Room vtable +0x0C, after the base entry (0x40B920): @p from is the room left. */
	virtual void enter(const Common::String &from) {}
	/** Room vtable +0x04, after the base tick (0x40B490). */
	virtual void tick() {}
	/** A click on @p sprite (-1 for none), before the engine's own handling. Returns true if taken. */
	virtual bool click(int sprite) { return false; }
	/** A key that skips (the original's input slots 4 and 5). Returns true if taken. */
	virtual bool skip() { return false; }
	/** A typed key (ASCII, or a keycode with no character). Returns true if taken. */
	virtual bool key(uint16 ascii, int keycode) { return false; }
	/** Room vtable +0x08: back to how the factory built it, for a new game (0x435780). */
	virtual void reset() {}
	/** Input is held (game +0x24) and the cursor is hidden. */
	virtual bool isBusy() const { return false; }

protected:
	ScoobyEngine &_engine;
};

/**
 * The menus' button (0x436C50): a sprite whose step is its state, 0 at
 * rest, 1 under the mouse, 2 pressed (its vtable +0x08, 0x436D70). Coming
 * under the mouse plays its sound (+0x60), unless that one is the button
 * sound already playing, and calls the room's rollover callback (+0x80);
 * leaving calls it again; a click calls the room's click callback (+0x78).
 * Its hit test is the step polygon if any step has one, else its cels
 * (0x436FC0).
 */
class ButtonRoom : public RoomClass {
public:
	explicit ButtonRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void tick() override;
	bool click(int sprite) override;
	uint getButtonCount() const { return _buttons.size(); }
	int getButtonSprite(uint button) const { return _buttons[button].sprite; }

protected:
	struct Button {
		int sprite;
		Common::String sound;
		int state;
	};

	/** Make @p sprite a button, its step held at its state. Returns its index. */
	int addButton(int sprite, const Common::String &sound = Common::String());
	/** The button at @p sprite, or -1. */
	int findButton(int sprite) const;
	/** The first sprite showing @p object, or -1. */
	int findSprite(const Common::String &object) const;
	void clearButtons() { _buttons.clear(); }

	virtual void onClick(int button) {}
	virtual void onRollover(int button, bool over) {}

	Common::Array<Button> _buttons;
};

/** The class the factory builds for @p room, or nullptr for the base class. */
RoomClass *createRoomClass(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_ROOM_CLASS_H
