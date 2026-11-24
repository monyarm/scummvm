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

// The script-level check of Phantom's secret passages, storage room and
// treasury (WP-071), against an independent reference written from the
// trace.

#include "common/debug.h"
#include "common/formats/ini-file.h"

#include "terraglyph/formats/art_archive.h"
#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/rooms/phantomknight/passage.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkPassages() {
	const char *const kP15 = "ROOM_P15_Secret_Passage1";
	const char *const kP19 = "ROOM_P19_Secret_Passage2";
	const char *const kP18 = "ROOM_P18_Storage";
	const char *const kP16 = "ROOM_P16_Treasury";
	const char *const kFlame = "OBJ_P19_SECRET_PASSAGE_GAR_FLAME";
	const char *const kRope = "OBJ_INV_ROPE";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kP19;
	if (!found) {
		debugC(3, kDebugScript, "passage check: no secret passages");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *a = createRoomClass(*this, kP15);
		RoomClass *b = createRoomClass(*this, kP19);
		RoomClass *c = createRoomClass(*this, kP18);
		RoomClass *d = createRoomClass(*this, kP16);
		debugC(3, kDebugScript, "passage check classes: P15 %s; P19 %s; P18 %s; P16 %s", dynamic_cast<SecretPassage1Room *>(a) ? "first passage" : "-",
		       dynamic_cast<SecretPassage2Room *>(b) ? "second passage" : "-", dynamic_cast<StorageRoomP18 *>(c) ? "storage room" : "-",
		       dynamic_cast<TreasuryRoom *>(d) ? "treasury" : "-");
		delete a;
		delete b;
		delete c;
		delete d;
	}
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
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
		const char *kind = r.isRequiresObject(s) ? "requires" : (r.isInventoryObject(s) ? "inventory" : "other");
		return Common::String::format("%s %s depth %d hidden %d", kind, r.getAnim(s).c_str(), r.getDepth(s), r.isHidden(s) ? 1 : 0);
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
	auto story = [&](int level) {
		newGameForCheck();
		setGameString("Whodunit", "JESTER");
		setDifficulty(level);
		_sound->stop();
	};
	auto settle = [&]() {
		// The opening played through (it starts at tick 100) and input free.
		for (int t = 0; t < 3000 && (!_script->isOpeningDone() || _script->isScenePlaying() || _script->isBusy()); t++) {
			if (_script->isScenePlaying())
				_script->skip();
			tick1();
		}
	};
	auto passage2 = [&]() { return dynamic_cast<SecretPassage2Room *>(getRoomClass(kP19)); };
	auto storage = [&]() { return dynamic_cast<StorageRoomP18 *>(getRoomClass(kP18)); };
	auto quietScene = [&]() {
		for (int t = 0; t < 2000 && _script->isScenePlaying(); t++) {
			_script->skip();
			tick1();
		}
	};

	// The first passage's walk-ins.
	{
		Common::String out;
		static const char *const kFrom[] = { "ROOM_P20_Kings_Armory_Slide", "ROOM_P18_Storage", "ROOM_P19_Secret_Passage2" };
		for (uint i = 0; i < ARRAYSIZE(kFrom); i++) {
			story(0);
			showRoom(kFrom[i]);
			showRoom(kP15);
			out += Common::String::format("%sfrom %s: %s", i ? "; " : "", kFrom[i] + 5, walker().c_str());
		}
		debugC(3, kDebugScript, "passage check first: %s", out.c_str());
	}

	// The wall of fire: built burning, with its sound; the snacks needed by
	// level; clicked without them, a line and the snack box; with them, out
	// through movie 39; back, out for good; the profile.
	{
		Common::String out;
		story(0);
		showRoom(kP19);
		out += Common::String::format("burning: %s, Wall_Of_Fire playing %d", sprite(find(kFlame)).c_str(), _sound->isClipPlaying("Secret_2.Wall_Of_Fire") ? 1 : 0);
		for (int l = 0; l < 3; l++) {
			story(l);
			showRoom(kP19);
			settle();
			_script->sendEvent(kFlame, false);
			out += Common::String::format("; level %d: needed %u, clicked with none: '%s' held %d, the snack box flashing %d, burning %d", l, _script->getSnacksNeeded(),
			                              _script->getScene().c_str(), _script->isBusy() ? 1 : 0, _inventory->isSnackFlashing() ? 1 : 0, passage2()->isFlameActive() ? 1 : 0);
			quietScene();
			_script->getMemory(kP19).snacksGiven = l + 1;
			_script->sendEvent(kFlame, false);
			out += Common::String::format(", with %d: '%s'", l + 1, _script->getScene().c_str());
			quietScene();
			_script->getMemory(kP19).snacksGiven = l + 2;
			_requestedRoom.clear();
			_script->sendEvent(kFlame, false);
			out += Common::String::format(", with %d: to %s movie %d, burning %d", l + 2, _requestedRoom.empty() ? "-" : _requestedRoom.c_str(), _requestedMovie,
			                              passage2()->isFlameActive() ? 1 : 0);
			_requestedRoom.clear();
		}
		_sound->stop();
		showRoom(kP19);
		out += Common::String::format("; back: %s, Wall_Of_Fire playing %d", sprite(find(kFlame)).c_str(), _sound->isClipPlaying("Secret_2.Wall_Of_Fire") ? 1 : 0);
		Common::INIFile profile;
		passage2()->saveKeys(profile);
		Common::String v;
		out += Common::String::format("; saved GargoyleFlameActive %s", profile.getKey("GargoyleFlameActive", kP19, v) ? v.c_str() : "-");
		profile.setKey("GargoyleFlameActive", kP19, "Yes");
		passage2()->loadKeys(profile);
		out += Common::String::format(", Yes loads burning %d", passage2()->isFlameActive() ? 1 : 0);
		profile.setKey("GargoyleFlameActive", kP19, "1");
		passage2()->loadKeys(profile);
		out += Common::String::format(", 1 burning %d", passage2()->isFlameActive() ? 1 : 0);
		passage2()->reset();
		out += Common::String::format("; a new game: burning %d", passage2()->isFlameActive() ? 1 : 0);
		debugC(3, kDebugScript, "passage check fire: %s", out.c_str());
	}

	// The storage room: the rope's placement; the hotspots' lines; the rope
	// with the arrow, its line by WellVisited, then Velma takes it.
	{
		Common::String out;
		story(0);
		showRoom(kP18);
		out += Common::String::format("the rope: %s", sprite(find(kRope)).c_str());
		_inventory->addItem(kRope);
		showRoom(kP18);
		out += Common::String::format("; had: %s", sprite(find(kRope)).c_str());
		story(0);
		showRoom(kP18);
		settle();
		out += "; the hotspots:";
		static const char *const kSpots[] = { "OBJ_P18_STORAGE_HOTSPOT1", "OBJ_P18_STORAGE_HOTSPOT2", "OBJ_P18_STORAGE_HOTSPOT1", "OBJ_P18_STORAGE_HOTSPOT2",
		                                      "OBJ_P18_STORAGE_HOTSPOT3" };
		for (uint k = 0; k < ARRAYSIZE(kSpots); k++) {
			_script->sendEvent(kSpots[k], false);
			out += Common::String::format(" '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
			quietScene();
		}
		for (int well = 0; well < 2; well++) {
			story(0);
			setGameFlag("WellVisited", well != 0);
			showRoom(kP18);
			settle();
			const int rope = find(kRope);
			_script->sendEvent(kRope, false);
			out += Common::String::format("; well visited %d: '%s' held %d, pending %d", well, _script->getScene().c_str(), _script->isBusy() ? 1 : 0,
			                              storage()->isTakePending() ? 1 : 0);
			if (well == 0) {
				// Ticks counted from the one the line ends in.
				Character *velma = _script->getCharacter("VELMA");
				int over = -1, taking = -1, inBar = -1, done = -1;
				Common::String anim, state;
				bool sound = false, hidden = false;
				for (int t = 0; t <= 2000 && done < 0; t++) {
					if (over < 0 && _script->isScenePlaying())
						_script->skip();
					tick1();
					if (over < 0 && !_script->isScenePlaying()) {
						over = t;
						state = Common::String::format("pending %d, held %d, cursor hidden %d", storage()->isTakePending() ? 1 : 0, storage()->isBusy() ? 1 : 0,
						                               storage()->hidesCursor() ? 1 : 0);
					}
					if (over < 0)
						continue;
					if (taking < 0 && storage()->isTaking()) {
						taking = t;
						anim = _currentRoom->getAnim(velma->getSprite());
						sound = _sound->isClipPlaying("Storage.Velma_Takes_Rope");
						hidden = rope >= 0 && _currentRoom->isHidden(rope);
					}
					if (inBar < 0 && _inventory->hasItem(kRope))
						inBar = t;
					if (taking >= 0 && !storage()->isTakePending())
						done = t;
				}
				out += Common::String::format("; the line over: %s; then Velma at tick %d (%s, Velma_Takes_Rope %d, the rope hidden %d); in the toolbar at tick %d; done at tick %d, held %d, cursor hidden %d",
				                              state.c_str(), taking < 0 ? -1 : taking - over, anim.c_str(), sound ? 1 : 0, hidden ? 1 : 0, inBar < 0 ? -1 : inBar - over,
				                              done < 0 ? -1 : done - over, storage()->isBusy() ? 1 : 0,
				                              storage()->hidesCursor() ? 1 : 0);
			} else {
				quietScene();
				tick1();
				showRoom(kP15);
				out += Common::String::format("; left before she took it: the rope %d", _inventory->hasItem(kRope) ? 1 : 0);
			}
		}
		debugC(3, kDebugScript, "passage check storage: %s", out.c_str());
	}

	// The treasury: the Princess clicked with the arrow, and with an item.
	{
		Common::String out;
		story(0);
		showRoom(kP16);
		Character *princess = _script->getCharacter("PRINCESS");
		const int s = princess ? princess->getSprite() : -1;
		_roomClass->click(s);
		tick1();
		out += Common::String::format("with the arrow: FinalClue %d, to %s movie %d", getGameFlag("FinalClue") ? 1 : 0, next.empty() ? "-" : next.c_str(), movie);
		story(0);
		_inventory->addItem(kRope);
		showRoom(kP16);
		_inventory->setCursor("ANIM_CURSORROPE");
		_roomClass->click(s);
		tick1();
		out += Common::String::format("; with the rope: FinalClue %d, to %s", getGameFlag("FinalClue") ? 1 : 0, next.empty() ? "-" : next.c_str());
		_inventory->setCursor("ANIM_CURSORARROW");
		story(0);
		showRoom(kP16);
		_roomClass->click(-1);
		tick1();
		out += Common::String::format("; elsewhere: FinalClue %d, to %s", getGameFlag("FinalClue") ? 1 : 0, next.empty() ? "-" : next.c_str());
		debugC(3, kDebugScript, "passage check treasury: %s", out.c_str());
	}
	_checking = false;
}

} // End of namespace Terraglyph
