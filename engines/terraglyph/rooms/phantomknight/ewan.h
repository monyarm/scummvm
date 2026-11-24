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

#ifndef TERRAGLYPH_EWAN_ROOMS_H
#define TERRAGLYPH_EWAN_ROOMS_H

#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"

namespace Terraglyph {

/**
 * Phantom's Ewan's quiz, ROOM_Ewan (0x4109D0, vtable 0x45E890), the
 * Sheriff's notebook quiz on a chalkboard: a page read out with its four
 * answers; hovered, the page or an answer lights up and is read again. A
 * right answer, and the eraser wipes the board for the next page; the
 * level's count of them (level + 3) back to the kitchen, and EwanPuzzle
 * as the gang leaves. A wrong one, two lines and back to the kitchen.
 */
class EwanRoom : public RoomClass {
public:
	static const uint kQuestions = 12;
	static const uint kAnswers = 4;

	explicit EwanRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x410A30
	void enter(const Common::String &from) override;                                // 0x410F20
	void roomLeft() override;                                                       // 0x410FB0
	void tick() override;                                                           // 0x410A70
	bool click(int sprite) override;                                                // the answers' callback, 0x411480 (vtable +0x40)
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x411210
	void saveKeys(Common::INIFile &profile) override;                               // 0x411780
	void loadKeys(const Common::INIFile &profile) override;                         // 0x411850
	// Game +0x24 (and the cursor faded), set every tick until the page and
	// its answers have been read.
	bool isBusy() const override { return !_free; }

	/** For a check: the next question drawn, as entry draws it. */
	void setQuestion(int q) { _question = q; }
	void setAnsweredForCheck(uint q, bool a) { _answered[q] = a; }

	int getQuestion() const { return _question; }
	bool isAnswered(uint q) const { return _answered[q]; }
	uint getAnsweredCount() const;
	int getBoard() const { return _board; }
	int getEraser() const { return _eraser; }
	int getAnswer(uint i) const { return _answers[i]; }
	const Common::String &getAnswerSound(uint i) const { return _answerSounds[i]; }
	const Common::String &getQuestionSound() const { return _questionSound; }
	const Common::String &getCurrent() const { return _current; }
	const Common::String &getHoverSound() const { return _hoverSound; }
	int getHoverCountdown() const { return _hoverCountdown; }
	int getState() const { return _state; }
	bool isErasing() const { return _erasing; }
	bool isDone() const { return _done; }
	bool isWrong() const { return _wrong; }

private:
	/** 0x410FF0: the board and answers for _question, and their sounds. */
	void setup();
	/** rand() % 12 until it isn't 1, then until it isn't 4 nor one answered. */
	void draw();
	/** 0x411940: @p base, a space, the space a digit 9 down to 1 at random, the first the disc has; or none. */
	Common::String pick(const Common::String &base);
	/** The sound's id from its name: none when the disc lacks it. */
	Common::String lookup(const Common::String &name) const;
	/** 0x435A10: the sound isn't playing (none never is). */
	bool over(const Common::String &name) const;
	void play(const Common::String &name);
	/** 0x4317B0 with mode 0: the sprite stopped on @p step. */
	void show(int sprite, uint step);

	int _board = -1;              // +0xB8: OBJ_EWAN_BOARD
	int _eraser = -1;             // +0xBC: OBJ_EWAN_ERASE
	int _answers[kAnswers] = { -1, -1, -1, -1 }; // +0xC0: OBJ_EWAN_ANSWERA-D
	Common::String _answerSounds[kAnswers];      // +0xD0
	Common::String _questionSound; // +0xE0
	Common::String _current;      // +0xE4: the sound waited for
	Common::String _hoverSound;   // +0xE8: what the mouse is over says
	int _hoverCountdown = 0;      // +0xEC
	bool _erasing = false;        // +0xF0: the next page once the eraser stops
	int _question = -1;           // +0xF4: CurrentQuestion, its page less 1
	bool _answered[kQuestions] = {}; // +0xF8: Question0-11
	int _state = 0;               // +0x104: 0 the page read, 1-4 its answers, 5 free
	bool _done = false;           // +0x108: the kitchen once the sound is over
	bool _wrong = false;          // +0x109: the second wrong line, then the kitchen
	bool _free = false;           // game +0x24 cleared this tick
};

/** The class the factory builds for @p room, or nullptr. */
RoomClass *createEwanRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_EWAN_ROOMS_H
