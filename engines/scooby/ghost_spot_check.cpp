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
#include "scooby/ghost_spot_room.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/room_setup.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

void ScoobyEngine::checkGhostSpot() {
	// The ghost spot (WP-056), in both Gen 1 titles: the class, the entry and
	// its sprites, the pan to the town, a skip, the toggle across visits, a
	// click, and where each title goes on to.
	const char *const kGhost = "ROOM_Ghost_Spot";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kGhost;
	if (!found) {
		debugC(3, kDebugScript, "ghost spot check: no ghost spot");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kGhost);
		debugC(3, kDebugScript, "ghost spot check classes: Ghost Spot %s", dynamic_cast<GhostSpotRoom *>(c) ? "ghost spot" : "-");
		delete c;
	}
	// The room's class as the factory builds it, then the room.
	auto fresh = [&](const Common::String &name) {
		RoomClass *c;
		if (_roomClasses.tryGetVal(name, c)) {
			if (_roomClass == c)
				_roomClass = nullptr;
			_script->setHooks(nullptr);
			delete c;
			_roomClasses.erase(name);
		}
		showRoom(name);
	};
	auto spot = [&]() { return dynamic_cast<GhostSpotRoom *>(_roomClass); };
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	const Common::String town = GhostSpotRoom::getReturnRoom(getGameId(), kGhost);
	auto enterSpot = [&](int toggle) {
		newGameForCheck();
		showRoom(town);
		GhostSpotRoom::setToggle(toggle);
		fresh(kGhost);
	};
	auto layerX = [&]() {
		const int layer = _currentRoom->getLayerSprite(0);
		return layer >= 0 ? _currentRoom->getOffsetX(layer) : 0;
	};

	// Entry.
	{
		enterSpot(0);
		GhostSpotRoom *c = spot();
		Room &r = *_currentRoom;
		const int layer = r.getLayerSprite(0);
		int fx = 0, fy = 0, fw = 0, fh = 0;
		if (layer >= 0)
			r.getFrameRect(layer, fx, fy, fw, fh);
		Common::String out = Common::String::format("from %s: layer 0 at %d %d, %dx%d; countdown %d; busy %d; music %s playing %d; toggle %d",
		                                            town.c_str(), layer >= 0 ? r.getOffsetX(layer) : 0, layer >= 0 ? r.getOffsetY(layer) : 0, fw, fh,
		                                            c ? c->getCountdown() : -1, c && c->isBusy() ? 1 : 0, _music->getClip().c_str(),
		                                            _sound->isClipPlaying(_music->getClip()) ? 1 : 0, GhostSpotRoom::getToggle());
		for (uint s = 0; s < r.getSpriteCount(); s++) {
			if (!r.getObjectName(s).hasPrefix("OBJ_TITLE_"))
				continue;
			const int parent = r.getParent(s);
			out += Common::String::format("; %s %s mode %d step %u depth %d hit %d parent %s at %d %d", r.getObjectName(s).c_str(), r.getAnim(s).c_str(),
			                              r.getMode(s), r.getStep(s), r.getDepth(s), r.getHitType(s), parent == 0 ? "root" : r.getLabel(parent).c_str(),
			                              r.getOffsetX(s), r.getOffsetY(s));
		}
		debugC(3, kDebugScript, "ghost spot check entry: %s", out.c_str());
	}

	// The pan, to the town.
	{
		enterSpot(0);
		GhostSpotRoom *c = spot();
		Common::String xs;
		int x395 = 0, count395 = 0, left = -1;
		Common::String to;
		for (uint t = 1; t <= 400 && left < 0; t++) {
			const bool changed = tick1();
			if (t <= 6)
				xs += Common::String::format(" %d", layerX());
			if (t == 395) {
				x395 = layerX();
				count395 = c->getCountdown();
			}
			if (changed) {
				left = t;
				to = next;
			}
		}
		Common::String out = Common::String::format("x after ticks 1-6%s; after tick 395 x %d, countdown %d; left at tick %d for %s, x %d, toggle %d", xs.c_str(),
		                                            x395, count395, left, to.c_str(), layerX(), GhostSpotRoom::getToggle());
		if (!to.empty())
			showRoom(to);
		out += Common::String::format("; then in %s", _currentRoom->getName().c_str());
		debugC(3, kDebugScript, "ghost spot check pan: %s", out.c_str());
	}

	// A skip key after 10 ticks; the next visit, with the toggle where the
	// skip left it.
	{
		enterSpot(0);
		GhostSpotRoom *c = spot();
		for (uint t = 1; t <= 10; t++)
			tick1();
		const int before = c->getCountdown(), x = layerX();
		const bool taken = c->skip();
		const bool changed = tick1();
		Common::String out = Common::String::format("after 10 ticks, x %d, countdown %d: taken %d, left at tick 11 %d for %s, toggle %d", x, before, taken ? 1 : 0,
		                                            changed ? 1 : 0, next.c_str(), GhostSpotRoom::getToggle());
		if (!next.empty())
			showRoom(next);
		// Back to the ghost spot, its class kept.
		const int toggle = GhostSpotRoom::getToggle();
		showRoom(kGhost);
		c = spot();
		const int x0 = layerX();
		tick1();
		const int x1 = layerX();
		tick1();
		out += Common::String::format("; again, toggle %d: x %d, after a tick %d, after two %d, countdown %d", toggle, x0, x1, layerX(), c ? c->getCountdown() : -1);
		debugC(3, kDebugScript, "ghost spot check skip: %s", out.c_str());
	}

	// A click while the room holds input.
	{
		enterSpot(0);
		for (uint t = 1; t <= 3; t++)
			tick1();
		_mouseDown = false;
		handleMouse(true, Common::Point(320, 240));
		const bool changed = tick1();
		debugC(3, kDebugScript, "ghost spot check click: after 3 ticks, left at tick 4 %d for %s", changed ? 1 : 0, next.c_str());
	}

	// Where each title goes on to (game vtable +0x64), by the room loaded.
	{
		static const char *const kFrom[] = {
			"ROOM_Ghost_Spot", "ROOM_P29_Princess_Bedroom", "ROOM_P28_Veranda", "ROOM_P25_Wizards_Library", "ROOM_P24_Wizards_Bedroom",
			"ROOM_P26_Castle_Balcony", "ROOM_P27_Below_Balcony"
		};
		Common::String out;
		for (uint i = 0; i < ARRAYSIZE(kFrom); i++)
			out += Common::String::format("%s%s %s", i ? ", " : "", kFrom[i], GhostSpotRoom::getReturnRoom(getGameId(), kFrom[i]));
		debugC(3, kDebugScript, "ghost spot check towns: %s", out.c_str());
	}

	GhostSpotRoom::setToggle(0);
	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Scooby
