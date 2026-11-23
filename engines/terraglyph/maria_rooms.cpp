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

// Phantom of the Knight's Maria game. The addresses are Phantom's.

#include "common/debug.h"
#include "common/scummsys.h"

#include "terraglyph/detection.h"
#include "terraglyph/maria_rooms.h"
#include "terraglyph/room.h"
#include "terraglyph/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static const char *const kMariaRoom = "ROOM_Maria";
static const char *const kMaria = "OBJ_MARIAGAME_MARIA";

// The pans by Maria's step (0x428770): the first pan's x and its bottom
// item's y, then the second's.
static const float kPans[5][4] = {
	{ 180, 175, 383, 158 }, { 181, 170, 382, 162 }, { 181, 167, 380, 164 }, { 183, 166, 376, 167 }, { 186, 161, 375, 170 }
};

static const char *const kColours[] = { "G", "R", "Y" };

void MariaGameRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x428F60: Maria, a sprite under the first layer at depth -10, stopped
	// on step 2 (enter()). The way out the default path builds.
	if (object == kMaria) {
		claim.build = Room::Claim::kSprite;
		claim.layer = 0;
		claim.depth = -10;
		claim.hitType = 1;
	}
}

void MariaGameRoom::playSound(const Common::String &name) {
	// 0x4359E0 then 0x4359A0: stopped, then played.
	Sound &sound = *_engine.getSound();
	sound.stopClip(name);
	sound.play(name);
}

void MariaGameRoom::syncSprite(const Item &it) {
	_engine.getRoom()->setOffset(it.sprite, (int)it.x, (int)it.y);
}

int MariaGameRoom::makeItem(float floorY, int colour, int kind, float x) {
	// 0x4279D0: OBJ_MARIAGAME_<colour><kind> under the first layer playing
	// its IDLE in mode 1, at (x, its floor), at depth floor + 50 - 443. B
	// weighs 2, C 1 (both cups), P 4 (a plate).
	static const char *const kKinds[] = { "B", "C", "P" };
	static const int kWeights[] = { 2, 1, 4 };
	Room &room = *_engine.getRoom();
	Item it;
	it.name = Common::String::format("MARIAGAME_%s%s", kColours[colour], kKinds[kind]);
	it.sound = kind == 2 ? "Plate" : "Cup";
	it.weight = kWeights[kind];
	it.floor = floorY;
	it.x = x;
	it.y = floorY;
	it.sprite = room.addEngineSprite(room.getLayerSprite(0), "OBJ_" + it.name, "ANIM_" + it.name + "IDLE", (int)(floorY + 50.0f - 443.0f),
	                                 (int)it.x, (int)it.y, 1, it.name);
	_items.push_back(it);
	return _items.size() - 1;
}

void MariaGameRoom::generate() {
	// 0x428890: groups that balance, until there are exactly level * 2 + 6
	// items: a floor (rand() % 27 + 443) for the group, then its first kind
	// (rand() % 3, drawn again while 1). A cup B with two C; a plate P with
	// either two B (rand() & 1 odd), a B and two C, or four C. Each item's
	// colour is rand() % 3, and its x rand() % 600 + 20 (0x4279D0).
	struct Spec {
		float floor;
		int colour, kind;
		float x;
	};
	Common::Array<Spec> specs;
	for (;;) {
		specs.clear();
		while ((int)specs.size() < _total) {
			const float floorY = (float)(_engine.getRandomNumber(26) + 443);
			int first;
			do
				first = _engine.getRandomNumber(2);
			while (first == 1);
			auto add = [&](int kind) {
				const int colour = _engine.getRandomNumber(2);
				const float x = (float)_engine.getRandomNumber(599) + 20.0f;
				specs.push_back({ floorY, colour, kind, x });
			};
			add(first);
			if (first == 0) {
				add(1);
				add(1);
			} else if (_engine.getRandomNumber(1) == 0) {
				if (_engine.getRandomNumber(1) == 0) {
					add(1);
					add(1);
					add(1);
					add(1);
				} else {
					add(0);
					add(1);
					add(1);
				}
			} else {
				add(0);
				add(0);
			}
		}
		if ((int)specs.size() == _total)
			break;
		debugC(1, kDebugScript, "maria: generated %u items, retrying", specs.size());
	}
	for (uint i = 0; i < specs.size(); i++) {
		const int k = makeItem(specs[i].floor, specs[i].colour, specs[i].kind, specs[i].x);
		debugC(1, kDebugScript, "maria: item %d %s weight %d at (%g, %g)", k, _items[k].name.c_str(), _items[k].weight, _items[k].x, _items[k].y);
	}
}

void MariaGameRoom::clearItemsForCheck() {
	for (uint i = 0; i < _items.size(); i++)
		_engine.getRoom()->setHidden(_items[i].sprite, true);
	_items.clear();
	_left.clear();
	_right.clear();
	_pendItem.clear();
	_pendDepth.clear();
	_spinCount = 0;
}

int MariaGameRoom::addItemForCheck(float floorY, int colour, int kind, float x) {
	return makeItem(floorY, colour, kind, x);
}

void MariaGameRoom::enter(const Common::String &from) {
	// 0x428890, after the base entry: the toolbar hidden (0x407CC0, the
	// fork's toolbar list), level * 2 + 6 items generated, both pans empty,
	// Maria to be balanced, and an hour (129600 ticks) before the game gives
	// up and goes back to the dining room.
	Room &room = *_engine.getRoom();
	_items.clear();
	_left.clear();
	_right.clear();
	_pendItem.clear();
	_pendDepth.clear();
	_spinCount = 0;
	_won = false;
	_click = false;
	_clicked = -1;
	_maria = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == kMaria)
			_maria = s;
	}
	if (_maria >= 0)
		room.startAnim(_maria, "ANIM_MARIAGAME_MARIAHAPPY", 0, 2);
	_total = (int)_engine.getDifficulty() * 2 + 6;
	generate();
	_excitedTimer = 0;
	_rebalance = true;
	_exitTimer = 129600;
	debugC(1, kDebugScript, "maria: from %s, %d items", from.c_str(), _total);
}

bool MariaGameRoom::hidesCursor() const {
	// 0x435E80 while an item is carried.
	for (uint i = 0; i < _items.size(); i++) {
		if (_items[i].dragging)
			return true;
	}
	return false;
}

bool MariaGameRoom::click(int sprite) {
	// The items read the button coming up (game +0x20 bit 4) in their
	// updates; the fork's click is the button going down. A click on an
	// item, or anywhere while one is carried, is the game's.
	_click = true;
	_clicked = sprite;
	if (hidesCursor())
		return true;
	for (uint i = 0; i < _items.size(); i++) {
		if (_items[i].sprite == sprite)
			return true;
	}
	return false;
}

void MariaGameRoom::pendDepth(int i, int depth) {
	// 0x4296D0: the depth set at the start of the next tick.
	for (uint k = 0; k < _pendItem.size(); k++) {
		if (_pendItem[k] == i) {
			_pendDepth[k] = depth;
			return;
		}
	}
	_pendItem.push_back(i);
	_pendDepth.push_back(depth);
}

int MariaGameRoom::panAt(float x, float y, float &tx, float &ty) const {
	// 0x429040, at Maria's step: within 80 of a pan's x and above its top
	// item (less than 20 below where the next one goes).
	const int s = _maria >= 0 ? (int)_engine.getRoom()->getStep(_maria) : 2;
	if (s < 0 || s > 4)
		return 0;
	const float leftTop = kPans[s][1] - (float)_left.size() * 15.0f;
	if (kPans[s][0] < x && x < kPans[s][0] + 80.0f && y < leftTop + 20.0f) {
		tx = kPans[s][0] + 40.0f;
		ty = leftTop;
		return 1;
	}
	const float rightTop = kPans[s][3] - (float)_right.size() * 15.0f;
	if (kPans[s][2] < x && x < kPans[s][2] + 80.0f && y < rightTop + 20.0f) {
		tx = kPans[s][2] + 40.0f;
		ty = rightTop;
		return 2;
	}
	return 0;
}

void MariaGameRoom::place(int i, int pan) {
	// 0x429180: on top of the pan, at the depth of its place in it, and
	// Maria balanced again.
	if (pan == 1 || pan == 2) {
		Common::Array<int> &list = pan == 1 ? _left : _right;
		list.push_back(i);
		pendDepth(i, list.size());
	}
	_rebalance = true;
}

bool MariaGameRoom::takeFromPan(int i) {
	// 0x4292D0: only a pan's top item can be taken, back at depth 50, and
	// Maria balanced again; one in no pan always can.
	for (int pan = 1; pan <= 2; pan++) {
		Common::Array<int> &list = pan == 1 ? _left : _right;
		for (uint k = 0; k < list.size(); k++) {
			if (list[k] != i)
				continue;
			if (k != list.size() - 1)
				return false;
			list.pop_back();
			_rebalance = true;
			pendDepth(i, 50);
			return true;
		}
	}
	return true;
}

bool MariaGameRoom::isPickable(int i) const {
	// 0x429270: not under another in the first pan, nor in the second.
	uint k = 0;
	while (k < _left.size() && _left[k] != i)
		k++;
	if ((int)k < (int)_left.size() - 1)
		return false;
	k = 0;
	while (k < _right.size() && _right[k] != i)
		k++;
	return (int)_right.size() - 1 <= (int)k;
}

void MariaGameRoom::knockOff(int i) {
	// 0x4285B0: back at its floor's depth, thrown off at (rand() % 10 - 5,
	// -(rand() % 10)), spinning backwards for a leftward throw.
	Item &it = _items[i];
	pendDepth(i, (int)(it.floor + 50.0f - 443.0f));
	it.vx = (float)(int)_engine.getRandomNumber(9) - 5.0f;
	it.vy = -(float)(int)_engine.getRandomNumber(9);
	it.pan = 0;
	it.resting = false;
	_engine.getRoom()->startAnim(it.sprite, "ANIM_" + it.name + "SPIN", it.vx <= 0.0f ? 2 : 1, -1);
}

void MariaGameRoom::rebalance() {
	// 0x429400: Maria's step goes one toward the weight difference (the
	// second pan less the first): 2 level, 3 for 1, 4 for 2-4, 1 for -1, 0
	// for -2 to -4, past them 5 or -1. There, every item falls off. Else, at
	// a new step, she is EXCITED on it for 36 ticks and the pans' items move
	// with it, 15 apart.
	Room &room = *_engine.getRoom();
	int first = 0, second = 0;
	for (uint k = 0; k < _right.size(); k++)
		second += _items[_right[k]].weight;
	for (uint k = 0; k < _left.size(); k++)
		first += _items[_left[k]].weight;
	const int diff = second - first;
	int target;
	switch (diff) {
	case 0:
		target = 2;
		break;
	case 1:
		target = 3;
		break;
	case 2:
	case 3:
	case 4:
		target = 4;
		break;
	case -1:
		target = 1;
		break;
	case -2:
	case -3:
	case -4:
		target = 0;
		break;
	default:
		target = diff >= 0 ? 5 : -1;
		break;
	}
	const int current = _maria >= 0 ? (int)room.getStep(_maria) : 2;
	int step;
	if (target < current) {
		step = current - 1;
	} else if (current < target) {
		step = current + 1;
	} else {
		step = target;
		_rebalance = false;
	}
	if (step != -1 && step != 5) {
		if (current != step) {
			if (_maria >= 0)
				room.startAnim(_maria, "ANIM_MARIAGAME_MARIAEXCITED", 0, step);
			_excitedTimer = 36;
			float y = kPans[step][1];
			for (uint k = 0; k < _left.size(); k++) {
				Item &it = _items[_left[k]];
				it.x = kPans[step][0] + 40.0f;
				it.y = y;
				syncSprite(it);
				y -= 15.0f;
			}
			y = kPans[step][3];
			for (uint k = 0; k < _right.size(); k++) {
				Item &it = _items[_right[k]];
				it.x = kPans[step][2] + 40.0f;
				it.y = y;
				syncSprite(it);
				y -= 15.0f;
			}
			debugC(1, kDebugScript, "maria: tilts to step %d (weights %d and %d)", step, first, second);
		}
		return;
	}
	debugC(1, kDebugScript, "maria: tipped over (weights %d and %d); everything falls off", first, second);
	for (uint k = 0; k < _left.size(); k++)
		knockOff(_left[k]);
	_left.clear();
	for (uint k = 0; k < _right.size(); k++)
		knockOff(_right[k]);
	_right.clear();
}

void MariaGameRoom::updateItem(int i, bool click, int clicked) {
	// 0x427CD0, the item's update after its player.
	Room &room = *_engine.getRoom();
	Item &it = _items[i];
	const int s = it.sprite;
	const Common::Point mouse = _engine.getMousePos();
	if (it.resting) {
		if (room.getMode(s) == 0 && room.getStep(s) == 0)
			room.setPlayer(s, 5, -1, 3);
		if ((int)room.getStep(s) == it.stopStep) {
			room.setPlayer(s, 0, -1, 3);
			it.stopStep = -1;
		}
		bool settled = true;
		if (!it.dragging) {
			// Taken: carried from where it was clicked, its sequence played
			// on once.
			if (click && clicked == s && takeFromPan(i)) {
				it.dragging = true;
				it.offX = (float)mouse.x - it.x;
				it.offY = (float)mouse.y - it.y;
				it.prevX = (float)mouse.x - it.offX;
				it.prevY = (float)mouse.y - it.offY;
				if (room.getStep(s) != 7)
					room.setPlayer(s, 3, room.getStep(s), 3);
				settled = room.getStep(s) == 7;
				debugC(1, kDebugScript, "maria: %s taken", it.name.c_str());
			}
		} else {
			// Carried, kept on the screen; let go above its floor it is
			// thrown, or falls into the pan under it.
			it.prevX = it.x;
			it.prevY = it.y;
			float nx = (float)mouse.x - it.offX;
			float ny = (float)mouse.y - it.offY;
			if (nx < 630.0f) {
				if (nx <= 10.0f)
					nx = 10.0f;
			} else {
				nx = 630.0f;
			}
			if (ny < 470.0f) {
				if (ny <= 10.0f)
					ny = 10.0f;
			} else {
				ny = 470.0f;
			}
			it.y = ny;
			it.x = nx;
			syncSprite(it);
			if (click) {
				it.dragging = false;
				if (it.y < it.floor) {
					it.resting = false;
					it.pan = panAt(it.x, it.y, it.panX, it.panY);
					if (it.pan) {
						it.vy = 0.0f;
					} else {
						room.startAnim(s, "ANIM_" + it.name + "SPIN", it.offX <= 0.0f ? 2 : 1, -1);
						const float dx = (it.x - it.prevX) * 0.2f;
						const float dy = (it.y - it.prevY) * 0.2f;
						it.vx = dx;
						it.vy = dy;
						const float speed = sqrtf(dx * dx + dy * dy);
						if (10.0f < speed) {
							it.vx = 10.0f / speed * dx;
							it.vy = 10.0f / speed * dy;
						}
						if (3.0f < speed)
							playSound("Maria.Throw." + it.sound);
						settled = false;
					}
					debugC(1, kDebugScript, "maria: %s let go at (%g, %g), pan %d, speed (%g, %g)", it.name.c_str(), it.x, it.y, it.pan, it.vx, it.vy);
				}
			}
		}
		if (!settled)
			it.stopStep = -1;
		// Spinning before: its spin over (Maria.Spin stops with the last),
		// and Maria.Floor.<Cup|Plate>.<rand() & 1 + 1>.
		if (it.spinning) {
			if (--_spinCount == 0)
				_engine.getSound()->stopClip("Maria.Spin");
			it.spinning = false;
			playSound(Common::String::format("Maria.Floor.%s.%d", it.sound.c_str(), _engine.getRandomNumber(1) + 1));
		}
		return;
	}

	// In the air: steered to its pan, or spinning (Maria.Spin, once for all).
	if (it.pan) {
		if (0.6f < it.panY - it.y)
			it.vx = (it.panX - it.x) / (it.panY - it.y);
	} else if (!it.spinning && _engine.getSound()->findClip("Maria.Spin")) {
		it.spinning = true;
		if (_spinCount++ == 0)
			playSound("Maria.Spin");
	}
	it.vy += 0.5f;
	const float ox = it.x;
	it.x = it.vx + ox;
	it.y = it.vy + it.y;
	if (639.0f < it.vx + ox) {
		it.x = 639.0f;
		it.vx = -(it.vx * 0.6f);
		playSound("Maria.Bounce");
	}
	if (it.x < 0.0f) {
		it.x = 0.0f;
		it.vx = -(it.vx * 0.6f);
		playSound("Maria.Bounce");
	}
	if (!it.pan) {
		const float top = it.floor + 2.0f;
		if (top < it.y) {
			it.y = it.floor;
			it.vy = it.vy * -0.6f;
		}
		if (it.floor - 2.0f < it.y && it.y < top) {
			// Landed: its sequence settles on step 3, or on its last.
			const int step = room.getStep(s);
			it.vy = 0.0f;
			it.resting = true;
			if (step < 2) {
				room.setPlayer(s, 4, step, 3);
			} else if (step < 6) {
				if (step < 3)
					room.setPlayer(s, 3, step, 3);
				else if (step < 4)
					room.setPlayer(s, 0, -1, 3);
				else
					room.setPlayer(s, 4, step, 3);
				it.stopStep = 3;
			} else {
				room.setPlayer(s, 3, step, 3);
			}
		}
	} else if (it.panY + 2.0f < it.y) {
		it.vy = 0.0f;
		it.resting = true;
		place(i, it.pan);
		playSound("Maria.Place." + it.sound);
	}
	syncSprite(it);
}

void MariaGameRoom::tick() {
	// 0x428CE0, after the base tick; the items' updates (0x427CD0) come
	// first, as the sprite tree runs them.
	Room &room = *_engine.getRoom();
	const bool click = _click;
	const int clicked = _clicked;
	_click = false;
	_clicked = -1;
	for (uint i = 0; i < _items.size(); i++) {
		updateItem(i, click, clicked);
		room.setRollover(_items[i].sprite, isPickable(i) ? "ANIM_CURSOROPEN" : "");
	}
	// The depths set since the last tick.
	for (uint k = 0; k < _pendItem.size(); k++)
		room.setDepth(_items[_pendItem[k]].sprite, _pendDepth[k]);
	_pendItem.clear();
	_pendDepth.clear();
	if (!_rebalance) {
		// Every item in the pans and the pans equal: MariaPuzzle, input held
		// and the cursor hidden, and back in 72 ticks.
		if ((int)(_left.size() + _right.size()) == _total) {
			int first = 0, second = 0;
			for (uint k = 0; k < _right.size(); k++)
				second += _items[_right[k]].weight;
			for (uint k = 0; k < _left.size(); k++)
				first += _items[_left[k]].weight;
			if (first == second && !_engine.getGameFlag("MariaPuzzle")) {
				_engine.setGameFlag("MariaPuzzle", true);
				_exitTimer = 72;
				_won = true;
				debugC(1, kDebugScript, "maria: balanced, her game won");
			}
		}
	} else {
		rebalance();
	}
	const int exitTimer = _exitTimer--;
	if (exitTimer < 0) {
		RoomScript::Exit exit;
		exit.destination = "ROOM_P12_Dining_Room";
		exit.movie = -1;
		_engine.getScript()->takeExit(exit);
		_exitTimer = 129600;
	}
	// EXCITED over: HAPPY on the same step.
	const int excited = _excitedTimer--;
	if (excited == 0 && _maria >= 0)
		room.startAnim(_maria, "ANIM_MARIAGAME_MARIAHAPPY", 0, room.getStep(_maria));
}

RoomClass *createMariaRoom(TerraglyphEngine &engine, const Common::String &room) {
	if (room == kMariaRoom)
		return new MariaGameRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
