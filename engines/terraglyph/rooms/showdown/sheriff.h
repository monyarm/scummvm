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

#ifndef TERRAGLYPH_SHERIFF_H
#define TERRAGLYPH_SHERIFF_H

#include "terraglyph/rooms/class.h"
#include "terraglyph/rooms/script.h"

namespace Terraglyph {

/**
 * Showdown's Sheriff's quiz, ROOM_Sheriff (0x416E20, vtable 0x45EA10): a
 * page of his notebook, read out with its four answers. Hovered, the page
 * or an answer lights up and is read again. A right answer, and the next
 * page; the level's count of them (level + 2) set SheriffPuzzle when the
 * gang leaves. A wrong one, and back to Street 1.
 */
class SheriffRoom : public RoomClass {
public:
	static const uint kQuestions = 15;
	static const uint kAnswers = 4;

	explicit SheriffRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                          // 0x416E80
	void enter(const Common::String &from) override;                                // 0x417220
	void roomLeft() override;                                                       // 0x4172C0
	void tick() override;                                                           // 0x416ED0
	bool click(int sprite) override;                                                // the answers' callback, 0x4176E0 (vtable +0x40)
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x417530
	void saveKeys(Common::INIFile &profile) override;                               // 0x4179B0
	void loadKeys(const Common::INIFile &profile) override;                         // 0x417A80
	// Game +0x24 (and the cursor faded), set every tick until the page and
	// its answers have been read.
	bool isBusy() const override { return !_free; }

	/** For a check: the next question drawn, as entry draws it. */
	void setQuestion(int q) { _question = q; }

	int getQuestion() const { return _question; }
	bool isAnswered(uint q) const { return _answered[q]; }
	uint getAnsweredCount() const;
	int getNotebook() const { return _notebook; }
	int getAnswer(uint i) const { return _answers[i]; }
	const Common::String &getAnswerSound(uint i) const { return _answerSounds[i]; }
	const Common::String &getQuestionSound() const { return _questionSound; }
	const Common::String &getCurrent() const { return _current; }
	const Common::String &getHoverSound() const { return _hoverSound; }
	int getHoverCountdown() const { return _hoverCountdown; }
	int getState() const { return _state; }
	bool isNext() const { return _next; }
	bool isDone() const { return _done; }
	bool isWrong() const { return _wrong; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	/** 0x417300: the page for _question, its answers and its sounds. */
	void setup();
	/** rand() % 15 until it isn't 2 (the last page), 5, or one answered. */
	void draw();
	/** 0x417B70: @p base and a digit, 9 down to 1 at random, the first the disc has; or none. */
	Common::String pick(const Common::String &base);
	/** The sound's id from its name (audio_GameSound_LookupId): none when the disc lacks it. */
	Common::String lookup(const Common::String &name) const;
	/** 0x434CE0: the sound isn't playing (none never is). */
	bool over(const Common::String &name) const;
	void play(const Common::String &name);
	/** graphics_AnimSprite_Play with mode 0: the sprite stopped on @p step. */
	void show(int sprite, uint step);

	int _question = -1;           // +0xB8: CurrentQuestion, its page less 1
	int _notebook = -1;           // +0xBC
	bool _answered[kQuestions] = {}; // +0xC0: Question0-14
	int _answers[kAnswers] = { -1, -1, -1, -1 }; // +0xD0: A to D
	Common::String _answerSounds[kAnswers];      // +0xE0
	Common::String _questionSound; // +0xF0
	Common::String _current;      // +0xF4: the sound waited for
	Common::String _hoverSound;   // +0xF8: what the mouse is over says
	int _hoverCountdown = 0;      // +0xFC
	bool _next = false;           // +0x100: the next page once the sound is over
	int _state = 0;               // +0x104: 0 the page read, 1-4 its answers, 5 free
	bool _done = false;           // +0x108: Street 1 once the sound is over
	bool _wrong = false;          // +0x109: the second wrong line, then Street 1
	bool _free = false;           // game +0x24 cleared this tick
	Common::String _lastSound;
};

/** The Sheriff's room class the factory builds for @p room, or nullptr. */
RoomClass *createSheriffRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_SHERIFF_H
