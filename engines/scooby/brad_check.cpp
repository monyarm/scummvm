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

// The script-level check of Phantom's Brad game (WP-070), against an
// independent reference written from the trace.

#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/brad_rooms.h"
#include "scooby/detection.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

void ScoobyEngine::checkBrad() {
	static const char *const kRooms[] = { "ROOM_Brad_SPOOKY", "ROOM_Brad_SPOOKIER", "ROOM_Brad_SPOOKTACULAR" };
	static const char *const kLevelNames[] = { "EASY", "MED", "HARD" };
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kRooms[0];
	if (!found) {
		debugC(3, kDebugScript, "brad check: no Brad game");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);
	{
		Common::String out;
		for (uint l = 0; l < 3; l++) {
			RoomClass *c = createRoomClass(*this, kRooms[l]);
			out += Common::String::format("%s%s %s", l ? "; " : "", kRooms[l] + 10, dynamic_cast<BradGameRoom *>(c) ? "Brad's game" : "-");
			delete c;
		}
		debugC(3, kDebugScript, "brad check classes: %s", out.c_str());
	}

	Common::String next;
	int movie;
	auto tick1 = [&]() {
		next.clear();
		movie = -1;
		return tickOnce(next, movie);
	};
	auto game = [&]() { return dynamic_cast<BradGameRoom *>(_roomClass); };
	auto story = [&](int level) {
		newGameForCheck();
		setGameString("Whodunit", "JESTER");
		setDifficulty(level);
		_mouseDown = false;
		_sound->stop();
	};
	auto enter = [&](int level, int room) {
		story(level);
		showRoom(kRooms[room]);
		return game();
	};
	auto find = [&](const char *name) {
		for (uint s = 0; s < _currentRoom->getSpriteCount(); s++) {
			if (_currentRoom->getObjectName(s) == name)
				return (int)s;
		}
		return -1;
	};
	auto f2 = [](double v) { return Common::String::format("%.2f", v); };
	auto quiet = [&](BradGameRoom *g) {
		// The introduction stopped, and its tick.
		_sound->stop();
		tick1();
		return g->isIntroDone();
	};

	// The placements: each level's pieces where they hang, by their
	// sequence's digit, at their depth in the order; the body, heads and
	// mouths under the root; another level's pieces not built.
	{
		Common::String out;
		for (int l = 0; l < 3; l++) {
			BradGameRoom *g = enter(l, l);
			Room &r = *_currentRoom;
			out += Common::String::format("%slevel %d: %u pieces:", l ? "; " : "", l, g->getCount());
			for (uint i = 0; i < BradGameRoom::kMaxPieces; i++) {
				const BradGameRoom::Piece &p = g->getPiece(i);
				if (p.sprite < 0)
					continue;
				out += Common::String::format(" %u %s (%d,%d) depth %d hit %d root %d%s", i, r.getAnim(p.sprite).c_str() + 15, r.getOffsetX(p.sprite),
				                              r.getOffsetY(p.sprite), r.getDepth(p.sprite), r.getHitType(p.sprite), r.getParent(p.sprite) == 0 ? 1 : 0,
				                              r.getRollover(p.sprite) == "ANIM_CURSOROPEN" ? " open" : "");
			}
			static const char *const kParts[] = { "OBJ_BRADARMOR_BODY", "OBJ_BRADARMOR_HEADS", "OBJ_BRADARMOR_MOUTHS" };
			for (uint k = 0; k < 3; k++) {
				const int s = find(kParts[k]);
				out += Common::String::format("; %s", kParts[k] + 14);
				if (s >= 0)
					out += Common::String::format(" depth %d root %d mode %d", r.getDepth(s), r.getParent(s) == 0 ? 1 : 0, r.getMode(s));
				else
					out += " none";
			}
			const int ex = find("OBJ_BRAD_EXIT");
			out += Common::String::format("; the way out to %s", ex >= 0 ? r.getDestination(ex).c_str() : "-");
			// Another level's game in this room: its pieces not built.
			g = enter((l + 1) % 3, l);
			const Common::String obj = Common::String::format("OBJ_BRADARMOR_%s", kLevelNames[l]);
			out += Common::String::format("; at level %d: %u pieces, %s built %d", (l + 1) % 3, g->getCount(), obj.c_str() + 14, find(obj.c_str()) >= 0 ? 1 : 0);
		}
		debugC(3, kDebugScript, "brad check placements: %s", out.c_str());
	}

	// The links, each piece's neighbours as its list runs.
	{
		Common::String out;
		for (int l = 0; l < 3; l++) {
			BradGameRoom *g = enter(l, l);
			out += Common::String::format("%slevel %d:", l ? "; " : "", l);
			for (uint i = 0; i < g->getCount(); i++) {
				const BradGameRoom::Piece &p = g->getPiece(i);
				out += Common::String::format(" %u[", i);
				for (uint k = 0; k < p.links.size(); k++)
					out += Common::String::format("%s%d", k ? " " : "", p.links[k]);
				out += "]";
			}
		}
		debugC(3, kDebugScript, "brad check links: %s", out.c_str());
	}

	// Entry: the order, each piece's way out (in degrees), the introduction
	// with the cursor hidden, the hour; the introduction over, the cursor.
	{
		Common::String out;
		for (int l = 0; l < 3; l++) {
			BradGameRoom *g = enter(l, l);
			out += Common::String::format("%slevel %d: order", l ? "; " : "", l);
			for (uint k = 0; k < g->getCount(); k++)
				out += Common::String::format(" %d", g->getOrder(k));
			out += ", ways";
			for (uint i = 0; i < g->getCount(); i++)
				out += Common::String::format(" %.1f", g->getPiece(i).rest * 180.0 / 3.14159265358979);
			out += Common::String::format(", shares");
			for (uint i = 0; i < g->getCount(); i++)
				out += Common::String::format(" %.3f", g->getPiece(i).share);
			out += Common::String::format(", '%s' playing %d, cursor hidden %d, busy %d, timer %d", g->getVoice().c_str(), _sound->isClipPlaying(g->getVoice()) ? 1 : 0,
			                              g->hidesCursor() ? 1 : 0, g->isBusy() ? 1 : 0, g->getExitTimer());
			const bool done = quiet(g);
			out += Common::String::format("; stopped: over %d, cursor hidden %d", done ? 1 : 0, g->hidesCursor() ? 1 : 0);
		}
		debugC(3, kDebugScript, "brad check entry: %s", out.c_str());
	}

	// Left alone, nothing moves; a click on a piece is the game's, on the
	// body not.
	{
		BradGameRoom *g = enter(0, 0);
		quiet(g);
		bool still = true;
		for (int t = 0; t < 300; t++) {
			tick1();
			for (uint i = 0; i < g->getCount(); i++) {
				const BradGameRoom::Piece &p = g->getPiece(i);
				still = still && p.x == p.ax && p.y == p.ay;
			}
		}
		const int body = find("OBJ_BRADARMOR_BODY");
		const bool onPiece = _roomClass->click(g->getPiece(g->current()).sprite);
		const bool onBody = _roomClass->click(body);
		tick1();
		debugC(3, kDebugScript, "brad check still: 300 ticks: where they hang %d, a room change %d; a click on a piece taken %d, on the body %d", still ? 1 : 0,
		       next.empty() ? 0 : 1, onPiece ? 1 : 0, onBody ? 1 : 0);
	}

	// The spring: the current piece moved 30 right of where it hangs; a tick
	// later, where it and its neighbours are, and a rattle; then how long
	// until the pile is back within half a pixel.
	{
		BradGameRoom *g = enter(0, 0);
		quiet(g);
		const int c = g->current();
		BradGameRoom::Piece &p = g->getPiece(c);
		g->placeForCheck(c, p.ax + 30.0f, p.ay);
		tick1();
		bool rattled = false;
		for (int n = 1; n <= 5; n++)
			rattled = rattled || _sound->isClipPlaying(Common::String::format("Brad.Armor.%d", n));
		Common::String out = Common::String::format("piece %d at (%s, %s)", c, f2(p.x - p.ax).c_str(), f2(p.y - p.ay).c_str());
		for (uint i = 0; i < g->getCount(); i++) {
			if ((int)i == c)
				continue;
			const BradGameRoom::Piece &q = g->getPiece(i);
			out += Common::String::format(", %u (%s, %s)", i, f2(q.x - q.ax).c_str(), f2(q.y - q.ay).c_str());
		}
		out += Common::String::format("; a rattle %d", rattled ? 1 : 0);
		int settled = -1;
		for (int t = 1; t <= 500 && settled < 0; t++) {
			tick1();
			bool all = true;
			for (uint i = 0; i < g->getCount(); i++) {
				const BradGameRoom::Piece &q = g->getPiece(i);
				all = all && fabs(q.x - q.ax) < 0.5 && fabs(q.y - q.ay) < 0.5;
			}
			if (all)
				settled = t;
		}
		out += Common::String::format("; settled after %d", settled);
		debugC(3, kDebugScript, "brad check spring: %s", out.c_str());
	}

	// Pulling a piece whose turn it isn't: held at 20; each fourth time,
	// the heads excited and Brad's next complaint unless a line plays; a
	// new piece starts over; the heads calm 72 ticks on.
	{
		BradGameRoom *g = enter(0, 0);
		quiet(g);
		Common::String out;
		auto tug = [&](int w, bool back) {
			BradGameRoom::Piece &q = g->getPiece(w);
			g->placeForCheck(w, q.ax + 30.0f, q.ay);
			tick1();
			const Common::String held = f2(q.x - q.ax);
			if (back) {
				g->placeForCheck(w, q.ax, q.ay);
				tick1();
			}
			return held;
		};
		auto round4 = [&](int w, bool lastBack) {
			Common::String held;
			for (int k = 0; k < 4; k++)
				held += Common::String::format("%s%s", k ? " " : "", tug(w, k < 3 || lastBack).c_str());
			const Common::String heads = _currentRoom->getAnim(g->getHeads());
			return Common::String::format("piece %d held at %s, heads %s, line %s playing %d, next %s", w, held.c_str(), heads.c_str() + 15, g->getVoice().c_str(),
			                              _sound->isClipPlaying(g->getVoice()) ? 1 : 0, g->getNextVoice().c_str());
		};
		const int w = g->getOrder(1);
		out += round4(w, true);
		// A line still playing: no complaint.
		out += "; again, the line playing: " + round4(w, true);
		_sound->stop();
		out += "; stopped: " + round4(w, true);
		_sound->stop();
		out += "; " + round4(w, true);
		_sound->stop();
		out += "; another, " + round4(g->getOrder(2), false);
		int calm = -1;
		for (int t = 1; t <= 200 && calm < 0; t++) {
			tick1();
			if (_currentRoom->getAnim(g->getHeads()) == "ANIM_BRADARMOR_HEADSNORM")
				calm = t;
		}
		out += Common::String::format("; the heads calm after %d", calm);
		debugC(3, kDebugScript, "brad check complaints: %s", out.c_str());
	}

	// The game at each level: each piece in turn dragged out its way, 2 a
	// tick, until it breaks loose; let go, until it is off the screen. The
	// last gone, his game won, and back to the arena.
	{
		Common::String out;
		for (int l = 0; l < 3; l++) {
			BradGameRoom *g = enter(l, l);
			quiet(g);
			out += Common::String::format("%slevel %d:", l ? "; " : "", l);
			bool failed = false;
			while (!g->isWon() && !failed) {
				const int i = g->current();
				BradGameRoom::Piece &p = g->getPiece(i);
				// A point on it, well inside it.
				Common::Point grab(-1, -1);
				int fx, fy, fw, fh;
				if (_currentRoom->getFrameRect(p.sprite, fx, fy, fw, fh)) {
					const int cx = fx + fw / 2, cy = fy + fh / 2;
					for (int rad = 0; rad < 60 && grab.x < 0; rad++) {
						for (int oy = -rad; oy <= rad && grab.x < 0; oy++) {
							for (int ox = -rad; ox <= rad && grab.x < 0; ox++) {
								if (MAX(ABS(ox), ABS(oy)) != rad)
									continue;
								bool ok = true;
								for (int dy = -4; dy <= 4 && ok; dy += 2) {
									for (int dx = -4; dx <= 4 && ok; dx += 2)
										ok = _currentRoom->hitTest(cx + ox + dx, cy + oy + dy) == p.sprite;
								}
								if (ok)
									grab = Common::Point(cx + ox, cy + oy);
							}
						}
					}
				}
				if (grab.x < 0) {
					out += Common::String::format(" %d no grip", i);
					failed = true;
					break;
				}
				const double way = p.rest;
				_mousePos = grab;
				_mouseDown = true;
				_roomClass->click(p.sprite);
				tick1();
				int loose = -1;
				for (int t = 1; t <= 100 && loose < 0; t++) {
					_mousePos = Common::Point(grab.x + (int)floor(2.0 * t * cos(way) + 0.5), grab.y + (int)floor(2.0 * t * sin(way) + 0.5));
					tick1();
					if (p.loose)
						loose = t;
				}
				_mouseDown = false;
				int gone = -1;
				for (int t = 1; t <= 200 && gone < 0 && loose > 0; t++) {
					tick1();
					if (p.gone)
						gone = t;
				}
				out += Common::String::format(" %d loose %d gone %d,", i, loose, gone);
				if (gone < 0)
					failed = true;
			}
			out += Common::String::format(" won %d: BradPuzzle %d, busy %d, cursor hidden %d", g->isWon() ? 1 : 0, getGameFlag("BradPuzzle") ? 1 : 0,
			                              g->isBusy() ? 1 : 0, g->hidesCursor() ? 1 : 0);
			int back = -1;
			for (int t = 1; t <= 200 && back < 0; t++) {
				tick1();
				if (!next.empty())
					back = t;
			}
			out += Common::String::format(", to %s after %d", next.empty() ? "-" : next.c_str(), back);
		}
		debugC(3, kDebugScript, "brad check game: %s", out.c_str());
	}
	_mouseDown = false;
	_checking = false;
}

} // End of namespace Scooby
