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

#ifndef TERRAGLYPH_MENU_ROOMS_H
#define TERRAGLYPH_MENU_ROOMS_H

#include "terraglyph/room_class.h"

namespace Terraglyph {

/**
 * The title (Showdown 0x428260, vtable 0x45F0A8): the title movies 27 and
 * 28, then the 1920-pixel background panning 2 pixels a tick for 324 ticks,
 * then Sign In. A skip key goes there at once.
 */
class TitleRoom : public RoomClass {
public:
	explicit TitleRoom(TerraglyphEngine &engine) : RoomClass(engine), _state(2), _countdown(0) {}
	void enter(const Common::String &from) override;
	void tick() override;
	bool skip() override;
	void reset() override { _state = 2; _countdown = 0; }
	bool isBusy() const override { return _state > 3; }
	/** The step the entry is at (+0xBC): 1-3 ask for a title movie, then the title shows. */
	uint getState() const { return _state; }
	int getCountdown() const { return _countdown; }

private:
	uint _state;     // +0xBC, 2 when the game starts (vtable +0x08, 0x4282C0)
	int _countdown;  // +0xB8
};

/** The main menu (0x4229C0, vtable 0x45EF2C): Play, Options, New Mystery, Quit. */
class MainMenuRoom : public ButtonRoom {
public:
	explicit MainMenuRoom(TerraglyphEngine &engine) : ButtonRoom(engine) {}
	void enter(const Common::String &from) override;
	const Common::String &getReturnRoom() const { return _from; }

protected:
	void onClick(int button) override;
	void onRollover(int button, bool over) override;

private:
	Common::String _from;   // +0xBC: the room Play goes back to
	Common::String _line;   // +0xB8: the narrator line playing
	int _play = -1, _options = -1, _mystery = -1, _quit = -1;
};

/** The level select (0x422680, vtable 0x45EEEC): three buttons start the game. */
class DifficultyRoom : public ButtonRoom {
public:
	explicit DifficultyRoom(TerraglyphEngine &engine) : ButtonRoom(engine) {}
	void enter(const Common::String &from) override;

protected:
	void onClick(int button) override;
};

/** The quit screen (0x423DB0, vtable 0x45EFB8): Yes quits, No goes back. */
class QuitRoom : public ButtonRoom {
public:
	explicit QuitRoom(TerraglyphEngine &engine) : ButtonRoom(engine) {}
	void enter(const Common::String &from) override;
	void tick() override;
	bool isBusy() const override { return !_line.empty(); }
	bool isQuitting() const { return _quitting; }
	/** The answer's line is over. */
	bool isDone() const { return _done; }
	int getEyesSprite() const { return _eyes; }

protected:
	void onClick(int button) override;
	void onRollover(int button, bool over) override;

private:
	Common::String _from;  // +0xB8
	Common::String _line;  // +0xCC
	bool _quitting = false; // +0xC8
	bool _talking = false;  // +0xC9
	bool _done = false;
	int _yes = -1, _no = -1, _eyes = -1, _mouth = -1;
};

} // End of namespace Terraglyph

#endif // TERRAGLYPH_MENU_ROOMS_H
