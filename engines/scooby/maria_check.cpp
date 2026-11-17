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
#include "scooby/maria_rooms.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

void ScoobyEngine::checkMaria() {
	// Phantom's Maria game (WP-064).
	const char *const kMariaRoom = "ROOM_Maria";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kMariaRoom;
	if (!found) {
		debugC(3, kDebugScript, "maria check: no Maria game");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		RoomClass *c = createRoomClass(*this, kMariaRoom);
		debugC(3, kDebugScript, "maria check classes: Maria %s", dynamic_cast<MariaGameRoom *>(c) ? "game" : "-");
		delete c;
	}
	const int savedDifficulty = _difficulty;
	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto game = [&]() { return dynamic_cast<MariaGameRoom *>(getRoomClass(kMariaRoom)); };
	auto start = [&](int level) {
		newGameForCheck();
		_difficulty = level;
		showRoom(kMariaRoom);
		_sound->stop();
		return game();
	};
	auto mariaState = [&]() {
		MariaGameRoom *g = game();
		const int m = g->getMaria();
		if (m < 0)
			return Common::String("none");
		return Common::String::format("%s step %u", _currentRoom->getAnim(m).c_str(), _currentRoom->getStep(m));
	};
	auto playing = [&](const char *clip) { return _sound->isClipPlaying(clip) ? 1 : 0; };

	// The items each level generates, 40 times.
	{
		Common::String out;
		for (int level = 0; level < 3; level++) {
			bool count = true, split = true, floors = true, xs = true, depths = true, drawn = true, weights = true;
			for (int k = 0; k < 40; k++) {
				MariaGameRoom *g = start(level);
				Common::Array<MariaGameRoom::Item> &items = g->getItems();
				count = count && (int)items.size() == level * 2 + 6 && g->getTotal() == level * 2 + 6;
				int sum = 0;
				for (uint i = 0; i < items.size(); i++) {
					const MariaGameRoom::Item &it = items[i];
					sum += it.weight;
					weights = weights && (it.weight == 1 || it.weight == 2 || it.weight == 4) && (it.sound == (it.weight == 4 ? "Plate" : "Cup"));
					floors = floors && it.floor >= 443 && it.floor <= 469 && it.y == it.floor && it.resting;
					xs = xs && it.x >= 20 && it.x <= 619;
					depths = depths && _currentRoom->getDepth(it.sprite) == (int)it.floor - 393;
					drawn = drawn && _currentRoom->hasFrame(it.sprite) && _currentRoom->getAnim(it.sprite) == "ANIM_" + it.name + "IDLE";
				}
				// A split into two equal pans exists.
				bool can[64] = { true };
				for (uint i = 0; i < items.size(); i++) {
					for (int s = 63; s >= items[i].weight; s--)
						can[s] = can[s] || can[s - items[i].weight];
				}
				split = split && sum % 2 == 0 && sum / 2 < 64 && can[sum / 2];
			}
			out += Common::String::format("%slevel %d: %d items %d, weights 1 2 4 by kind %d, a balanced split %d, floors 443-469 resting %d, x 20-619 %d, depth floor - 393 %d, drawn %d",
			                              level ? "; " : "", level, level * 2 + 6, count ? 1 : 0, weights ? 1 : 0, split ? 1 : 0, floors ? 1 : 0, xs ? 1 : 0,
			                              depths ? 1 : 0, drawn ? 1 : 0);
		}
		debugC(3, kDebugScript, "maria check items: %s", out.c_str());
	}

	// The room: Maria, the way out, the timers.
	{
		MariaGameRoom *g = start(0);
		const int m = g->getMaria();
		int exitSprite = -1;
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == "OBJ_MARIAGAME_EXIT")
				exitSprite = s;
		}
		const Common::String maria = m >= 0 ? Common::String::format("%s mode %d step %u depth %d", _currentRoom->getAnim(m).c_str(), _currentRoom->getMode(m),
		                                                             _currentRoom->getStep(m), _currentRoom->getDepth(m))
		                                    : Common::String("none");
		const Common::String way = exitSprite >= 0 ? _currentRoom->getDestination(exitSprite) : Common::String("none");
		const int timer = g->getExitTimer();
		const int rebalancing = g->isRebalancing() ? 1 : 0;
		tick1();
		const int after = g->isRebalancing() ? 1 : 0;
		debugC(3, kDebugScript, "maria check room: Maria %s; the way out to %s; timer %d; rebalancing %d, after a tick %d, Maria %s", maria.c_str(), way.c_str(), timer,
		       rebalancing, after, mariaState().c_str());
	}

	// A throw: from (300, 200) at (4, 0) to its floor at 460.
	{
		MariaGameRoom *g = start(0);
		g->clearItemsForCheck();
		const int i = g->addItemForCheck(460, 0, 1, 300);
		MariaGameRoom::Item &it = g->getItems()[i];
		it.x = 300;
		it.y = 200;
		it.vx = 4;
		it.vy = 0;
		it.resting = false;
		int landed = -1, spin = 0;
		Common::String at;
		for (int t = 1; t <= 60 && landed < 0; t++) {
			tick1();
			MariaGameRoom::Item &now = g->getItems()[i];
			spin = spin || playing("Maria.Spin");
			if (now.resting) {
				landed = t;
				at = Common::String::format("(%d, %d)", (int)now.x, (int)now.y);
			}
		}
		const int spinAfterLanding = playing("Maria.Spin");
		tick1();
		const int floorSound = playing("Maria.Floor.Cup.1") || playing("Maria.Floor.Cup.2");
		const int spinAfter = playing("Maria.Spin");
		debugC(3, kDebugScript, "maria check throw: landed at tick %d at %s; Maria.Spin while flying %d, as it lands %d, a tick on %d; Maria.Floor.Cup a tick on %d", landed,
		       at.c_str(), spin, spinAfterLanding, spinAfter, floorSound);
	}

	// Off the right wall.
	{
		MariaGameRoom *g = start(0);
		g->clearItemsForCheck();
		const int i = g->addItemForCheck(460, 1, 0, 630);
		MariaGameRoom::Item &it = g->getItems()[i];
		it.y = 200;
		it.vx = 10;
		it.vy = 0;
		it.resting = false;
		tick1();
		const MariaGameRoom::Item &a = g->getItems()[i];
		const Common::String one = Common::String::format("x %g vx %g, Maria.Bounce %d", a.x, a.vx, playing("Maria.Bounce"));
		tick1();
		const MariaGameRoom::Item &b = g->getItems()[i];
		debugC(3, kDebugScript, "maria check wall: tick 1: %s; tick 2: x %g", one.c_str(), b.x);
	}

	// Taken, carried and thrown.
	{
		MariaGameRoom *g = start(0);
		g->clearItemsForCheck();
		const int i = g->addItemForCheck(460, 2, 1, 300);
		const int s = g->getItems()[i].sprite;
		_mousePos = Common::Point(310, 455);
		g->click(s);
		tick1();
		const MariaGameRoom::Item &a = g->getItems()[i];
		Common::String out = Common::String::format("taken: carried %d, cursor hidden %d, offset (%g, %g)", a.dragging ? 1 : 0, g->hidesCursor() ? 1 : 0, a.offX, a.offY);
		static const int kPath[][2] = { { 330, 445 }, { 350, 435 }, { 370, 425 } };
		for (uint k = 0; k < ARRAYSIZE(kPath); k++) {
			_mousePos = Common::Point(kPath[k][0], kPath[k][1]);
			if (k == ARRAYSIZE(kPath) - 1)
				g->click(-1);
			tick1();
		}
		const MariaGameRoom::Item &b = g->getItems()[i];
		out += Common::String::format("; let go at (%g, %g): carried %d, resting %d, pan %d, speed (%g, %g), %s mode %d, Maria.Throw.Cup %d, cursor hidden %d", b.x, b.y,
		                              b.dragging ? 1 : 0, b.resting ? 1 : 0, b.pan, b.vx, b.vy, _currentRoom->getAnim(s).c_str(), _currentRoom->getMode(s),
		                              playing("Maria.Throw.Cup"), g->hidesCursor() ? 1 : 0);
		debugC(3, kDebugScript, "maria check carry: %s", out.c_str());
	}

	// Into the first pan: Maria tilts, then is HAPPY again.
	{
		MariaGameRoom *g = start(0);
		g->clearItemsForCheck();
		for (int t = 0; t < 40; t++)
			tick1();
		const int i = g->addItemForCheck(460, 0, 1, 200);
		MariaGameRoom::Item &it = g->getItems()[i];
		it.y = 120;
		it.resting = false;
		it.pan = g->panAt(200, 120, it.panX, it.panY);
		Common::String out = Common::String::format("from (200, 120): pan %d at (%g, %g)", it.pan, it.panX, it.panY);
		int landed = -1, happy = -1;
		Common::String tilt;
		for (int t = 1; t <= 80; t++) {
			tick1();
			const MariaGameRoom::Item &now = g->getItems()[i];
			if (landed < 0 && now.resting) {
				landed = t;
				tilt = Common::String::format("landed at tick %d: pan 1 holds %u, Maria.Place.Cup %d, Maria %s, the cup at (%g, %g) depth %d", t, g->getPan(1).size(),
				                              playing("Maria.Place.Cup"), mariaState().c_str(), now.x, now.y, _currentRoom->getDepth(now.sprite));
			}
			if (landed > 0 && t == landed + 1)
				tilt += Common::String::format("; a tick on rebalancing %d", g->isRebalancing() ? 1 : 0);
			if (landed > 0 && happy < 0 && _currentRoom->getAnim(g->getMaria()) == "ANIM_MARIAGAME_MARIAHAPPY")
				happy = t;
		}
		out += Common::String::format("; %s; HAPPY again at tick %d, %s", tilt.c_str(), happy, mariaState().c_str());
		debugC(3, kDebugScript, "maria check pan: %s", out.c_str());
	}

	// Only a pan's top item can be taken.
	{
		MariaGameRoom *g = start(0);
		g->clearItemsForCheck();
		const int a = g->addItemForCheck(460, 0, 1, 100);
		const int b = g->addItemForCheck(460, 1, 1, 150);
		g->placeOnPan(a, 1);
		g->placeOnPan(b, 1);
		Common::String out = Common::String::format("two in pan 1: the bottom one may be taken %d, the top %d", g->isPickable(a) ? 1 : 0, g->isPickable(b) ? 1 : 0);
		g->click(g->getItems()[a].sprite);
		tick1();
		out += Common::String::format("; the bottom clicked: carried %d", g->getItems()[a].dragging ? 1 : 0);
		g->click(g->getItems()[b].sprite);
		tick1();
		out += Common::String::format("; the top clicked: carried %d, pan 1 holds %u, its depth %d", g->getItems()[b].dragging ? 1 : 0, g->getPan(1).size(),
		                              _currentRoom->getDepth(g->getItems()[b].sprite));
		debugC(3, kDebugScript, "maria check top: %s", out.c_str());
	}

	// Tipped over: a plate and a cup, 5, in the first pan.
	{
		MariaGameRoom *g = start(0);
		g->clearItemsForCheck();
		tick1();
		const int a = g->addItemForCheck(450, 0, 2, 100);
		const int b = g->addItemForCheck(460, 1, 1, 150);
		g->placeOnPan(a, 1);
		g->placeOnPan(b, 1);
		Common::String steps;
		int fell = -1;
		for (int t = 1; t <= 6; t++) {
			tick1();
			steps += Common::String::format("%s%u", t > 1 ? " " : "", _currentRoom->getStep(g->getMaria()));
			if (fell < 0 && g->getPan(1).empty())
				fell = t;
		}
		const MariaGameRoom::Item &ia = g->getItems()[a];
		const MariaGameRoom::Item &ib = g->getItems()[b];
		debugC(3, kDebugScript, "maria check tip: Maria's steps %s; fell off at tick %d; pans %u %u; resting %d %d; depths %d %d; rebalancing %d", steps.c_str(), fell,
		       g->getPan(1).size(), g->getPan(2).size(), ia.resting ? 1 : 0, ib.resting ? 1 : 0, _currentRoom->getDepth(ia.sprite), _currentRoom->getDepth(ib.sprite),
		       g->isRebalancing() ? 1 : 0);
	}

	// Won: every item in the pans, balanced.
	{
		MariaGameRoom *g = start(0);
		tick1();
		Common::Array<MariaGameRoom::Item> &items = g->getItems();
		int sum = 0;
		for (uint i = 0; i < items.size(); i++)
			sum += items[i].weight;
		// A subset of half the weight goes in the first pan.
		int from[64];
		for (int s = 0; s < 64; s++)
			from[s] = -2;
		from[0] = -1;
		for (uint i = 0; i < items.size(); i++) {
			for (int s = 63; s >= items[i].weight; s--) {
				if (from[s] == -2 && from[s - items[i].weight] != -2)
					from[s] = i;
			}
		}
		Common::Array<bool> first(items.size(), false);
		for (int s = sum / 2; s > 0 && from[s] >= 0; s -= items[from[s]].weight)
			first[from[s]] = true;
		for (uint i = 0; i < items.size(); i++) {
			if (first[i])
				g->placeOnPan(i, 1);
		}
		for (uint i = 0; i < items.size(); i++) {
			if (!first[i])
				g->placeOnPan(i, 2);
		}
		const uint pan1 = g->getPan(1).size(), pan2 = g->getPan(2).size();
		int won = -1, wrapped = -1;
		Common::String state, dest;
		for (int t = 1; t <= 400 && dest.empty(); t++) {
			tick1();
			if (won < 0 && getGameFlag("MariaPuzzle")) {
				won = t;
				state = Common::String::format("MariaPuzzle %s, held %d, timer %d", getGameString("MariaPuzzle").c_str(), g->isBusy() ? 1 : 0, g->getExitTimer());
			}
			if (won > 0 && wrapped < 0 && g->getExitTimer() == 129600)
				wrapped = t;
			dest = next;
		}
		debugC(3, kDebugScript, "maria check win: every item in a pan %d; won at tick %d: %s; the way back asked at tick %d; then to %s",
		       (int)(pan1 + pan2) == g->getTotal() ? 1 : 0, won, state.c_str(), wrapped, dest.c_str());
	}

	_difficulty = savedDifficulty;
	newGameForCheck();
	_script->setGhostChance(&_randomSource, ConfMan.hasKey("ghost_chance") ? (uint)ConfMan.getInt("ghost_chance") : 30);
	_checking = false;
}

} // End of namespace Scooby
