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

#include "common/config-manager.h"
#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/detection.h"
#include "scooby/ewan_rooms.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

void ScoobyEngine::checkEwan() {
	// Phantom's Ewan's quiz (WP-066).
	const char *const kEwanRoom = "ROOM_Ewan";
	const char *const kKitchen = "ROOM_P13_Kitchen";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kEwanRoom;
	if (!found) {
		debugC(3, kDebugScript, "ewan check: no Ewan's quiz");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kEwanRoom);
		debugC(3, kDebugScript, "ewan check classes: Ewan %s", dynamic_cast<EwanRoom *>(c) ? "quiz" : "-");
		delete c;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto ewan = [&]() { return dynamic_cast<EwanRoom *>(getRoomClass(kEwanRoom)); };
	auto sprite = [&](int s) {
		if (s < 0)
			return Common::String("none");
		return Common::String::format("%s %s mode %d step %u depth %d hit %d%s%s", _currentRoom->getObjectName(s).c_str(), _currentRoom->getAnim(s).c_str(),
		                              _currentRoom->getMode(s), _currentRoom->getStep(s), _currentRoom->getDepth(s), _currentRoom->getHitType(s),
		                              _currentRoom->isRequiresObject(s) ? " requires" : "", _currentRoom->isHidden(s) ? " hidden" : "");
	};
	auto steps = [&]() {
		Common::String r = Common::String::format("board step %u, answers", _currentRoom->getStep(ewan()->getBoard()));
		for (uint i = 0; i < EwanRoom::kAnswers; i++)
			r += Common::String::format(" %u", _currentRoom->getStep(ewan()->getAnswer(i)));
		return r;
	};
	// A new game, the question set, from the kitchen; the opening played.
	auto enterEwan = [&](int q, bool skipOpening) {
		newGameForCheck();
		showRoom(kKitchen);
		ewan()->setQuestion(q);
		_hover = -1;
		showRoom(kEwanRoom);
		if (skipOpening)
			_script->skip();
	};
	// The sound waited for over at once, then a tick.
	auto overTick = [&]() {
		_sound->stopClip(ewan()->getCurrent());
		return tick1();
	};
	auto readAll = [&]() {
		for (uint t = 0; t < 20 && ewan()->getState() < 5; t++)
			overTick();
		overTick();
	};
	auto eraseOut = [&]() {
		int t = 0;
		while (ewan()->isErasing() && t < 400) {
			tick1();
			t++;
		}
		return t;
	};

	// Entry: the board, the answers and the eraser; the page's sounds; the
	// opening; drawn questions.
	{
		enterEwan(0, false);
		Common::String out = Common::String::format("board %s", sprite(ewan()->getBoard()).c_str());
		for (uint i = 0; i < EwanRoom::kAnswers; i++)
			out += Common::String::format("; %c %s", 'A' + i, sprite(ewan()->getAnswer(i)).c_str());
		out += Common::String::format("; eraser %s", sprite(ewan()->getEraser()).c_str());
		int exitSprite = -1;
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == "OBJ_EWAN_EXIT")
				exitSprite = s;
		}
		out += Common::String::format("; exit to %s; question %d, state %d, busy %d; sounds '%s'", exitSprite >= 0 ? _currentRoom->getDestination(exitSprite).c_str() : "none",
		                              ewan()->getQuestion(), ewan()->getState(), ewan()->isBusy() ? 1 : 0, ewan()->getQuestionSound().c_str());
		for (uint i = 0; i < EwanRoom::kAnswers; i++)
			out += Common::String::format(" '%s'", ewan()->getAnswerSound(i).c_str());
		tick1();
		out += Common::String::format("; opening '%s' unplayed: a tick on state %d", _script->getOpening().c_str(), ewan()->getState());
		int bad = 0;
		for (uint k = 0; k < 100; k++) {
			newGameForCheck();
			showRoom(kKitchen);
			showRoom(kEwanRoom);
			const int q = ewan()->getQuestion();
			bad += (q < 0 || q > 11 || q == 1 || q == 4) ? 1 : 0;
		}
		out += Common::String::format("; 100 drawn, page 2 or 5 or out of range %d", bad);
		debugC(3, kDebugScript, "ewan check entry: %s", out.c_str());
	}

	// The page read, then each answer, each lit up; then free.
	{
		enterEwan(0, true);
		tick1();
		Common::String out = Common::String::format("tick 1: state %d, sound '%s', %s, busy %d", ewan()->getState(), ewan()->getCurrent().c_str(), steps().c_str(),
		                                            ewan()->isBusy() ? 1 : 0);
		for (uint i = 0; i < EwanRoom::kAnswers; i++) {
			overTick();
			out += Common::String::format("; %c: state %d, sound '%s', %s, busy %d", 'A' + i, ewan()->getState(), ewan()->getCurrent().c_str(), steps().c_str(),
			                              ewan()->isBusy() ? 1 : 0);
		}
		overTick();
		out += Common::String::format("; free: state %d, sound '%s', %s, busy %d", ewan()->getState(), ewan()->getCurrent().c_str(), steps().c_str(),
		                              ewan()->isBusy() ? 1 : 0);
		debugC(3, kDebugScript, "ewan check reading: %s", out.c_str());

		// Hovered: lit up, its sound on, then again after it ends.
		_hover = ewan()->getAnswer(1);
		tick1();
		out = Common::String::format("B hovered: %s, sound '%s', countdown %d", steps().c_str(), ewan()->getHoverSound().c_str(), ewan()->getHoverCountdown());
		int said = -1;
		for (uint t = 2; t <= 200 && said < 0; t++) {
			tick1();
			if (_sound->isClipPlaying(ewan()->getHoverSound()))
				said = t;
		}
		out += Common::String::format("; said at tick %d, countdown %d", said, ewan()->getHoverCountdown());
		_sound->stopClip(ewan()->getHoverSound());
		int again = -1;
		for (uint t = 1; t <= 200 && again < 0; t++) {
			tick1();
			if (_sound->isClipPlaying(ewan()->getHoverSound()))
				again = t;
		}
		out += Common::String::format("; again %d ticks after it ends", again);
		_sound->stopClip(ewan()->getHoverSound());
		_hover = ewan()->getBoard();
		tick1();
		out += Common::String::format("; the board hovered: %s, sound '%s', countdown %d", steps().c_str(), ewan()->getHoverSound().c_str(), ewan()->getHoverCountdown());
		_hover = -1;
		tick1();
		out += Common::String::format("; nothing: %s, sound '%s'", steps().c_str(), ewan()->getHoverSound().c_str());
		debugC(3, kDebugScript, "ewan check hover: %s", out.c_str());
	}

	// A right answer and the eraser, then a wrong one.
	{
		enterEwan(0, true);
		readAll();
		const bool taken = _roomClass->click(ewan()->getAnswer(1));
		const Common::String line = ewan()->getCurrent();
		const bool known = line == "Ewan.NOTEBOOKPAGE_ANSWER_RIGHT_1" || line == "Ewan.NOTEBOOKPAGE_ANSWER_RIGHT_2";
		const int eraser = ewan()->getEraser();
		Common::String out = Common::String::format("page 1, B: taken %d, B step %u, one of RIGHT_1-2 %d, answered %d, erasing %d, eraser %s, Erase_Board %d, busy %d",
		                                            taken ? 1 : 0, _currentRoom->getStep(ewan()->getAnswer(1)), known ? 1 : 0, ewan()->isAnswered(0) ? 1 : 0,
		                                            ewan()->isErasing() ? 1 : 0, sprite(eraser).c_str(), _sound->isClipPlaying("Ewan.Erase_Board") ? 1 : 0,
		                                            ewan()->isBusy() ? 1 : 0);
		const int t = eraseOut();
		const int q = ewan()->getQuestion();
		out += Common::String::format("; erased at tick %d: drawn anew %d, eraser hidden %d, state %d, board on its page %d", t, q != 0 && q != 1 && q != 4 ? 1 : 0,
		                              _currentRoom->isHidden(eraser) ? 1 : 0, ewan()->getState(),
		                              _currentRoom->getAnim(ewan()->getBoard()) == Common::String::format("ANIM_EWAN_BOARDPAGE%d", q + 1) ? 1 : 0);
		_sound->stopClip(ewan()->getCurrent());
		enterEwan(3, true);
		readAll();
		_roomClass->click(ewan()->getAnswer(0));
		const Common::String wrong1 = ewan()->getCurrent();
		out += Common::String::format("; page 4, A: one of WRONG_1.1-5 %d, wrong %d", wrong1.hasPrefix("Ewan.NOTEBOOKPAGE_ANSWER_WRONG_1.") ? 1 : 0, ewan()->isWrong() ? 1 : 0);
		overTick();
		out += Common::String::format("; over: '%s', wrong %d, done %d", ewan()->getCurrent().c_str(), ewan()->isWrong() ? 1 : 0, ewan()->isDone() ? 1 : 0);
		overTick();
		out += Common::String::format("; next %s", next.c_str());
		showRoom(kKitchen);
		out += Common::String::format("; left: EwanPuzzle %d, answered %u", getGameFlag("EwanPuzzle") ? 1 : 0, ewan()->getAnsweredCount());
		debugC(3, kDebugScript, "ewan check answers: %s", out.c_str());
	}

	// The level's count right (level + 3): the board and answers hidden,
	// then the kitchen, and the puzzle won.
	{
		Common::String out;
		for (int level = 0; level < 3; level++) {
			newGameForCheck();
			setDifficulty(level);
			showRoom(kKitchen);
			Common::INIFile ini;
			static const int kDone[] = { 0, 5, 6, 7 };
			for (int k = 0; k < level + 2; k++)
				ini.setKey(Common::String::format("Question%d", kDone[k]), kEwanRoom, "Yes");
			ini.setKey("CurrentQuestion", kEwanRoom, "2");
			ewan()->loadKeys(ini);
			_hover = -1;
			showRoom(kEwanRoom);
			_script->skip();
			readAll();
			_roomClass->click(ewan()->getAnswer(0));
			eraseOut();
			int shown = 0;
			for (uint i = 0; i < EwanRoom::kAnswers; i++)
				shown += _currentRoom->isHidden(ewan()->getAnswer(i)) ? 0 : 1;
			out += Common::String::format("%slevel %d, %u right: board hidden %d, answers shown %d, done %d", level ? "; " : "", level, ewan()->getAnsweredCount(),
			                              _currentRoom->isHidden(ewan()->getBoard()) ? 1 : 0, shown, ewan()->isDone() ? 1 : 0);
			overTick();
			out += Common::String::format(", next %s", next.c_str());
			showRoom(kKitchen);
			out += Common::String::format(", EwanPuzzle %d", getGameFlag("EwanPuzzle") ? 1 : 0);
		}
		setDifficulty(0);
		debugC(3, kDebugScript, "ewan check done: %s", out.c_str());
	}

	// The keys.
	{
		auto list = [&](const Common::INIFile &ini) {
			Common::String r, v;
			if (ini.getKey("CurrentQuestion", kEwanRoom, v))
				r += " CurrentQuestion=" + v;
			for (uint q = 0; q < EwanRoom::kQuestions; q++) {
				if (ini.getKey(Common::String::format("Question%u", q), kEwanRoom, v) && v == "Yes")
					r += Common::String::format(" Question%u=Yes", q);
			}
			return r;
		};
		newGameForCheck();
		Common::INIFile a;
		ewan()->saveKeys(a);
		Common::String out = "new game:" + list(a);
		Common::INIFile in;
		in.setKey("Question11", kEwanRoom, "Yes");
		ewan()->loadKeys(in);
		Common::INIFile b;
		ewan()->saveKeys(b);
		out += "; read Question11 alone:" + list(b);
		debugC(3, kDebugScript, "ewan check keys: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Scooby
