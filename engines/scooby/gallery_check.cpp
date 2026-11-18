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

// The script-level check of Phantom's royal gallery and magic mirror room
// (WP-072), against an independent reference written from the trace.

#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/character.h"
#include "scooby/detection.h"
#include "scooby/gallery_rooms.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

void ScoobyEngine::checkGallery() {
	const char *const kGallery = "ROOM_P07_Royal_Gallery";
	const char *const kMirrorRoom = "ROOM_P08_Magic_Mirror_Room";
	const char *const kMirror = "OBJ_P07_ROYAL_GALLERY_MIRROR";
	const char *const kToP08 = "OBJ_P07_TO_P08";
	const char *const kInvMirror = "OBJ_INV_MIRROR";
	const char *const kBallRed = "OBJ_INV_BALL_RED";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kGallery;
	if (!found) {
		debugC(3, kDebugScript, "gallery check: no gallery");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

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
		for (int t = 0; t < 3000 && (!_script->isOpeningDone() || _script->isScenePlaying() || _script->isBusy()); t++) {
			if (_script->isScenePlaying())
				_script->skip();
			tick1();
		}
	};
	auto quietScene = [&]() {
		for (int t = 0; t < 2000 && _script->isScenePlaying(); t++) {
			_script->skip();
			tick1();
		}
	};

	// The classes.
	{
		RoomClass *a = createRoomClass(*this, kGallery);
		RoomClass *b = createRoomClass(*this, kMirrorRoom);
		debugC(3, kDebugScript, "gallery check classes: Royal Gallery %s; Magic Mirror Room %s",
		       dynamic_cast<RoyalGalleryRoom *>(a) ? "royal gallery" : "-", dynamic_cast<MagicMirrorRoom *>(b) ? "magic mirror room" : "-");
		delete a;
		delete b;
	}

	// The walk-ins, and the snacks the way on needs by level.
	{
		Common::String out;
		story(0);
		showRoom("ROOM_P12_Dining_Room");
		showRoom(kGallery);
		out += Common::String::format("from P12_Dining_Room %s", walker().c_str());
		story(0);
		showRoom(kMirrorRoom);
		showRoom(kGallery);
		out += Common::String::format("; from P08_Magic_Mirror_Room %s", walker().c_str());
		out += "; snacks needed by level";
		for (int l = 0; l < 3; l++) {
			story(l);
			showRoom(kGallery);
			out += Common::String::format(" %u", _script->getSnacksNeeded());
		}
		debugC(3, kDebugScript, "gallery check enter: %s", out.c_str());
	}

	// The mirror: bare it is stopped and the way on hidden; clicked it cycles
	// its lines; the found mirror used on it, Shaggy hangs it, it loops, the
	// way on opens, a line by level and the snack box; placed, it is hung on
	// return.
	{
		Common::String out;
		story(0);
		showRoom(kGallery);
		settle();
		int m = find(kMirror), ex = find(kToP08);
		out += Common::String::format("bare, mode %d step %u, the way out hidden %d", _currentRoom->getMode(m), _currentRoom->getStep(m), ex >= 0 && _currentRoom->isHidden(ex) ? 1 : 0);
		out += "; clicked:";
		for (int k = 0; k < 3; k++) {
			_script->sendEvent(kMirror, false);
			out += Common::String::format(" '%s'", _script->getScene().c_str());
			quietScene();
		}

		story(0);
		_inventory->addItem(kInvMirror);
		showRoom(kGallery);
		settle();
		m = find(kMirror);
		ex = find(kToP08);
		out += Common::String::format("; in the toolbar: placed %d", RoyalGalleryRoom::isMirrorPlaced(*this) ? 1 : 0);
		_script->sendEvent(kMirror, true);
		bool puts = false, sound = false, flash = false;
		Common::String line;
		for (int t = 0; t < 4000; t++) {
			Character *shaggy = _script->getCharacter("SHAGGY");
			if (shaggy && _currentRoom->getAnim(shaggy->getSprite()).contains("PUT_MIRROR"))
				puts = true;
			if (_sound->isClipPlaying("Royal_Gallery.Shaggy_Puts_Mirror"))
				sound = true;
			if (_script->getScene().hasPrefix("Royal_Gallery.210B"))
				line = _script->getScene();
			if (_inventory->isSnackFlashing())
				flash = true;
			if (m >= 0 && _currentRoom->getMode(m) == 1 && ex >= 0 && !_currentRoom->isHidden(ex) && !_script->isScenePlaying())
				break;
			if (_script->isScenePlaying())
				_script->skip();
			tick1();
		}
		out += Common::String::format("; hung: Shaggy_Puts_Mirror %d (%d), looping %d, the way out hidden %d, '%s', the snack box %d", sound ? 1 : 0, puts ? 1 : 0,
		                              m >= 0 && _currentRoom->getMode(m) == 1 ? 1 : 0, ex >= 0 && _currentRoom->isHidden(ex) ? 1 : 0, line.c_str(), flash ? 1 : 0);

		showRoom(kGallery);
		m = find(kMirror);
		ex = find(kToP08);
		out += Common::String::format("; on return: placed %d, looping %d, the way out hidden %d", RoyalGalleryRoom::isMirrorPlaced(*this) ? 1 : 0,
		                              m >= 0 && _currentRoom->getMode(m) == 1 ? 1 : 0, ex >= 0 && _currentRoom->isHidden(ex) ? 1 : 0);
		debugC(3, kDebugScript, "gallery check mirror: %s", out.c_str());
	}

	// The red ball shown to the Jester: found, and the gallery re-entered.
	{
		story(0);
		showRoom(kGallery);
		settle();
		_requestedRoom.clear();
		_requestedMovie = -1;
		_script->sendEvent(kBallRed, true);
		debugC(3, kDebugScript, "gallery check ball: RedBallFound %d, to %s movie %d", getGameFlag("RedBallFound") ? 1 : 0,
		       _requestedRoom.empty() ? "-" : _requestedRoom.c_str(), _requestedMovie);
	}

	// The way to the magic mirror room, snack-gated by level.
	{
		Common::String out = "to P08 with none:";
		for (int l = 0; l < 3; l++) {
			story(l);
			showRoom(kGallery);
			settle();
			RoomClass *gallery = getRoomClass(kGallery);
			const bool blocked = gallery->leaveBy(kToP08);
			out += Common::String::format(" '%s' held %d snack box %d blocked %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0,
			                              _inventory->isSnackFlashing() ? 1 : 0, blocked ? 1 : 0);
			quietScene();
		}
		story(2);
		showRoom(kGallery);
		settle();
		_script->getMemory(kGallery).snacksGiven = 4;
		out += Common::String::format("; with enough: blocked %d", getRoomClass(kGallery)->leaveBy(kToP08) ? 1 : 0);
		debugC(3, kDebugScript, "gallery check gate: %s", out.c_str());
	}

	// The magic mirror room: its hum, its hotspots, its placements.
	{
		Common::String out;
		story(0);
		showRoom(kMirrorRoom);
		out += Common::String::format("enter Magic_Mirror_Room.Mirror %d", _sound->isClipPlaying("Magic_Mirror_Room.Mirror") ? 1 : 0);
		settle();
		out += "; HOTSPOT2";
		for (int k = 0; k < 2; k++) {
			_script->sendEvent("OBJ_P08_MAGIC_MIRROR_ROOM_HOTSPOT2", false);
			out += Common::String::format(" '%s'", _script->getScene().c_str());
			quietScene();
		}
		_script->sendEvent("OBJ_P08_MAGIC_MIRROR_ROOM_HOTSPOT3", false);
		out += Common::String::format("; HOTSPOT3 '%s' held %d", _script->getScene().c_str(), _script->isBusy() ? 1 : 0);
		quietScene();
		out += "; HOTSPOT4";
		for (int k = 0; k < 3; k++) {
			_script->sendEvent("OBJ_P08_MAGIC_MIRROR_ROOM_HOTSPOT4", false);
			out += Common::String::format(" '%s'", _script->getScene().c_str());
			quietScene();
		}
		auto kind = [&](int s) {
			if (s < 0)
				return Common::String("none");
			return Common::String::format("depth %d hidden %d", _currentRoom->getDepth(s), _currentRoom->isHidden(s) ? 1 : 0);
		};
		out += Common::String::format("; HOTSPOT5 %s, the skull %s, CAM1 %s", kind(find("OBJ_P08_MAGIC_MIRROR_ROOM_HOTSPOT5")).c_str(),
		                              kind(find("OBJ_P08_TO_WIZARDS_SKULL")).c_str(), kind(find("OBJ_P08_MAGIC_MIRROR_CAM1")).c_str());
		debugC(3, kDebugScript, "gallery check mirror room: %s", out.c_str());
	}

	_checking = false;
}

} // End of namespace Scooby
