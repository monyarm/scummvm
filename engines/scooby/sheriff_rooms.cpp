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

#include "common/debug.h"

#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sheriff_rooms.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kSheriffRoom = "ROOM_Sheriff";
static const char *const kAnswers1 = "OBJ_SHERIFF_ANSWERS1";

// 0x4176E0's right answers, by page (page 3, the last, has none).
static const char *const kRight[] = {
	"ANIM_SHERIFF_ANSWERS1C", "ANIM_SHERIFF_ANSWERS2C", "ANIM_SHERIFF_ANSWERS4C", "ANIM_SHERIFF_ANSWERS5D", "ANIM_SHERIFF_ANSWERS6C",
	"ANIM_SHERIFF_ANSWERS7A", "ANIM_SHERIFF_ANSWERS8B", "ANIM_SHERIFF_ANSWERS9C", "ANIM_SHERIFF_ANSWERS10A", "ANIM_SHERIFF_ANSWERS11D",
	"ANIM_SHERIFF_ANSWERS12B", "ANIM_SHERIFF_ANSWERS13D", "ANIM_SHERIFF_ANSWERS14C", "ANIM_SHERIFF_ANSWERS15A"
};

void SheriffRoom::reset() {
	// 0x416E80, after the base reset.
	for (uint q = 0; q < kQuestions; q++)
		_answered[q] = false;
	_current.clear();
	_next = false;
	_question = -1;
	_hoverSound.clear();
}

uint SheriffRoom::getAnsweredCount() const {
	uint n = 0;
	for (uint q = 0; q < kQuestions; q++)
		n += _answered[q] ? 1 : 0;
	return n;
}

Common::String SheriffRoom::lookup(const Common::String &name) const {
	return _engine.getSound()->findClip(name) ? name : Common::String();
}

bool SheriffRoom::over(const Common::String &name) const {
	return name.empty() || !_engine.getSound()->isClipPlaying(name);
}

void SheriffRoom::play(const Common::String &name) {
	if (name.empty())
		return;
	_engine.getSound()->play(name);
	_lastSound = name;
}

void SheriffRoom::show(int sprite, uint step) {
	if (sprite >= 0)
		_engine.getRoom()->setPlayer(sprite, 0, step);
}

void SheriffRoom::draw() {
	// 0x417220 and 0x4176E0: never page 3 (the last) or 6, nor one answered.
	int q;
	do {
		q = _engine.getRandomNumber(kQuestions - 1);
	} while (q == 2 || q == 5 || _answered[q]);
	_question = q;
}

Common::String SheriffRoom::pick(const Common::String &base) {
	// 0x417B70: the base and a space; the space a digit '1' + rand() % n,
	// n from 9, then the digit drawn, until one is on the disc or n is 0.
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

void SheriffRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x417530. The notebook a sprite (graphics_AnimSprite_ctor) under layer
	// 0 (game +0x58) at depth 0, looping. Each of the four answers
	// (OBJ_SHERIFF_ANSWERS1, kept by its sequence's letter) a requires
	// object (0x408C80 with -1: layer 0) at depth 10, 0x4176E0 its callback
	// (0x402FF0), stopped on step 0. The rest the default path builds.
	if (object == "OBJ_SHERIFF_NOTEBOOK") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = 0;
	} else if (object == kAnswers1) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
		claim.depth = 10;
	}
}

void SheriffRoom::setup() {
	// 0x417300. The level's count answered: page 3, the last. The page's
	// sequence looping, its sound; each answer to OBJ_SHERIFF_ANSWERS<page>
	// (0x4313C0) on ANIM_SHERIFF_ANSWERS<page><letter>, stopped on step 0,
	// with its sound. A page without answers (3) has no such object: they
	// show nothing.
	Room &room = *_engine.getRoom();
	if (getAnsweredCount() == (uint)(_engine.getDifficulty() + 2))
		_question = 2;
	const int page = _question + 1;
	if (_notebook >= 0)
		room.startAnim(_notebook, Common::String::format("ANIM_SHERIFF_NOTEBOOKPAGE%i", page), 1, -1);
	_questionSound = lookup(Common::String::format("Sheriff.NOTEBOOKPAGE%i", page));
	for (uint i = 0; i < kAnswers; i++) {
		const Common::String obj = Common::String::format("OBJ_SHERIFF_ANSWERS%i", page);
		const Common::String anim = Common::String::format("ANIM_SHERIFF_ANSWERS%i%c", page, 'A' + i);
		if (_answers[i] >= 0 && !room.setSequence(_answers[i], obj, anim, 0, 0))
			room.startAnim(_answers[i], anim, 0, 0);
		_answerSounds[i] = lookup(Common::String::format("Sheriff.NOTEBOOKPAGE%d_%c", page, 'A' + i));
	}
	_state = 0;
}

void SheriffRoom::enter(const Common::String &from) {
	// 0x417220, after the base entry: the toolbar off (0x407CC0, in the
	// toolbar's hidden list). No question yet: one drawn. Then its page.
	Room &room = *_engine.getRoom();
	_done = false;
	_wrong = false;
	_free = false;
	_lastSound.clear();
	_notebook = -1;
	for (uint i = 0; i < kAnswers; i++)
		_answers[i] = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == "OBJ_SHERIFF_NOTEBOOK")
			_notebook = s;
		if (room.getObjectName(s) == kAnswers1 && !room.getAnim(s).empty()) {
			const int i = room.getAnim(s).lastChar() - 'A';
			if (i >= 0 && i < (int)kAnswers)
				_answers[i] = s;
		}
	}
	for (uint i = 0; i < kAnswers; i++)
		show(_answers[i], 0);
	if (_question == -1)
		draw();
	setup();
}

void SheriffRoom::roomLeft() {
	// 0x4172C0, after the base leave: the level's count answered, the
	// Sheriff's puzzle done (SheriffPuzzle, game +0x26C).
	if (getAnsweredCount() == (uint)(_engine.getDifficulty() + 2))
		_engine.setGameFlag("SheriffPuzzle", true);
}

bool SheriffRoom::click(int sprite) {
	// The answers' callback (0x4176E0, room vtable +0x40), from their click
	// (0x402F20) with the arrow: the answer pressed (step 2). Right: the
	// page answered, a RIGHT_ line, a new page drawn for once the line is
	// over. Wrong: a WRONG_1. line, then the second and Street 1. Either
	// stops what the SFX and speech files play first (0x434E10).
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
	show(sprite, 2);
	bool right = false;
	for (uint k = 0; k < ARRAYSIZE(kRight); k++)
		right = right || room.getAnim(sprite) == kRight[k];
	const Common::String line = pick(right ? "Sheriff.NOTEBOOK_ANSWER_RIGHT_" : "Sheriff.NOTEBOOK_ANSWER_WRONG_1.");
	_engine.getSound()->stopEffects();
	_current = line;
	play(line);
	const int page = _question + 1;
	if (right) {
		_answered[_question] = true;
		draw();
		_next = true;
	} else {
		_wrong = true;
	}
	debugC(1, kDebugScript, "Sheriff: answer %c on page %d %s, '%s'", 'A' + i, page, right ? "right" : "wrong", line.c_str());
	return true;
}

void SheriffRoom::tick() {
	// 0x416ED0, after the base tick. Input held and the cursor faded; the
	// rest waits for the sound playing to be over.
	_free = false;
	if (!over(_current))
		return;
	// Done: Street 1 (game vtable +0x04).
	if (_done) {
		_engine.requestRoom("ROOM_P03_Street1");
		return;
	}
	// Wrong: the second line (the exe names Ewan's), then done.
	if (_wrong) {
		_current = lookup("Ewan.NOTEBOOKPAGE_ANSWER_WRONG_2");
		play(_current);
		_wrong = false;
		_done = true;
		return;
	}
	// Right: the next page.
	if (_next) {
		setup();
		_next = false;
		return;
	}
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	switch (_state) {
	case 0:
		// The page read, lit up.
		sound.stopClip(_current);
		_current = _questionSound;
		play(_current);
		show(_notebook, 1);
		_state++;
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		// Each answer read in turn, lit up.
		show(_notebook, 0);
		for (uint k = 0; k < kAnswers; k++)
			show(_answers[k], 0);
		show(_answers[_state - 1], 1);
		_current = _answerSounds[_state - 1];
		play(_current);
		_state++;
		break;
	case 5: {
		// Free. What the mouse is over lights up and, 9 ticks on and every
		// 72 after its sound, says itself again.
		_current.clear();
		_free = true;
		const int hover = _engine.getHover();
		bool idle = true;
		if (hover != _notebook || _notebook < 0) {
			show(_notebook, 0);
			if (_questionSound == _hoverSound) {
				sound.stopClip(_hoverSound);
				_hoverSound.clear();
			}
		} else {
			if (_hoverSound != _questionSound) {
				show(_notebook, 1);
				_hoverCountdown = 9;
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
					_hoverCountdown = 9;
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
	(void)room;
	// The level's count answered: done.
	if (getAnsweredCount() == (uint)(_engine.getDifficulty() + 2))
		_done = true;
}

void SheriffRoom::saveKeys(Common::INIFile &profile) {
	// 0x4179B0: the base keys, then CurrentQuestion (%i) and Question0-14,
	// in the room's section.
	profile.setKey("CurrentQuestion", kSheriffRoom, Common::String::format("%i", _question));
	for (uint q = 0; q < kQuestions; q++)
		profile.setKey(Common::String::format("Question%u", q), kSheriffRoom, _answered[q] ? "Yes" : "No");
}

void SheriffRoom::loadKeys(const Common::INIFile &profile) {
	// 0x417A80: CurrentQuestion through atoi (0x40C670); a missing flag
	// reads as false (0x40C560).
	Common::String v;
	_question = profile.getKey("CurrentQuestion", kSheriffRoom, v) ? atoi(v.c_str()) : 0;
	for (uint q = 0; q < kQuestions; q++)
		_answered[q] = profile.getKey(Common::String::format("Question%u", q), kSheriffRoom, v) && v == "Yes";
}

RoomClass *createSheriffRoom(ScoobyEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kSheriffRoom)
		return new SheriffRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
