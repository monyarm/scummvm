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

#ifndef TERRAGLYPH_SIGNIN_H
#define TERRAGLYPH_SIGNIN_H

#include "terraglyph/rooms/class.h"

namespace Terraglyph {

/**
 * Sign In (Showdown 0x424770, vtable 0x45F024): the player types a name,
 * or picks one of the saved players, and plays. A known name loads its
 * profile; a new one starts a new game at the level select. The players
 * are the saved profiles: <name>.sav in the original, here the ScummVM
 * saves whose description is a player's name.
 */
class SignInRoom : public ButtonRoom {
public:
	static const uint kMaxName = 10;
	static const uint kRows = 4;

	explicit SignInRoom(TerraglyphEngine &engine) : ButtonRoom(engine) {}
	void enter(const Common::String &from) override;
	void tick() override;
	bool click(int sprite) override;
	bool skip() override;
	bool key(uint16 ascii, int keycode) override;
	/** Leaving: input is held and the cursor hidden (game +0x24). */
	bool isBusy() const override { return !_leaving.empty(); }

	const Common::String &getName() const { return _name; }
	/** The saved players, in list order, and their save slots. */
	const Common::Array<Common::String> &getPlayers() const { return _players; }
	int getPlayerSlot(uint i) const { return _slots[i]; }
	int getSelected() const { return _selected; }
	int getHovered() const { return _hovered; }
	int getScroll() const { return _scroll; }
	int getIdle() const { return _idle; }
	int getCountdown() const { return _countdown; }
	bool isTyping() const { return _typing; }
	const Common::String &getLeavingLine() const { return _leaving; }
	/** The field showing the typed name (row -1), or a row of the list. */
	int getFieldBox(int row) const { return field(row).box; }
	int getFieldHighlight(int row) const { return field(row).highlight; }
	int getFieldChar(int row, uint i) const { return field(row).chars[i]; }
	const Common::String &getFieldText(int row) const { return field(row).text; }
	/** The list's top left, and the typed field's. */
	int getListX() const { return _listX; }
	int getListY() const { return _listY; }
	int getFieldX() const { return _fieldX; }
	int getFieldY() const { return _fieldY; }
	int getArrowSprite(bool up) const;
	int findButtonFor(const Common::String &object, const Common::String &anim) const;
	/** The list under the mouse at (x, y), and a click there (0x426280). */
	void mouseOnList(int x, int y, bool click);
	/** The letters the character buttons type, in placement order. */
	Common::String getLetters() const;
	/** For the checks: the list without reading the saves. */
	void setPlayers(const Common::Array<Common::String> &names, const Common::Array<int> &slots) {
		_players = names;
		_slots = slots;
	}
	/** Whether a player's name can be shown and typed: 1-10 of A-Z, 0-9 and space. */
	static bool isPlayerName(const Common::String &name);

protected:
	void onClick(int button) override;

private:
	// A text field (0x424340): a 240 x 30 box under layer 0, its highlight,
	// and ten letters of OBJ_SIGN_IN_FONT.
	struct Field {
		int box = -1;
		int highlight = -1;
		int chars[kMaxName] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
		Common::String text;   // +0x84
	};

	const Field &field(int row) const { return row < 0 ? _typed : _rows[row]; }
	void buildField(Field &f, int x, int y, const Common::String &label);
	void showText(Field &f, const Common::String &text);     // 0x4244F0
	void setRowState(Field &f, bool selected, bool hovered); // 0x424660, 0x424680
	void refreshRows();                                      // 0x426130
	void readKeys();                                         // 0x4264B0
	void play();                                             // 0x425C70
	void quit();                                             // 0x425E00
	void addSpace();                                         // 0x425EF0
	void backspace();                                        // 0x425E70
	void clickSound();                                       // Sign_In.Click
	void leave(const Common::String &to);                    // 0x425210
	void deleteSelected();
	void loadPlayers();                                      // 0x426630

	Common::String _name;                   // +0x104
	Common::Array<Common::String> _players; // +0xDC, +0xE0
	Common::Array<int> _slots;
	Field _rows[kRows];                     // +0xE8
	Field _typed;                           // +0xE4
	int _selected = -1;                     // +0x10C
	int _hovered = -1;                      // +0x110
	int _scroll = 0;                        // +0x114
	uint32 _lastClick = 0;                  // +0x118
	int _idle = 0;                          // +0xC4
	int _countdown = 0;                     // +0xCC
	bool _typing = false;                   // +0x100
	Common::String _leaving;                // +0xC8
	Common::String _prompt;                 // +0xD0
	int _listX = 0, _listY = 0;             // +0xD4
	int _fieldX = 0, _fieldY = 0;           // +0xF8
	int _play = -1, _quit = -1, _up = -1, _down = -1, _right = -1, _left = -1;
	Common::Array<char> _letters;           // a letter per button, 0 for the others
	Common::Array<uint16> _keys;            // typed, not read yet
	bool _clicked = false;                  // a click not on a button, this tick
	bool _deleteKey = false;
};

} // End of namespace Terraglyph

#endif // TERRAGLYPH_SIGNIN_H
