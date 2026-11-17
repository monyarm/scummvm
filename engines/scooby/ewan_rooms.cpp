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

// Phantom of the Knight's Ewan's quiz. The addresses are Phantom's.

#include "common/debug.h"

#include "scooby/detection.h"
#include "scooby/ewan_rooms.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kEwanRoom = "ROOM_Ewan";

// 0x411480's right answers, by page (pages 2 and 5 are never drawn).
static const char *const kRight[] = {
	"ANIM_EWAN_ANSWERB1", "ANIM_EWAN_ANSWERB2", "ANIM_EWAN_ANSWERA3", "ANIM_EWAN_ANSWERD4", "ANIM_EWAN_ANSWERB5", "ANIM_EWAN_ANSWERA6",
	"ANIM_EWAN_ANSWERB7", "ANIM_EWAN_ANSWERA8", "ANIM_EWAN_ANSWERC9", "ANIM_EWAN_ANSWERA10", "ANIM_EWAN_ANSWERA11", "ANIM_EWAN_ANSWERB12"
};

void EwanRoom::reset() {
	// 0x410A30, after the base reset.
	for (uint q = 0; q < kQuestions; q++)
		_answered[q] = false;
	_current.clear();
	_erasing = false;
	_question = -1;
	_hoverSound.clear();
}

uint EwanRoom::getAnsweredCount() const {
	uint n = 0;
	for (uint q = 0; q < kQuestions; q++)
		n += _answered[q] ? 1 : 0;
	return n;
}

Common::String EwanRoom::lookup(const Common::String &name) const {
	return _engine.getSound()->findClip(name) ? name : Common::String();
}

bool EwanRoom::over(const Common::String &name) const {
	return name.empty() || !_engine.getSound()->isClipPlaying(name);
}

void EwanRoom::play(const Common::String &name) {
	if (!name.empty())
		_engine.getSound()->play(name);
}

void EwanRoom::show(int sprite, uint step) {
	if (sprite >= 0)
		_engine.getRoom()->setPlayer(sprite, 0, step);
}

void EwanRoom::draw() {
	// 0x410F20 and 0x410A70: never page 2, then never page 5 nor one
	// answered.
	int q;
	do {
		do
			q = _engine.getRandomNumber(kQuestions - 1);
		while (q == 1);
	} while (q == 4 || _answered[q]);
	_question = q;
}

Common::String EwanRoom::pick(const Common::String &base) {
	// 0x411940: the base and a space; the space a digit '1' + rand() % n, n
	// from 9, then the digit drawn, until one is on the disc or n is 0.
	Common::String name = base + " ";
	uint n = 9;
	do {
		n = _engine.getRandomNumber(n - 1);
		name.setChar('1' + n, name.size() - 1);
		if (_engine.getSound()->findClip(name))
			return name;
	} while (n != 0);
	return Common::String();
}

void EwanRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x411210. The board a sprite (0x4314C0) under layer 0 at depth 0,
	// hidden, stopped on step 0. Each answer (OBJ_EWAN_ANSWER*, kept by its
	// name's letter) a requires object (0x408C80 with -1) at depth 10,
	// hidden, stopped, 0x411480 its callback (0x402FF0). The eraser a sprite
	// at depth 100, hidden and stopped. The rest the default path builds.
	if (object == "OBJ_EWAN_BOARD") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
		claim.hidden = true;
	} else if (object.hasPrefix("OBJ_EWAN_ANSWER")) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.depth = 10;
		claim.hidden = true;
	} else if (object == "OBJ_EWAN_ERASE") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 100;
		claim.hidden = true;
	}
}

void EwanRoom::setup() {
	// 0x410FF0. The board on ANIM_EWAN_BOARDPAGE<page>, stopped on step 0
	// and shown, its sound Ewan.NOTEBOOKPAGE<page>; each answer on
	// ANIM_EWAN_ANSWER<letter><page>, stopped and shown, with its sound
	// Ewan.NOTEBOOKPAGE<page>_<letter>.
	Room &room = *_engine.getRoom();
	const int page = _question + 1;
	if (_board >= 0) {
		room.startAnim(_board, Common::String::format("ANIM_EWAN_BOARDPAGE%i", page), 1, -1);
		show(_board, 0);
		room.setHidden(_board, false);
	}
	_questionSound = lookup(Common::String::format("Ewan.NOTEBOOKPAGE%i", page));
	for (uint i = 0; i < kAnswers; i++) {
		if (_answers[i] >= 0) {
			room.startAnim(_answers[i], Common::String::format("ANIM_EWAN_ANSWER%c%d", 'A' + i, page), 0, -1);
			room.setHidden(_answers[i], false);
		}
		_answerSounds[i] = lookup(Common::String::format("Ewan.NOTEBOOKPAGE%d_%c", page, 'A' + i));
	}
	_state = 0;
}

void EwanRoom::enter(const Common::String &from) {
	// 0x410F20, after the base entry: the toolbar off (0x407CC0, in the
	// toolbar's hidden list), nothing wrong or done, no sound waited for,
	// input held. No question yet: one drawn. Then its page.
	Room &room = *_engine.getRoom();
	_done = false;
	_wrong = false;
	_current.clear();
	_free = false;
	_board = _eraser = -1;
	for (uint i = 0; i < kAnswers; i++)
		_answers[i] = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &obj = room.getObjectName(s);
		if (obj == "OBJ_EWAN_BOARD")
			_board = s;
		else if (obj == "OBJ_EWAN_ERASE")
			_eraser = s;
		else if (obj.hasPrefix("OBJ_EWAN_ANSWER") && !obj.empty()) {
			const int i = obj.lastChar() - 'A';
			if (i >= 0 && i < (int)kAnswers)
				_answers[i] = s;
		}
	}
	if (_board >= 0)
		show(_board, 0);
	for (uint i = 0; i < kAnswers; i++)
		show(_answers[i], 0);
	if (_eraser >= 0)
		room.setPlayer(_eraser, 0, -1);
	if (_question == -1)
		draw();
	setup();
	debugC(1, kDebugScript, "Ewan: page %d, %u answered", _question + 1, getAnsweredCount());
}

void EwanRoom::roomLeft() {
	// 0x410FB0, after the base leave: the level's count answered (level +
	// 3), his quiz won (EwanPuzzle, game +0x254).
	if (getAnsweredCount() == (uint)(_engine.getDifficulty() + 3))
		_engine.setGameFlag("EwanPuzzle", true);
}

bool EwanRoom::click(int sprite) {
	// The answers' callback (0x411480, room vtable +0x40), from their click
	// (0x402F20) with the arrow: the answer pressed (step 2), the sound
	// waited for and the hovered one stopped. Right: the page answered, a
	// RIGHT_ line, the eraser shown and played once with Ewan.Erase_Board.
	// Wrong: a WRONG_1. line, then the second and the kitchen. Either stops
	// what the SFX and speech files play first (0x435B40), and holds input.
	if (sprite < 0 || !_engine.getInventory()->isDefaultCursor())
		return false;
	int i = -1;
	for (uint k = 0; k < kAnswers; k++) {
		if (_answers[k] == sprite)
			i = k;
	}
	if (i < 0)
		return false;
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	show(sprite, 2);
	if (!_current.empty())
		sound.stopClip(_current);
	if (!_hoverSound.empty())
		sound.stopClip(_hoverSound);
	bool right = false;
	for (uint k = 0; k < ARRAYSIZE(kRight); k++)
		right = right || room.getAnim(sprite) == kRight[k];
	const int page = _question + 1;
	if (right) {
		_answered[_question] = true;
		const Common::String line = pick("Ewan.NOTEBOOKPAGE_ANSWER_RIGHT_");
		sound.stopEffects();
		_current = line;
		play(line);
		_erasing = true;
		if (_eraser >= 0) {
			room.setHidden(_eraser, false);
			room.setPlayer(_eraser, 3, -1);
		}
		play("Ewan.Erase_Board");
		debugC(1, kDebugScript, "Ewan: answer %c on page %d right, '%s'", 'A' + i, page, line.c_str());
	} else {
		const Common::String line = pick("Ewan.NOTEBOOKPAGE_ANSWER_WRONG_1.");
		sound.stopEffects();
		_current = line;
		play(line);
		_wrong = true;
		debugC(1, kDebugScript, "Ewan: answer %c on page %d wrong, '%s'", 'A' + i, page, line.c_str());
	}
	_free = false;
	return true;
}

void EwanRoom::tick() {
	// 0x410A70, after the base tick. Input held and the cursor faded.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	_free = false;
	// The eraser played out: the next page, the eraser hidden.
	if (_erasing) {
		if (_eraser >= 0 && room.getMode(_eraser) != 0)
			return;
		draw();
		setup();
		if (_eraser >= 0)
			room.setHidden(_eraser, true);
		_erasing = false;
	}
	// The level's count answered (level + 3) and the eraser stopped: the
	// board and answers hidden, and done.
	if (getAnsweredCount() == (uint)(_engine.getDifficulty() + 3) && (_eraser < 0 || room.getMode(_eraser) == 0)) {
		for (uint k = 0; k < kAnswers; k++) {
			if (_answers[k] >= 0)
				room.setHidden(_answers[k], true);
		}
		if (_board >= 0)
			room.setHidden(_board, true);
		_done = true;
	}
	// The rest waits for the sound and the opening (room +0x10).
	if (!over(_current) || !_engine.getScript()->getMemory(kEwanRoom).openingPlayed)
		return;
	// Done: the kitchen (game vtable +0x04).
	if (_done) {
		_engine.requestRoom("ROOM_P13_Kitchen");
		return;
	}
	// Wrong: the second line, then done.
	if (_wrong) {
		_current = lookup("Ewan.NOTEBOOKPAGE_ANSWER_WRONG_2");
		play(_current);
		_wrong = false;
		_done = true;
		return;
	}
	switch (_state) {
	case 0:
		// The page read, lit up.
		sound.stopClip(_current);
		_current = _questionSound;
		play(_current);
		show(_board, 1);
		_state++;
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		// Each answer read in turn, lit up.
		show(_board, 0);
		for (uint k = 0; k < kAnswers; k++)
			show(_answers[k], 0);
		show(_answers[_state - 1], 1);
		_current = _answerSounds[_state - 1];
		play(_current);
		_state++;
		break;
	case 5: {
		// Free. What the mouse is over lights up and, 36 ticks on and every
		// 72 after its sound, says itself again.
		_current.clear();
		_free = true;
		const int hover = _engine.getHover();
		bool idle = true;
		if (hover != _board || _board < 0) {
			show(_board, 0);
			if (_questionSound == _hoverSound) {
				sound.stopClip(_hoverSound);
				_hoverSound.clear();
			}
		} else {
			if (_hoverSound != _questionSound) {
				show(_board, 1);
				_hoverCountdown = 36;
				_hoverSound = _questionSound;
			}
			idle = false;
		}
		for (uint k = 0; k < kAnswers; k++) {
			if (hover != _answers[k] || _answers[k] < 0) {
				show(_answers[k], 0);
				if (_answerSounds[k] == _hoverSound) {
					sound.stopClip(_hoverSound);
					_hoverSound.clear();
				}
			} else {
				show(_answers[k], 1);
				if (_answerSounds[k] != _hoverSound) {
					_hoverSound = _answerSounds[k];
					_hoverCountdown = 36;
				}
				idle = false;
			}
		}
		if (idle) {
			sound.stopClip(_hoverSound);
			_hoverSound.clear();
		}
		if (over(_hoverSound) && _hoverCountdown-- == 0) {
			sound.stopEffects();
			play(_hoverSound);
			_hoverCountdown = 72;
		}
		break;
	}
	default:
		break;
	}
}

void EwanRoom::saveKeys(Common::INIFile &profile) {
	// 0x411780: the base keys, then CurrentQuestion (%i) and Question0-11,
	// in the room's section.
	profile.setKey("CurrentQuestion", kEwanRoom, Common::String::format("%i", _question));
	for (uint q = 0; q < kQuestions; q++)
		profile.setKey(Common::String::format("Question%u", q), kEwanRoom, _answered[q] ? "Yes" : "No");
}

void EwanRoom::loadKeys(const Common::INIFile &profile) {
	// 0x411850: CurrentQuestion through atoi (0x40C670); a missing flag
	// reads as false (0x40C560).
	Common::String v;
	_question = profile.getKey("CurrentQuestion", kEwanRoom, v) ? atoi(v.c_str()) : 0;
	for (uint q = 0; q < kQuestions; q++)
		_answered[q] = profile.getKey(Common::String::format("Question%u", q), kEwanRoom, v) && v == "Yes";
}

RoomClass *createEwanRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kEwanRoom)
		return new EwanRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
