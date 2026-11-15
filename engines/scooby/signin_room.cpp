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

// Sign In. The addresses are Showdown's; Phantom's class is the same code
// with its own string addresses.

#include "common/debug.h"
#include "common/keyboard.h"
#include "common/system.h"

#include "scooby/detection.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/signin_room.h"
#include "scooby/sound.h"

namespace Scooby {

static const int kIdleTicks = 1080;        // 0x438: 30 seconds
static const int kPromptTicks = 90;        // 0x5A
static const int kTypingTicks = 30;        // 0x1E: typing starts this close to the prompt
static const int kFieldWidth = 240;        // 0xF0
static const int kRowHeight = 30;          // 0x1E
static const int kFieldHitWidth = 244;     // 0xF4: the typed field's click box
static const int kSpaceWidth = 10;         // 0x45F020
static const int kLetterGap = 2;           // 0x45EB18
static const uint32 kDoubleClick = 1000;   // ms

static const char *const kClick = "Sign_In.Click";
static const char *const kQuitLine = "Sign_In.Quit_Button";
static const char *const kPlayLine = "Sign_In.SHAGGY.Click_Play";
static const char *const kPromptLine = "Sign_In.SHAGGY.Type_Name";
static const char *const kIdleLine = "Sign_In.SHAGGY.Type_Name_Idle";
static const char *const kSavedLine = "Sign_In.SHAGGY.Saved_Game";
static const char *const kCharset = "ANIM_SIGN_IN_FONTCHARSET";
static const char *const kHighlighted = "ANIM_SIGN_IN_FONTHIGHLIGHTED";

bool SignInRoom::isPlayerName(const Common::String &name) {
	if (name.empty() || name.size() > kMaxName)
		return false;
	for (uint i = 0; i < name.size(); i++) {
		const char c = name[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' '))
			return false;
	}
	return true;
}

void SignInRoom::loadPlayers() {
	// 0x426630 lists <savepath>\*.sav, each name without ".sav".
	_engine.listPlayers(_players, _slots);
}

// ---------------------------------------------------------------------------
// Entry (0x425050) and the placements (0x425560)

void SignInRoom::enter(const Common::String &from) {
	Room &room = *_engine.getRoom();
	// The base entry has run, with the placements: these are the ones
	// 0x425560 builds itself. The characters are buttons in a 560 x 125 box
	// at (0, 350), which leaves them where they are placed; Play, Quit and
	// the arrows are buttons; the two text fields' placements only say
	// where the fields go, and aren't drawn.
	clearButtons();
	_letters.clear();
	_play = _quit = _up = _down = _right = _left = -1;
	_listX = _listY = _fieldX = _fieldY = 0;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		const Common::String &anim = room.getAnim(s);
		int b = -1;
		char letter = 0;
		if (obj == "OBJ_SIGN_IN_CHARACTERS") {
			b = addButton(s);
			// 0x425B90 types the last letter of the button's ANIM.
			letter = anim.empty() ? 0 : anim.lastChar();
		} else if (obj == "OBJ_SIGN_IN_PLAY") {
			b = _play = addButton(s);
		} else if (obj == "OBJ_SIGN_IN_QUIT") {
			b = _quit = addButton(s);
		} else if (obj == "OBJ_SIGN_IN_U_ARROW") {
			b = _up = addButton(s);
		} else if (obj == "OBJ_SIGN_IN_D_ARROW") {
			b = _down = addButton(s);
		} else if (obj == "OBJ_SIGN_IN_R_ARROW") {
			b = _right = addButton(s);
		} else if (obj == "OBJ_SIGN_IN_L_ARROW") {
			b = _left = addButton(s);
		} else if (anim == "ANIM_SIGN_IN_TEXT_FIELDA") {
			_fieldX = room.getX(s);
			_fieldY = room.getY(s);
			room.setHidden(s, true);
		} else if (anim == "ANIM_SIGN_IN_TEXT_FIELDNAMES") {
			_listX = room.getX(s);
			_listY = room.getY(s);
			room.setHidden(s, true);
		}
		if (b >= 0) {
			_letters.resize(b + 1);
			_letters[b] = letter;
		}
	}

	_idle = kIdleTicks;
	_leaving.clear();
	_prompt.clear();
	_countdown = kPromptTicks;
	_typing = false;
	_keys.clear();
	_clicked = _deleteKey = false;
	loadPlayers();
	for (uint i = 0; i < kRows; i++)
		buildField(_rows[i], _listX, _listY + kRowHeight * i, Common::String::format("sign in row %u", i));
	buildField(_typed, _fieldX, _fieldY, "sign in name");
	_scroll = 0;
	_selected = _hovered = -1;
	_name.clear();
	_lastClick = 0;
	debugC(1, kDebugScript, "sign in: %u players, list at %d,%d, name at %d,%d, %u buttons", _players.size(), _listX, _listY,
	       _fieldX, _fieldY, _buttons.size());
}

void SignInRoom::buildField(Field &f, int x, int y, const Common::String &label) {
	// 0x424340: the box under the room's first layer (game +0x58) at depth
	// 0; the highlight at depth 0 and the letters at depth 1 in it, all
	// hidden.
	Room &room = *_engine.getRoom();
	f.box = room.addGroupSprite(room.getLayerSprite(0), 0, x, y, label);
	f.highlight = room.addEngineSprite(f.box, "OBJ_SIGN_IN_TEXT_FIELD", "ANIM_SIGN_IN_TEXT_FIELDHIGHLIGHT", 0, 0, 0, 1, label + " highlight");
	room.setHidden(f.highlight, true);
	for (uint i = 0; i < kMaxName; i++) {
		f.chars[i] = room.addEngineSprite(f.box, "OBJ_SIGN_IN_FONT", kCharset, 1, 0, 0, 1, label + " letter");
		room.setHidden(f.chars[i], true);
	}
	f.text.clear();
}

void SignInRoom::showText(Field &f, const Common::String &text) {
	// 0x4244F0: each letter's step is its value ('0'-'9' are 0-9, 'A'-'Z'
	// are 10-35), at 1 + the widths before it, its first cel's width and 2
	// apart; a space is hidden and 10 wide. Then the whole line moves to
	// the middle of the 240-pixel box.
	if (text == f.text)
		return;
	f.text = text;
	Room &room = *_engine.getRoom();
	int pos = 0;
	int x[kMaxName];
	uint n = 0;
	for (; n < text.size() && n < kMaxName; n++) {
		const char c = text[n];
		x[n] = 0;
		if (c == ' ') {
			room.setHidden(f.chars[n], true);
			pos += kSpaceWidth + kLetterGap;
			continue;
		}
		room.setHidden(f.chars[n], false);
		x[n] = pos + 1;
		room.setPlayer(f.chars[n], 0, c < 'A' ? c - '0' : c - 0x37);
		pos += room.getFirstCelWidth(f.chars[n]) + kLetterGap;
	}
	for (uint i = n; i < kMaxName; i++)
		room.setHidden(f.chars[i], true);
	// The positions are floats; the shift is (240 - width) * 0.5, and the
	// drawing truncates.
	for (uint i = 0; i < n; i++)
		room.setOffset(f.chars[i], (2 * x[i] + kFieldWidth - pos) / 2, 1);
}

void SignInRoom::setRowState(Field &f, bool selected, bool hovered) {
	// 0x424660 shows the highlight on the selected row; 0x424680 switches
	// the letters of the row under the mouse to the highlighted font, each
	// on the step it shows.
	Room &room = *_engine.getRoom();
	room.setHidden(f.highlight, !selected);
	const char *anim = hovered ? kHighlighted : kCharset;
	if (room.getAnim(f.chars[0]) == anim)
		return;
	for (uint i = 0; i < kMaxName; i++)
		room.startAnim(f.chars[i], anim, 0, room.getStep(f.chars[i]));
}

void SignInRoom::refreshRows() {
	// 0x426130: the four rows from the scroll position, then the arrows.
	Room &room = *_engine.getRoom();
	const int count = _players.size();
	for (int i = 0; i < (int)kRows; i++) {
		const int p = _scroll + i;
		showText(_rows[i], p >= 0 && p < count ? _players[p] : Common::String());
		setRowState(_rows[i], _selected == i, _hovered == i);
	}
	const bool up = count != 0 && (_scroll != 0 || _selected > 0);
	// Down shows while the selection can move down, or the list scroll.
	bool down = false;
	if (count != 0)
		down = (_selected != -1 && _scroll + _selected < count - 1) || _scroll < count - 4;
	if (_up >= 0)
		room.setHidden(_buttons[_up].sprite, !up);
	if (_down >= 0)
		room.setHidden(_buttons[_down].sprite, !down);
}

int SignInRoom::getArrowSprite(bool up) const {
	const int b = up ? _up : _down;
	return b >= 0 ? _buttons[b].sprite : -1;
}

Common::String SignInRoom::getLetters() const {
	Common::String letters;
	for (uint b = 0; b < _letters.size(); b++) {
		if (_letters[b])
			letters += _letters[b];
	}
	return letters;
}

int SignInRoom::findButtonFor(const Common::String &object, const Common::String &anim) const {
	Room &room = *_engine.getRoom();
	for (uint b = 0; b < _buttons.size(); b++) {
		if (room.getObjectName(_buttons[b].sprite) == object && (anim.empty() || room.getAnim(_buttons[b].sprite) == anim))
			return _buttons[b].sprite;
	}
	return -1;
}

// ---------------------------------------------------------------------------
// The tick (0x424900)

void SignInRoom::tick() {
	ButtonRoom::tick();
	Sound &sound = *_engine.getSound();
	// Nothing is read while leaving, or while an exit runs (room +0x2C).
	if (_leaving.empty() && !_engine.getScript()->isBusy()) {
		// Every 30 seconds without a key, Shaggy asks again; with players
		// saved, half the time he asks for a saved game.
		if (--_idle < 1) {
			_idle = kIdleTicks;
			const char *line = _players.empty() || _engine.getRandomNumber(1) ? kIdleLine : kSavedLine;
			debugC(1, kDebugScript, "sign in: idle, Shaggy says %s", line);
			sound.play(line);
		}
		// 2.5 seconds in, the prompt; the keys pressed before it are lost.
		if (--_countdown == 0) {
			_prompt = kPromptLine;
			sound.play(_prompt);
			_keys.clear();
			_typing = true;
			debugC(1, kDebugScript, "sign in: Shaggy asks for a name");
		}
		if (_countdown < kTypingTicks)
			_typing = true;
		else
			_keys.clear();
		if (_typing)
			readKeys();
		const Common::Point mouse = _engine.getMousePos();
		mouseOnList(mouse.x, mouse.y, _clicked);
		// A click on the typed name lets go of the list's choice.
		if (_clicked && _fieldX <= mouse.x && _fieldY <= mouse.y && mouse.x < _fieldX + kFieldHitWidth &&
		    mouse.y < _fieldY + kRowHeight)
			_selected = -1;
	}
	_clicked = false;

	// When the line Play or Quit started ends, leave.
	if (!_leaving.empty() && !sound.isClipPlaying(_leaving)) {
		const bool toQuit = _leaving == kQuitLine;
		_leaving.clear();
		leave(toQuit ? "ROOM_Quit" : "ROOM_Difficulty");
	}

	// A changed name shows, and the list lets go.
	if (_name != _typed.text) {
		showText(_typed, _name);
		_selected = -1;
	}
	refreshRows();

	// A key with a row selected deletes that player's profile. The
	// original's key isn't traced; here it is Delete.
	if (_deleteKey && _selected != -1)
		deleteSelected();
	_deleteKey = false;
}

void SignInRoom::readKeys() {
	// 0x4264B0: backspace, space, letters and digits (up to 10, made
	// upper case), and Enter plays. Every key resets the idle time.
	for (uint k = 0; k < _keys.size(); k++) {
		char c = (char)_keys[k];
		if (c == '\b') {
			if (!_name.empty())
				_name.deleteLastChar();
			clickSound();
			_idle = kIdleTicks;
		} else if (c == ' ') {
			addSpace();
		} else {
			if (_name.size() < kMaxName) {
				if (c >= 'a' && c <= 'z')
					c -= 0x20;
				if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
					_name += c;
					clickSound();
				}
			}
			if (c == '\r')
				play();
		}
		_idle = kIdleTicks;
	}
	_keys.clear();
}

void SignInRoom::mouseOnList(int x, int y, bool click) {
	// 0x426280: the rows holding a player, 30 pixels apart, take the mouse
	// strictly inside their top and bottom. A click selects a row; a second
	// one on it within a second plays.
	int hovered = -1;
	if (_listX <= x && _listY <= y && x < _listX + kFieldWidth && y < _listY + kRowHeight * (int)kRows) {
		int top = _listY;
		for (int p = _scroll, row = 0; p < (int)_players.size(); p++) {
			if (top < y && y < top + kRowHeight) {
				hovered = row;
				if (click) {
					const uint32 now = g_system->getMillis();
					if (_selected == row && now < _lastClick + kDoubleClick) {
						debugC(1, kDebugScript, "sign in: %s clicked twice", _players[p].c_str());
						play();
						_hovered = row;
						return;
					}
					debugC(1, kDebugScript, "sign in: %s selected", _players[p].c_str());
					_selected = row;
					_lastClick = now;
					_hovered = row;
					return;
				}
				break;
			}
			top += kRowHeight;
			if (++row > 3)
				break;
		}
	}
	_hovered = hovered;
}

void SignInRoom::deleteSelected() {
	const int p = _scroll + _selected;
	if (p < 0 || p >= (int)_players.size())
		return;
	debugC(1, kDebugScript, "sign in: deleting %s (slot %d)", _players[p].c_str(), _slots[p]);
	_engine.deletePlayer(_slots[p]);
	const int count = _players.size();
	_players.remove_at(p);
	_slots.remove_at(p);
	_selected = -1;
	if (count - 5 < _scroll && count - 1 > 3)
		_scroll--;
}

// ---------------------------------------------------------------------------
// Input

bool SignInRoom::click(int sprite) {
	if (isBusy())
		return false;
	if (ButtonRoom::click(sprite))
		return true;
	// The list and the typed field read the mouse on the tick.
	_clicked = true;
	return true;
}

bool SignInRoom::key(uint16 ascii, int keycode) {
	// The keys go to the tick; Escape, and Space before typing starts,
	// skip.
	if (keycode == Common::KEYCODE_ESCAPE)
		return false;
	if (keycode == Common::KEYCODE_DELETE) {
		_deleteKey = true;
		return true;
	}
	if (!ascii || ascii > 0x7F)
		return false;
	_keys.push_back(ascii);
	return ascii != ' ' || _typing;
}

bool SignInRoom::skip() {
	// The skip key: the prompt stops (or never starts), typing starts at
	// once, and a leaving line ends.
	Sound &sound = *_engine.getSound();
	if (_prompt.empty())
		_prompt = kPromptLine;
	else
		sound.stopClip(_prompt);
	_countdown = 0;
	_typing = true;
	if (!_leaving.empty())
		sound.stopClip(_leaving);
	debugC(1, kDebugScript, "sign in: skipped");
	return true;
}

void SignInRoom::clickSound() {
	Sound &sound = *_engine.getSound();
	sound.stopClip(kClick);
	sound.play(kClick);
}

void SignInRoom::onClick(int button) {
	if (button == _play) {
		play();
	} else if (button == _quit) {
		quit();
	} else if (button == _up) {
		// 0x4260D0
		if (_selected < 1)
			_scroll--;
		else
			_selected--;
		clickSound();
	} else if (button == _down) {
		// 0x426070
		if (_selected == -1 || _selected > 2)
			_scroll++;
		else
			_selected++;
		clickSound();
	} else if (button == _right) {
		addSpace();
	} else if (button == _left) {
		backspace();
	} else if (button < (int)_letters.size() && _letters[button]) {
		// 0x425B90
		if (_name.size() < kMaxName) {
			_name += _letters[button];
			clickSound();
		}
		_idle = kIdleTicks;
	}
}

void SignInRoom::addSpace() {
	// 0x425EF0: not first, not twice.
	if (_name.size() < kMaxName && !_name.empty() && _name.lastChar() != ' ') {
		_name += ' ';
		clickSound();
		_idle = kIdleTicks;
	}
}

void SignInRoom::backspace() {
	// 0x425E70
	if (!_name.empty())
		_name.deleteLastChar();
	clickSound();
	_idle = kIdleTicks;
}

void SignInRoom::play() {
	// 0x425C70: the selected player, else the typed name. With a name,
	// Shaggy's line plays and the room leaves when it ends; without one,
	// he asks for a name, unless he is asking already.
	Sound &sound = *_engine.getSound();
	_idle = kIdleTicks;
	const int p = _scroll + _selected;
	if (_selected != -1 && p >= 0 && p < (int)_players.size())
		_name = _players[p];
	if (!_name.empty()) {
		if (_name.lastChar() == ' ')
			_name.deleteLastChar();
		_leaving = kPlayLine;
		sound.stop();
		sound.play(_leaving);
		_selected = _hovered = -1;
		_engine.getRoom()->setCursor("");
		debugC(1, kDebugScript, "sign in: play as %s", _name.c_str());
	} else if (!sound.isClipPlaying(kIdleLine)) {
		sound.stop();
		sound.play(kIdleLine);
	}
}

void SignInRoom::quit() {
	// 0x425E00
	Sound &sound = *_engine.getSound();
	_leaving = kQuitLine;
	_idle = kIdleTicks;
	sound.stop();
	sound.play(_leaving);
	_engine.getRoom()->setCursor("");
	debugC(1, kDebugScript, "sign in: quit");
}

void SignInRoom::leave(const Common::String &to) {
	// 0x425210, the class's exit: anywhere but Quit starts a new game
	// (game vtable +0x4C, 0x435780, +0x50) for the name; a saved player's
	// profile then loads (+0x58) and goes to its StartRoom.
	_countdown = 0;
	if (to != "ROOM_Quit") {
		_engine.newGame();
		int slot = -1;
		for (uint i = 0; i < _players.size(); i++) {
			if (_players[i] == _name) {
				slot = _slots[i];
				break;
			}
		}
		_engine.signIn(_name, slot);
		if (slot >= 0) {
			debugC(1, kDebugScript, "sign in: %s is saved in slot %d; loading it", _name.c_str(), slot);
			_engine.requestLoad(slot);
			return;
		}
		debugC(1, kDebugScript, "sign in: %s is a new player", _name.c_str());
	}
	_engine.exitTo(to);
}

} // End of namespace Scooby
