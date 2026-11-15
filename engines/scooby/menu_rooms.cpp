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

// The Gen 1 front end. The addresses are Showdown's; Phantom's classes are
// the same code, with its own strings (FBFTitleMovies, ROOM_P01_Drawbridge).

#include "common/config-manager.h"
#include "common/debug.h"

#include "scooby/detection.h"
#include "scooby/menu_rooms.h"
#include "scooby/room.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

// ---------------------------------------------------------------------------
// The title (0x428380 entry, 0x4282E0 tick, 0x428480 placements)

static const int kTitleStart = -640;    // 0xC4200000
static const int kTitlePan = 2;         // 0x45EB18
static const int kTitleWrap = 1280;     // 0x45EB14
static const int kTitleTicks = 324;     // 0x144: 9 seconds

void TitleRoom::enter(const Common::String &from) {
	Room &room = *_engine.getRoom();
	if (_state <= 3) {
		// Steps 1-3 ask for title movie 26, 27 or 28, then this room again.
		static const int kMovies[] = { 0, 26, 27, 28 };
		if (_state >= 1)
			_engine.requestRoom(room.getName(), kMovies[_state]);
		debugC(1, kDebugScript, "title: step %u, movie %d", _state, _state >= 1 ? kMovies[_state] : -1);
		_state++;
		return;
	}

	// The title itself: the background (layer 0) from x -640, input held
	// and the cursor hidden, the toolbar hidden (0x407CC0), and the machine
	// remembers the title movies were seen (GBHTitleMovies=Yes).
	const int layer = room.getLayerSprite(0);
	if (layer >= 0)
		room.setOffset(layer, kTitleStart, 0);
	_countdown = kTitleTicks;
	ConfMan.setBool("title_movies_seen", true);
	// 0x428380 ends any fade: the music at full (0x434F50).
	_engine.getSound()->cancelMusicFade();
	ConfMan.flushToDisk();

	// 0x428480 builds three of the title's placements itself, at depths 1,
	// -1 and 100 instead of 0.
	static const struct {
		const char *object;
		int depth;
	} kDepths[] = { { "OBJ_TITLE_SCOOBY", 1 }, { "OBJ_TITLE_DAPHNE", -1 }, { "OBJ_TITLE_LEGAL", 100 } };
	for (uint i = 0; i < ARRAYSIZE(kDepths); i++) {
		for (uint s = 0; s < room.getSpriteCount(); s++) {
			if (room.getObjectName(s) == kDepths[i].object) {
				room.setDepth(s, kDepths[i].depth);
				break;
			}
		}
	}
	debugC(1, kDebugScript, "title: shown, %d ticks to Sign In", _countdown);
}

void TitleRoom::tick() {
	if (_state <= 3 || _countdown <= 0)
		return;
	// 0x4282E0: pan the background until the countdown ends, then Sign In.
	if (--_countdown != 0) {
		Room &room = *_engine.getRoom();
		const int layer = room.getLayerSprite(0);
		if (layer >= 0) {
			int x = room.getX(layer) + kTitlePan;
			if (x > 0)
				x -= kTitleWrap;
			room.setOffset(layer, x, 0);
		}
		return;
	}
	debugC(1, kDebugScript, "title: done, to Sign In");
	_engine.requestRoom("ROOM_Sign_In");
}

bool TitleRoom::skip() {
	// Input slot 5 (untraced keys; Escape, Space or a click here).
	if (_state <= 3 || _countdown <= 0)
		return false;
	debugC(1, kDebugScript, "title: skipped with %d ticks left", _countdown);
	_countdown = 0;
	_engine.requestRoom("ROOM_Sign_In");
	return true;
}

// ---------------------------------------------------------------------------
// The main menu (0x422A90 entry, 0x422B00 placements, callbacks 0x423120,
// 0x4233D0, 0x422FC0, 0x422E60; rollovers 0x423580, 0x423530)

void MainMenuRoom::enter(const Common::String &from) {
	// Play goes back to where the menu was opened from, but not to Quit or
	// Options, which come back here.
	if (from != "ROOM_Quit" && from != "ROOM_Options")
		_from = from;
	_line.clear();
	clearButtons();
	_play = _options = _mystery = _quit = -1;
	int s;
	if ((s = findSprite("OBJ_MAIN_MENU_PLAY")) >= 0)
		_play = addButton(s);
	if ((s = findSprite("OBJ_MAIN_MENU_OPTIONS")) >= 0)
		_options = addButton(s);
	if ((s = findSprite("OBJ_MAIN_MENU_MYSTERY")) >= 0)
		_mystery = addButton(s);
	if ((s = findSprite("OBJ_MAIN_MENU_QUIT")) >= 0)
		_quit = addButton(s);
	debugC(1, kDebugScript, "main menu: Play goes back to %s", _from.c_str());
}

void MainMenuRoom::onRollover(int button, bool over) {
	// Options and New Mystery have the narrator say what they do.
	if (!over || (button != _options && button != _mystery))
		return;
	Sound &sound = *_engine.getSound();
	if (!_line.empty())
		sound.stopClip(_line);
	_line = button == _options ? "Main_Menu.Sound_NARRATOR" : "Main_Menu.New_Mystery_NARRATOR";
	sound.play(_line);
	debugC(1, kDebugScript, "main menu: the narrator says %s", _line.c_str());
}

void MainMenuRoom::onClick(int button) {
	Common::String to;
	if (button == _play)
		to = _from == "ROOM_Title_Screen" ? "ROOM_Sign_In" : _from;
	else if (button == _options)
		to = "ROOM_Options";
	else if (button == _mystery)
		to = "ROOM_Sign_In";
	else if (button == _quit)
		to = "ROOM_Quit";
	debugC(1, kDebugScript, "main menu: to %s", to.c_str());
	_engine.exitTo(to);
}

// ---------------------------------------------------------------------------
// The level select (0x4226E0 entry, 0x422700 placements, callbacks 0x422930,
// 0x422960, 0x422990)

static const struct {
	const char *object;
	const char *sound;
} kLevels[] = {
	{ "OBJ_DIFFICULTY_SPOOKY", "Level_Select.SHAGGY.Spooky" },
	{ "OBJ_DIFFICULTY_SPOOKIER", "Level_Select.SHAGGY.Spookier" },
	{ "OBJ_DIFFICULTY_SPOOKTAC", "Level_Select.SHAGGY.Spooktacular" }
};

void DifficultyRoom::enter(const Common::String &from) {
	clearButtons();
	for (uint i = 0; i < ARRAYSIZE(kLevels); i++) {
		const int s = findSprite(kLevels[i].object);
		if (s < 0)
			continue;
		// The buttons are built at (0, 0); the placement's x and y aren't
		// read.
		_engine.getRoom()->setOffset(s, 0, 0);
		addButton(s, kLevels[i].sound);
	}
}

void DifficultyRoom::onClick(int button) {
	// Game +0x160 is the level; then movie 1, and the first room of the
	// story.
	Room &room = *_engine.getRoom();
	int level = 0;
	for (uint i = 0; i < ARRAYSIZE(kLevels); i++) {
		if (room.getObjectName(_buttons[button].sprite) == kLevels[i].object)
			level = i;
	}
	_engine.setDifficulty(level);
	debugC(1, kDebugScript, "level %d: movie 1, then %s", level, _engine.getFirstGameRoom().c_str());
	_engine.requestRoom(_engine.getFirstGameRoom(), 1);
}

// ---------------------------------------------------------------------------
// The quit screen (0x423E90 entry, 0x424140 tick, callbacks 0x424040,
// 0x424090; rollovers 0x4240E0, 0x424110)

void QuitRoom::enter(const Common::String &from) {
	Room &room = *_engine.getRoom();
	_from = from;
	_line.clear();
	_quitting = _talking = _done = false;
	clearButtons();
	// Two buttons, the eyes that look at them and the big mouth, all at
	// (0, 0).
	_yes = addButton(room.addEngineSprite(-1, "OBJ_QUIT_BUTTONS", "ANIM_QUIT_BUTTONSYES", 0, 0, 0, 1, "quit yes"));
	_no = addButton(room.addEngineSprite(-1, "OBJ_QUIT_BUTTONS", "ANIM_QUIT_BUTTONSNO", 0, 0, 0, 1, "quit no"));
	_eyes = room.addEngineSprite(-1, "OBJ_QUIT_BUTTONS", "ANIM_QUIT_BUTTONSEYES", 0, 0, 0, 1, "quit eyes");
	room.setPlayer(_eyes, 0, 0);
	_mouth = room.addEngineSprite(-1, "OBJ_QUIT_BIG_MOUTH", "ANIM_QUIT_BIG_MOUTHTALKS", 0, 0, 0, 1, "quit mouth");
	room.setPlayer(_mouth, 0, 0);
}

void QuitRoom::onRollover(int button, bool over) {
	// The eyes look at Yes (step 2) or No (step 1).
	_engine.getRoom()->setPlayer(_eyes, 0, over ? (button == _yes ? 2 : 1) : 0);
}

void QuitRoom::onClick(int button) {
	_quitting = button == _yes;
	_line = _quitting ? "Quit_Screen.Yes_1_SCOOBY" : "Quit_Screen.No_1_SCOOBY";
	_engine.getSound()->play(_line);
	_talking = true;
	debugC(1, kDebugScript, "quit: %s, Scooby says %s", _quitting ? "yes" : "no", _line.c_str());
}

void QuitRoom::tick() {
	ButtonRoom::tick();
	Sound &sound = *_engine.getSound();
	if (!_line.empty() && !_done && !sound.isClipPlaying(_line)) {
		// When the line ends: back where the menu was, or out of the game
		// (game +0x110).
		_done = true;
		debugC(1, kDebugScript, "quit: the line is over: %s", _quitting ? "quitting" : _from.c_str());
		if (_quitting)
			_engine.requestQuit();
		else
			_engine.exitTo(_from);
	}
	if (_talking) {
		uint shape;
		if (sound.getLipShape(_line, shape)) {
			if (shape)
				_engine.getRoom()->setPlayer(_mouth, 0, shape - 1);
		} else if (!sound.isClipPlaying(_line)) {
			_talking = false;
		}
	}
}

} // End of namespace Scooby
