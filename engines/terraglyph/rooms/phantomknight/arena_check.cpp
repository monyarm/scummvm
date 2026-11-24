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

#include "terraglyph/rooms/phantomknight/arena.h"
#include "terraglyph/formats/art_archive.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkArenas() {
	// Phantom's two arenas (WP-069).
	const char *const kArena1 = "ROOM_P09_Arena1";
	const char *const kArena2 = "ROOM_P14_Arena2";
	const char *const kThrone = "ROOM_P10_Throne_Room";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kArena2;
	if (!found) {
		debugC(3, kDebugScript, "arena check: no arenas");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *a = createRoomClass(*this, kArena1);
		RoomClass *b = createRoomClass(*this, kArena2);
		debugC(3, kDebugScript, "arena check classes: Arena 1 %s; Arena 2 %s", dynamic_cast<Arena1Room *>(a) ? "first arena" : "-",
		       dynamic_cast<Arena2Room *>(b) ? "second arena" : "-");
		delete a;
		delete b;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto arena1 = [&]() { return dynamic_cast<Arena1Room *>(getRoomClass(kArena1)); };
	auto arena2 = [&]() { return dynamic_cast<Arena2Room *>(getRoomClass(kArena2)); };
	auto find = [&](const char *name) {
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == name)
				return (int)s;
		}
		return -1;
	};
	auto sprite = [&](int s) {
		if (s < 0)
			return Common::String("none");
		Room &r = *_currentRoom;
		const char *kind = r.isClickable(s) ? "clickable" : "other";
		return Common::String::format("%s %s depth %d hit %d hidden %d", kind, r.getAnim(s).c_str(), r.getDepth(s), r.getHitType(s), r.isHidden(s) ? 1 : 0);
	};
	auto walker = [&]() {
		static const char *const kGang[] = { "FRED", "VELMA", "DAPHNE", "SHAGGY", "SCOOBY" };
		Common::String who;
		for (uint g = 0; g < ARRAYSIZE(kGang); g++) {
			Character *ch = _script->getCharacter(kGang[g]);
			if (ch && ch->getState() == Character::kStateEndWalk)
				who += Common::String::format("%s%s depth %d", who.empty() ? "" : " ", kGang[g], _currentRoom->getDepth(ch->getSprite()));
		}
		return who.empty() ? Common::String("nobody") : who;
	};
	auto inSlot = [&](const char *item) {
		for (uint i = 0; i < Inventory::kSlotCount; i++) {
			if (_inventory->getSlot(i) == item)
				return 1;
		}
		return 0;
	};
	// A fresh story; Brad's game won or not; the second arena's opening
	// already played or not.
	auto story = [&](bool won, bool played) {
		newGameForCheck();
		setGameString("Whodunit", "JESTER");
		setGameFlag("BradPuzzle", won);
		_script->getMemory(kArena2).openingPlayed = played;
		_sound->stop();
	};
	auto untilNext = [&](uint maxTicks) {
		next.clear();
		movie = -1;
		for (uint t = 0; t < maxTicks && next.empty(); t++) {
			tick1();
			if (_script->isScenePlaying())
				_script->skip();
		}
		return Common::String::format("to %s movie %d", next.empty() ? "-" : next.c_str(), movie);
	};

	// The first arena: walk-ins, the toolbar back from the stone's movie,
	// the stone, hotspot 3.
	{
		Common::String out;
		static const char *const kFrom[] = { kThrone, kArena2, "ROOM_P04_Town" };
		for (uint i = 0; i < ARRAYSIZE(kFrom); i++) {
			story(false, true);
			showRoom(kFrom[i]);
			showRoom(kArena1);
			out += Common::String::format("%sfrom %s: %s", i ? "; " : "", kFrom[i] + 5, walker().c_str());
		}
		story(false, true);
		showRoom(kArena1);
		showRoom(kArena1);
		out += Common::String::format("; from itself: the toolbar opening %d", _inventory->getState() != Inventory::kStateParked ? 1 : 0);
		story(false, true);
		_inventory->addItem("OBJ_INV_RUSTY_SHEARS");
		showRoom(kThrone);
		showRoom(kArena1);
		_script->skip();
		out += "; the arrow on the stone:";
		for (int k = 0; k < 5; k++) {
			_script->sendEvent("OBJ_P09_ARENA_ONE_STONE", false);
			out += Common::String::format(" '%s'", _script->getScene().c_str());
			_script->skip();
			tick1();
		}
		_inventory->setCursor("ANIM_CURSORRUSTY_SHEARS");
		_script->sendEvent("OBJ_P09_ARENA_ONE_STONE", true);
		const int slot = inSlot("OBJ_INV_RUSTY_SHEARS");
		const int arrow = _inventory->isDefaultCursor() ? 1 : 0;
		const int sharpening = arena1()->isSharpening() ? 1 : 0;
		tick1();
		out += Common::String::format("; the rusty shears: to %s movie %d, in a slot %d, the arrow %d, sharpening %d", next.c_str(), movie, slot, arrow, sharpening);
		showRoom(kArena1);
		out += Common::String::format("; back: the sharp shears %d, sharpening %d", _inventory->hasItem("OBJ_INV_SHARP_SHEARS") ? 1 : 0, arena1()->isSharpening() ? 1 : 0);
		_script->sendEvent("OBJ_P09_ARENA_ONE_HOTSPOT3", true);
		out += Common::String::format("; hotspot 3: '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		debugC(3, kDebugScript, "arena check first: %s", out.c_str());
	}

	// The second arena's placements.
	{
		Common::String out;
		for (int k = 0; k < 3; k++) {
			story(k > 0, true);
			setDifficulty(k == 2 ? 1 : 0);
			if (k == 2) {
				_inventory->addItem("OBJ_INV_SHIELD");
				_inventory->removeItem("OBJ_INV_SHIELD");
				_inventory->addItem("OBJ_INV_MIRROR");
			}
			showRoom(kThrone);
			showRoom(kArena2);
			static const char *const kCase[] = { "not won", "won, level 0", "won, level 1, the shield used, the mirror had" };
			out += Common::String::format("%s%s: pile %s, Brad %s, Brad_H %s, mirror %s, pole %s, shield %s", k ? "; " : "", kCase[k], sprite(find("OBJ_BRAD_ARMOR_PILE")).c_str(),
			                              sprite(find("OBJ_BRAD")).c_str(), sprite(find("OBJ_BRAD_H")).c_str(), sprite(find("OBJ_INV_MIRROR")).c_str(),
			                              sprite(find("OBJ_P14_ARENA2_POLE")).c_str(), sprite(find("OBJ_P14_ARENA2_SHIELD")).c_str());
		}
		setDifficulty(0);
		story(false, true);
		showRoom(kThrone);
		showRoom(kArena2);
		out += Common::String::format("; from the throne room: %s", walker().c_str());
		story(false, true);
		showRoom(kArena1);
		showRoom(kArena2);
		out += Common::String::format("; from the first arena: %s", walker().c_str());
		debugC(3, kDebugScript, "arena check placements: %s", out.c_str());
	}

	// The first visit: Brad hidden, rising after the opening's fifth line;
	// then into his game.
	{
		story(false, false);
		showRoom(kThrone);
		showRoom(kArena2);
		// The throne room's first visit asked for Jane's movie; not this test's.
		_requestedRoom.clear();
		Character *brad = _script->getCharacter("BRAD");
		Arena2Room *c = arena2();
		Common::String out = Common::String::format("entry: Brad hidden %d, the opening pending %d, the pile's click its own %d", brad && _currentRoom->isHidden(brad->getSprite()) ? 1 : 0,
		                                            c->isOpeningPending() ? 1 : 0, c->isIgnoringClick() ? 1 : 0);
		int rose = -1, roseLine = -1, held = 0, line6 = -1, ended = -1;
		Common::String riseAnim;
		next.clear();
		movie = -1;
		for (int t = 1; t <= 6000 && next.empty(); t++) {
			tick1();
			if (rose < 0 && brad && !_currentRoom->isHidden(brad->getSprite())) {
				rose = t;
				roseLine = _script->getLine();
				riseAnim = _currentRoom->getAnim(brad->getSprite());
			}
			if (rose > 0 && line6 < 0 && _script->getLine() == 5 && brad->getState() != Character::kStateIdle)
				held++;
			if (line6 < 0 && _script->getLine() >= 6)
				line6 = t;
			if (ended < 0 && line6 > 0 && _script->getScene().empty())
				ended = t;
			// The lines said at once, so the scene runs on.
			if (_script->isSpeakerBusy())
				_sound->stop();
		}
		out += Common::String::format("; Brad rose on line %d (%s), the scene held while he rose %d, line 6 after %d, the opening over %d; then to %s",
		                              roseLine, riseAnim.c_str(), held > 0 ? 1 : 0, line6 > rose ? 1 : 0, ended > 0 ? 1 : 0, next.c_str());
		debugC(3, kDebugScript, "arena check rise: %s", out.c_str());
	}

	// The pile: the player's click, by level; its rattles.
	{
		Common::String out;
		for (int level = 0; level < 3; level++) {
			story(false, true);
			setDifficulty(level);
			showRoom(kThrone);
			showRoom(kArena2);
			const int pile = arena2()->getPile();
			// The room's own queued click plays the pile first; a click
			// while it plays is ignored.
			for (int t = 0; t < 400 && (t == 0 || _script->isClickablePlaying(pile)); t++)
				tick1();
			// A rattle while the click plays swallows it, as in the game;
			// a fixed seed keeps this run free of one.
			_randomSource.setSeed(1);
			_script->clickClickable(pile);
			out += Common::String::format("%slevel %d: %s", level ? "; " : "", level, untilNext(2000).c_str());
		}
		setDifficulty(0);
		story(false, true);
		showRoom(kThrone);
		showRoom(kArena2);
		int rattles = 0, changes = 0;
		for (int t = 0; t < 6000; t++) {
			const bool before = arena2()->isIgnoringClick();
			tick1();
			if (!before && arena2()->isIgnoringClick())
				rattles++;
			if (!next.empty())
				changes++;
			_sound->stop();
		}
		out += Common::String::format("; 6000 ticks: rattles 1-60 %d, a room change %d", rattles >= 1 && rattles <= 60 ? 1 : 0, changes);
		debugC(3, kDebugScript, "arena check pile: %s", out.c_str());
	}

	// The shield on Brad; back, he stands aside and Velma takes the mirror.
	{
		story(true, true);
		giveClue("BRAD");
		_inventory->addItem("OBJ_INV_SHIELD");
		showRoom(kThrone);
		showRoom(kArena2);
		_script->skip();
		Character *brad = _script->getCharacter("BRAD");
		const int bradX = _currentRoom->getOffsetX(brad->getSprite());
		_inventory->setCursor("ANIM_CURSORSHIELD");
		const bool taken = _roomClass->click(brad->getSprite());
		tick1();
		Common::String out = Common::String::format("the shield: taken %d, to %s movie %d, '%s' on %s back to %s, in a slot %d, the arrow %d, given %d", taken ? 1 : 0, next.c_str(), movie,
		                                            getBigHeadScene().c_str(), getBigHeadBackground().c_str(), getBigHeadNext().c_str(), inSlot("OBJ_INV_SHIELD"),
		                                            _inventory->isDefaultCursor() ? 1 : 0, arena2()->isShieldGiven() ? 1 : 0);
		showRoom("ROOM_Big_Head");
		showRoom(kArena2);
		_script->skip();
		brad = _script->getCharacter("BRAD");
		Arena2Room *c = arena2();
		out += Common::String::format("; back: Brad moved %d, stood aside %d", _currentRoom->getOffsetX(brad->getSprite()) - bradX, c->isBradMoved() ? 1 : 0);
		Character *velma = _script->getCharacter("VELMA");
		const int mirror = c->getMirror();
		int start = -1, done = -1;
		Common::String at;
		for (int t = 1; t <= 600 && done < 0; t++) {
			tick1();
			if (start < 0 && velma->getState() == Character::kStateSpecial) {
				start = t;
				at = Common::String::format("%s, Velma_Takes_Mirror %d, hidden %d, busy %d", _currentRoom->getAnim(velma->getSprite()).c_str(),
				                            _sound->isClipPlaying("Arena2.Velma_Takes_Mirror") ? 1 : 0, mirror >= 0 && _currentRoom->isHidden(mirror) ? 1 : 0, c->isBusy() ? 1 : 0);
			}
			if (start > 0 && !c->isMirrorPending())
				done = t;
		}
		out += Common::String::format("; Velma at tick %d (%s); at tick %d the mirror %d, busy %d", start, at.c_str(), done, _inventory->hasItem("OBJ_INV_MIRROR") ? 1 : 0,
		                              c->isBusy() ? 1 : 0);
		debugC(3, kDebugScript, "arena check shield: %s", out.c_str());
	}

	// Brad's game won: his clue, GIVES, the thanks.
	{
		story(true, true);
		showRoom(kThrone);
		showRoom(kArena2);
		const Common::String scene = _script->getScene();
		const int held = _script->isBusy() ? 1 : 0;
		const Common::String clue = getGameString("CluesFound0");
		const bool own = clue.hasPrefix("BRAD_");
		Common::String out = Common::String::format("'%s' held %d, his own clue %d, met %d, giving %d", scene.c_str(), held, own ? 1 : 0, _inventory->hasMetSuspect("BRAD") ? 1 : 0,
		                                            arena2()->isGiving() ? 1 : 0);
		bool acted = false;
		Common::String scenes, last;
		for (int t = 0; t < 4000 && next.empty(); t++) {
			tick1();
			Character *brad = _script->getCharacter("BRAD");
			acted = acted || (brad && brad->getState() == Character::kStateAct);
			const Common::String s = _script->getScene();
			if (!s.empty() && s != last) {
				scenes += Common::String::format("%s%s", scenes.empty() ? "" : " ", s.c_str());
				last = s;
				_script->skip();
			} else if (s.empty()) {
				last.clear();
			}
		}
		out += Common::String::format("; then scenes %s, Brad acted %d, to %s", scenes.empty() ? "none" : scenes.c_str(), acted ? 1 : 0, next.c_str());
		debugC(3, kDebugScript, "arena check clue: %s", out.c_str());
	}

	// Hotspot 2, the idle lines and chatter.
	{
		story(false, true);
		showRoom(kThrone);
		showRoom(kArena2);
		_script->skip();
		Arena2Room *c = arena2();
		const int h2 = find("OBJ_P14_ARENA2_HOTSPOT2");
		_script->sendEvent("OBJ_P14_ARENA2_HOTSPOT2", true);
		Common::String out = Common::String::format("hotspot 2 on its A: '%s'", _script->getScene().c_str());
		_script->clickClickable(h2);
		tick1();
		out += Common::String::format("; clicked, on its %s: '%s'", _script->getClickableState(h2).c_str(), _script->getScene().c_str());
		_script->skip();
		const Common::String base = _script->getBaseIdleLine("BRAD");
		out += Common::String::format("; not won: Brad's line the base one %d", c->idleLine(*_script, "BRAD") == base ? 1 : 0);
		c->idle();
		out += Common::String::format(", chatter '%s'", _script->getScene().c_str());
		story(true, true);
		giveClue("BRAD");
		showRoom(kThrone);
		showRoom(kArena2);
		_script->skip();
		c = arena2();
		uint seen = 0;
		for (int k = 0; k < 100; k++) {
			const Common::String l = c->idleLine(*_script, "BRAD");
			seen |= l == "Arena_2.1010H_1_BRAD" ? 1 : l == "Arena_2.1010H2_1_BRAD" ? 2 : 4;
		}
		out += Common::String::format("; won: 1010H_ %d, 1010H2_ %d, other %d", seen & 1 ? 1 : 0, seen & 2 ? 1 : 0, seen & 4 ? 1 : 0);
		_inventory->addItem("OBJ_INV_SHIELD");
		_inventory->removeItem("OBJ_INV_SHIELD");
		out += Common::String::format(", the shield used: '%s'", c->idleLine(*_script, "BRAD").c_str());
		c->idle();
		out += Common::String::format(", chatter '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		debugC(3, kDebugScript, "arena check idle: %s", out.c_str());
	}

	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Terraglyph
