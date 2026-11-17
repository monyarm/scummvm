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

// Phantom of the Knight's Brad game. The addresses are Phantom's.

#include "common/debug.h"

#include "scooby/brad_rooms.h"
#include "scooby/detection.h"
#include "scooby/room.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kRooms[] = { "ROOM_Brad_SPOOKY", "ROOM_Brad_SPOOKIER", "ROOM_Brad_SPOOKTACULAR" };
static const char *const kArena2 = "ROOM_P14_Arena2";
static const char *const kPrefix = "OBJ_BRADARMOR_";
static const char *const kLevels[] = { "EASY", "MED", "HARD" };
static const char *const kIntro = "Brad.ARMOR_INTRO_1";
static const int kHour = 0x1FA40; // 129600 ticks

// The game's float constants (0x45EF04-0x45EF44, 0x45E590).
static const double kPi = 3.1415927f;
static const double kTwoPi = 6.2831855f;
static const double kInvPi = 0.31830987f;
static const double kPull = 1.5f;

// Each level's order, and its pieces' links, by the last digit of each
// piece's sequence less one (0x41EE80).
static const int kOrders[3][BradGameRoom::kMaxPieces] = {
	{ 0, 2, 4, 5, 1, 3 },
	{ 2, 1, 6, 4, 3, 7, 0, 5 },
	{ 1, 2, 3, 8, 5, 4, 0, 9, 7, 6 }
};
static const int kLinks0[][2] = { { 0, 2 }, { 0, 4 }, { 0, 5 }, { 1, 3 }, { 2, 3 }, { 2, 4 }, { 3, 4 }, { 3, 5 } };
static const int kLinks1[][2] = { { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 1, 3 }, { 1, 5 },
                                  { 1, 7 }, { 2, 4 }, { 2, 5 }, { 2, 7 }, { 3, 5 }, { 4, 5 }, { 4, 6 } };
static const int kLinks2[][2] = { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 6 }, { 0, 7 }, { 0, 8 }, { 0, 9 }, { 1, 8 }, { 1, 9 }, { 2, 3 },
                                  { 3, 6 }, { 4, 5 }, { 4, 6 }, { 4, 7 }, { 4, 8 }, { 5, 6 }, { 6, 7 }, { 6, 9 }, { 8, 9 } };

static int pieceIndex(const Common::String &anim) {
	// 0x41EA60: the sequence's last digit, less one; 0 is the tenth.
	if (anim.empty())
		return -1;
	const int c = (int)(char)(anim.lastChar() - '0');
	return ((c + 9) % 10 + 10) % 10;
}

static int levelOf(const Common::String &suffix) {
	for (int l = 0; l < 3; l++) {
		if (suffix == kLevels[l])
			return l;
	}
	return -1;
}

void BradGameRoom::reset() {
	// 0x41EA40.
	_count = 0;
	_lastWrong = -1;
}

void BradGameRoom::roomLeft() {
	// 0x41F3A0, after the base leave.
	_count = 0;
	_lastWrong = -1;
}

void BradGameRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x41EA60, for OBJ_BRADARMOR_<suffix>; anything else the default path
	// builds.
	// - A piece of another level than the game's (EASY 0, MED 1, HARD 2):
	//   claimed, not built.
	// - A piece of this level: a piece (0x41DAB0) under the second layer
	//   (game +0x5C; no room has one, so the root) at depth 10, looping, hit
	//   by its cels; counted (+0xB8).
	// - BODY, HEADS (+0x120) and MOUTHS (+0x124): sprites under the root
	//   (parent 0) at depths -10, -9 and -8; the mouths stopped (enter()).
	// - Any other suffix: the default path.
	if (!object.hasPrefix(kPrefix))
		return;
	const Common::String suffix(object.c_str() + strlen(kPrefix));
	const int level = levelOf(suffix);
	if (level >= 0) {
		if (level != (int)_engine.getDifficulty()) {
			claim.build = Room::Claim::kSkip;
			return;
		}
		claim.build = Room::Claim::kSprite;
		claim.layer = 1;
		claim.depth = 10;
		claim.hitType = 1;
		_count++;
		return;
	}
	int depth;
	if (suffix == "BODY")
		depth = -10;
	else if (suffix == "HEADS")
		depth = -9;
	else if (suffix == "MOUTHS")
		depth = -8;
	else
		return;
	claim.build = Room::Claim::kSprite;
	claim.layer = 1;
	claim.depth = depth;
	claim.hitType = 1;
}

int BradGameRoom::findPiece(int sprite) const {
	if (sprite < 0)
		return -1;
	for (uint i = 0; i < kMaxPieces; i++) {
		if (_pieces[i].sprite == sprite)
			return i;
	}
	return -1;
}

void BradGameRoom::sync(const Piece &p) {
	if (p.sprite >= 0)
		_engine.getRoom()->setOffset(p.sprite, (int)p.x, (int)p.y);
}

void BradGameRoom::placeForCheck(uint i, float x, float y) {
	_pieces[i].x = x;
	_pieces[i].y = y;
	sync(_pieces[i]);
}

void BradGameRoom::link(int a, int b) {
	// 0x41E460 with both ways, each at the head of the other's list
	// (0x41E760).
	if (_pieces[a].sprite < 0 || _pieces[b].sprite < 0)
		return;
	_pieces[a].links.insert_at(0, b);
	_pieces[b].links.insert_at(0, a);
}

void BradGameRoom::restAngles() {
	// 0x41F6F0: the middle of the pieces still on the pile, where they are
	// now; each one's way out (0x41DD10) from it to where it hangs.
	float sumX = 0.0f, sumY = 0.0f;
	for (uint k = _next; k < _count; k++) {
		const Piece &p = _pieces[_order[k]];
		sumX = p.x + sumX;
		sumY = sumY + p.y;
	}
	const float n = (float)(_count - _next);
	for (uint k = _next; k < _count; k++) {
		Piece &p = _pieces[_order[k]];
		p.rest = (float)atan2((double)p.ay - (double)(sumY / n), (double)p.ax - (double)(sumX / n));
	}
}

void BradGameRoom::enter(const Common::String &from) {
	// 0x41EE80, after the base entry (the toolbar hidden, 0x407CC0, is the
	// fork's toolbar list).
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	const int level = (int)_engine.getDifficulty();
	for (uint i = 0; i < kMaxPieces; i++)
		_pieces[i] = Piece();
	_heads = _mouths = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &object = room.getObjectName(s);
		if (!object.hasPrefix(kPrefix))
			continue;
		const Common::String suffix(object.c_str() + strlen(kPrefix));
		if (suffix == "HEADS") {
			_heads = s;
		} else if (suffix == "MOUTHS") {
			_mouths = s;
			room.setPlayer(s, 0, 0, 3);
		} else if (levelOf(suffix) == level) {
			const int i = pieceIndex(room.getAnim(s));
			if (i < 0)
				continue;
			Piece &p = _pieces[i];
			p.sprite = s;
			p.x = p.ax = p.prevX = (float)room.getOffsetX(s);
			p.y = p.ay = p.prevY = (float)room.getOffsetY(s);
			room.setActor(s, this);
			// 0x41E6C0, the piece's vtable +0x18: the open hand.
			room.setRollover(s, "ANIM_CURSOROPEN");
		}
	}

	// The level's order and links.
	if (level >= 0 && level < 3) {
		for (uint i = 0; i < kMaxPieces; i++)
			_order[i] = kOrders[level][i];
		if (level == 0) {
			for (uint k = 0; k < ARRAYSIZE(kLinks0); k++)
				link(kLinks0[k][0], kLinks0[k][1]);
		} else if (level == 1) {
			for (uint k = 0; k < ARRAYSIZE(kLinks1); k++)
				link(kLinks1[k][0], kLinks1[k][1]);
		} else {
			for (uint k = 0; k < ARRAYSIZE(kLinks2); k++)
				link(kLinks2[k][0], kLinks2[k][1]);
		}
	}
	// Each piece's share of the order, and its depth: the first on top
	// (0x431C10).
	for (uint i = 0; i < _count && i < kMaxPieces; i++) {
		Piece &p = _pieces[_order[i]];
		p.share = (float)(i + 1) / (float)_count;
		if (p.sprite >= 0)
			room.setDepth(p.sprite, (int)(_count - i));
	}
	_next = 0;
	restAngles();
	// Brad's introduction, the cursor hidden until it is over, and an hour
	// before the game gives up and goes back to the arena.
	_introDone = false;
	_won = false;
	sound.play(kIntro);
	_voice = kIntro;
	_exitTimer = kHour;
	_pressed = false;
	_pressedOn = -1;
	debugC(1, kDebugScript, "brad: from %s, level %d, %u pieces", from.c_str(), level, _count);
}

bool BradGameRoom::click(int sprite) {
	// The pieces read the button going down (game +0x20 bit 2) on their
	// sprite (+0x08) in their updates.
	const int i = findPiece(sprite);
	if (i < 0 || _pieces[i].gone)
		return false;
	_pressed = true;
	_pressedOn = sprite;
	return true;
}

void BradGameRoom::complain(int i) {
	// 0x41F5C0: a piece not its turn pulled past 20. The same piece four
	// times: Brad's heads excited for 72 ticks, and his next complaint unless
	// a line is still playing (1, then 2, 3, 2, 3...). Another piece starts
	// the count and the complaints again.
	Sound &sound = *_engine.getSound();
	if (i != _lastWrong) {
		_lastWrong = i;
		_wrongCount = 0;
		_nextVoice = "Brad.ARMOR_RESPONSE_1";
	}
	if (++_wrongCount < 4)
		return;
	if (_heads >= 0)
		_engine.getRoom()->startAnim(_heads, "ANIM_BRADARMOR_HEADSEXCITED", 1, -1);
	_excitedTimer = 72;
	if (!sound.isClipPlaying(_voice)) {
		sound.play(_nextVoice);
		_voice = _nextVoice;
		if (_voice == "Brad.ARMOR_RESPONSE_1")
			_nextVoice = "Brad.ARMOR_RESPONSE_2";
		else if (_voice == "Brad.ARMOR_RESPONSE_2")
			_nextVoice = "Brad.ARMOR_RESPONSE_3";
		else if (_voice == "Brad.ARMOR_RESPONSE_3")
			_nextVoice = "Brad.ARMOR_RESPONSE_2";
		debugC(1, kDebugScript, "brad: %s", _voice.c_str());
	}
	_wrongCount = 0;
}

void BradGameRoom::pieceGone(int i) {
	// 0x41F570: the next piece's turn. The last: the game won (BradPuzzle,
	// game +0x256), the cursor faded out over 18 ticks, input held, and back
	// to the arena in 72 ticks; else each piece left its way out again.
	_next++;
	if (_next == _count) {
		_exitTimer = 72;
		_engine.setGameFlag("BradPuzzle", true);
		_won = true;
		debugC(1, kDebugScript, "brad: the last piece off, his game won");
		return;
	}
	restAngles();
}

void BradGameRoom::rattle(int i) {
	// 0x41E4B0, in the room's tick: a piece still on the pile that moved
	// more than 4 since the last tick rattles (Brad.Armor.<rand() % 5 + 1>).
	Piece &p = _pieces[i];
	if (!p.loose) {
		const double dy = (double)p.y - p.prevY;
		const double dx = (double)p.x - p.prevX;
		if (4.0 < sqrt(dy * dy + dx * dx))
			_engine.getSound()->play(Common::String::format("Brad.Armor.%d", _engine.getRandomNumber(4) + 1));
	}
	p.prevX = p.x;
	p.prevY = p.y;
}

void BradGameRoom::actorUpdate(uint sprite) {
	// 0x41DD60, a piece's update after its player.
	const int i = findPiece(sprite);
	if (i < 0 || _pieces[i].gone)
		return;
	Piece &p = _pieces[i];
	Room &room = *_engine.getRoom();
	const Common::Point mouse = _engine.getMousePos();

	// Taken: the button going down on it.
	if (!p.dragging && _pressed && _pressedOn == (int)sprite) {
		p.dragging = true;
		p.offX = (float)mouse.x - p.x;
		p.offY = (float)mouse.y - p.y;
	}

	// How far from where it hangs, which way, and how far off its way out.
	const float dx = p.x - p.ax;
	const float dy = p.y - p.ay;
	const double dist = sqrt((double)dy * dy + (double)dx * dx);
	const double angle = atan2((double)dy, (double)dx);
	double off = fabs(angle - (double)p.rest);
	if (kPi < off)
		off = kTwoPi - off;

	// Its turn, dragged past 60: loose, flying out the way it was pulled.
	if (p.dragging && !p.loose && 60.0 < dist && current() == i) {
		p.loose = true;
		p.rest = (float)angle;
		p.vx = (float)(cos((double)p.rest) * kPull);
		p.vy = (float)(sin((double)p.rest) * kPull);
		debugC(1, kDebugScript, "brad: piece %d loose at %g", i, dist);
	}
	const bool loose = p.loose;
	if (loose) {
		// Faster each tick.
		p.vx = (float)(cos((double)p.rest) * kPull + p.vx);
		p.vy = (float)(sin((double)p.rest) * kPull + p.vy);
	} else {
		// A spring back to where it hangs, stiffer the further it is off its
		// way out, and damped.
		const float k = ((float)(off * kInvPi) + 1.0f) * 1.5f;
		p.vx = (float)(((double)p.vx - (double)k * ((double)p.x - p.ax)) * 0.2f);
		p.vy = (float)(((double)p.vy - (double)k * ((double)p.y - p.ay)) * 0.2f);
	}
	if (!p.dragging || loose) {
		if (!loose) {
			const double speed = sqrt((double)p.vy * p.vy + (double)p.vx * p.vx);
			if (kPull < speed) {
				const double f = kPull / speed;
				p.vx = (float)(f * p.vx);
				p.vy = (float)(f * p.vy);
			}
		}
		p.x = p.x + p.vx;
		p.y = p.vy + p.y;
	} else {
		// Dragged: with the mouse, the spring only a hundredth of it.
		const float ny = (float)mouse.y - p.offY;
		p.x = (float)mouse.x - p.offX;
		p.y = ny;
		const double speed = sqrt((double)p.vy * p.vy + (double)p.vx * p.vx);
		if (kPull < speed) {
			const double f = kPull / speed;
			p.vx = (float)(f * p.vx);
			p.vy = (float)(f * p.vy);
		}
		p.x = (float)((double)p.vx * 0.01f + p.x);
		p.y = (float)((double)p.vy * 0.01f + ny);
		p.offX = (float)mouse.x - p.x;
		p.offY = (float)mouse.y - p.y;
	}

	// Loose and off the screen: off the pile, its links undone, and gone.
	if (loose && (960.0f < p.x || p.x < -320.0f || 720.0f < p.y || p.y < -240.0f)) {
		for (uint k = 0; k < p.links.size(); k++) {
			Common::Array<int> &theirs = _pieces[p.links[k]].links;
			for (uint m = 0; m < theirs.size(); m++) {
				if (theirs[m] == i) {
					theirs.remove_at(m);
					break;
				}
			}
		}
		p.links.clear();
		p.gone = true;
		sync(p);
		debugC(1, kDebugScript, "brad: piece %d off the screen", i);
		pieceGone(i);
		room.setPaused(sprite, true);
		return;
	}

	// Held to where it hangs: its turn, within a reach that shrinks the
	// further it is off its way out (78 on it, 60 at its share of the
	// order, 20 opposite); else within 20, and Brad complains each time it
	// goes past.
	const float ex = p.x - p.ax;
	const float ey = p.y - p.ay;
	const float reach = (float)sqrt((double)ey * ey + (double)ex * ex);
	const float way = (float)atan2((double)ey, (double)ex);
	if (current() != i) {
		if (reach <= 20.0f) {
			p.strained = false;
		} else {
			if (!p.strained)
				complain(i);
			const double f = 20.0f / (double)reach;
			p.strained = true;
			p.x = (float)(f * ex + p.ax);
			p.y = (float)(f * ey + p.ay);
		}
	} else if (!p.loose) {
		double a = fabs((double)way - p.rest);
		if (kPi < a)
			a = kTwoPi - a;
		a *= kInvPi;
		double limit;
		if (a < p.share)
			limit = a / p.share * -18.0f + 78.0f;
		else
			limit = (a - p.share) / (1.0f - p.share) * -40.0f + 60.0f;
		if (limit < reach) {
			const double f = limit / reach;
			p.x = (float)(f * ex + p.ax);
			p.y = (float)(f * ey + p.ay);
		}
	}
	sync(p);

	// Let go, or the mouse off it: no longer dragged.
	if (!_engine.isMouseDown() || !room.hitsPoint(sprite, mouse.x, mouse.y))
		p.dragging = false;

	// On the pile, it pushes its neighbours along the line to each, by how
	// far it is from where it hangs (the original reads the x distance for
	// both ways).
	if (!p.loose) {
		const float d = p.x - p.ax;
		for (uint k = 0; k < p.links.size(); k++) {
			Piece &q = _pieces[p.links[k]];
			const float lx = q.x - p.x;
			const float ly = q.y - p.y;
			const double len = sqrt((double)ly * ly + (double)lx * lx);
			const float ux = (float)(lx / len);
			const double uy = ly / len;
			const double f = (uy * d + (double)ux * d) * 0.05f;
			q.x = (float)(f * ux + q.x);
			q.y = (float)(f * uy + q.y);
			sync(q);
		}
	}
}

void BradGameRoom::tick() {
	// 0x41F3C0, after the base tick.
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	// The mouths follow the line playing (0x435A70): on its shape less one,
	// held where they are on a 0, back to the first with no line.
	if (_mouths >= 0) {
		uint shape;
		if (sound.getLipShape(_voice, shape)) {
			if (shape != 0)
				room.setPlayer(_mouths, 0, (int)shape - 1, 3);
		} else {
			room.setPlayer(_mouths, 0, 0, 3);
		}
	}
	// The introduction over: the cursor back.
	if (!_introDone && !sound.isClipPlaying(_voice)) {
		_introDone = true;
		debugC(1, kDebugScript, "brad: his introduction over");
	}
	// Excited over: the heads back to normal.
	if (--_excitedTimer == 0 && _heads >= 0)
		room.startAnim(_heads, "ANIM_BRADARMOR_HEADSNORM", 1, -1);
	// Won, or an hour gone: back to the arena (game vtable +0x04).
	const int timer = _exitTimer--;
	if (timer < 0) {
		_engine.requestRoom(kArena2);
		_exitTimer = kHour;
		debugC(1, kDebugScript, "brad: back to the arena");
	}
	for (uint k = _next; k < _count; k++)
		rattle(_order[k]);
	_pressed = false;
	_pressedOn = -1;
}

RoomClass *createBradRoom(ScoobyEngine &engine, const Common::String &room) {
	for (uint l = 0; l < ARRAYSIZE(kRooms); l++) {
		if (room == kRooms[l])
			return new BradGameRoom(engine);
	}
	return nullptr;
}

} // End of namespace Scooby
