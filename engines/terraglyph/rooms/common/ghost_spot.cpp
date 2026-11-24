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

// The ghost spot. The addresses are Showdown's; Phantom's class is the same
// code (0x420350, vtable 0x45EFD4), and only its game vtable +0x64 differs.

#include "common/debug.h"

#include "terraglyph/detection.h"
#include "terraglyph/rooms/common/ghost_spot.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

// 0x475F10: counts 0, 1, 0, ... once a tick in the ghost spot, and lasts
// across visits; the background moves on the ticks it is 0.
static int g_toggle = 0;

int GhostSpotRoom::getToggle() {
	return g_toggle;
}

void GhostSpotRoom::setToggle(int toggle) {
	g_toggle = toggle;
}

const char *GhostSpotRoom::getReturnRoom(const Common::String &gameId, const Common::String &current) {
	if (gameId == "showdown")
		return "ROOM_P01_Town_Center";
	static const char *const kCastle[] = {
		"ROOM_P29_Princess_Bedroom", "ROOM_P28_Veranda", "ROOM_P25_Wizards_Library", "ROOM_P24_Wizards_Bedroom", "ROOM_P26_Castle_Balcony"
	};
	for (uint i = 0; i < ARRAYSIZE(kCastle); i++) {
		if (current == kCastle[i])
			return "ROOM_P27_Below_Balcony";
	}
	return "ROOM_P04_Town";
}

void GhostSpotRoom::enter(const Common::String &from) {
	// 0x422420, after the base entry: the background (layer 0) at x -1280,
	// input held and the cursor faded (isBusy), the toolbar hidden
	// (0x407CC0, isToolbarHidden), the countdown at 396, and any music fade
	// stopped with the music at full (0x434F50).
	Room &room = *_engine.getRoom();
	const int layer = room.getLayerSprite(0);
	if (layer >= 0)
		room.setOffset(layer, kStart, 0);
	_countdown = kTicks;
	_skip = false;
	_engine.getSound()->cancelMusicFade();
	debugC(1, kDebugScript, "ghost spot: shown, %d ticks to %s", _countdown,
	       getReturnRoom(_engine.getGameId(), room.getName()));
}

void GhostSpotRoom::tick() {
	if (_countdown <= 0)
		return;
	// 0x422350, after the base tick: the toggle moves on every tick, and the
	// countdown goes down. Until it reaches 0, and with no skip key, the
	// background moves 6 pixels right on the ticks the toggle is 0, and back
	// 1280 once past 0. Then the game goes on to its town (game vtable +0x64,
	// then +0x04).
	if (++g_toggle > 1)
		g_toggle = 0;
	if (--_countdown != 0 && !_skip) {
		if (g_toggle != 0)
			return;
		Room &room = *_engine.getRoom();
		const int layer = room.getLayerSprite(0);
		if (layer >= 0) {
			int x = room.getOffsetX(layer) + kPan;
			if (x > 0)
				x -= kWrap;
			room.setOffset(layer, x, 0);
		}
		return;
	}
	const char *next = getReturnRoom(_engine.getGameId(), _engine.getRoom()->getName());
	debugC(1, kDebugScript, "ghost spot: %s with %d ticks left, to %s", _skip ? "skipped" : "done", _countdown, next);
	_countdown = 0;
	_skip = false;
	_engine.requestRoom(next);
}

bool GhostSpotRoom::skip() {
	// Input slot 5, seen in the next tick (untraced keys; Escape, Space or a
	// click here, as on the title). The original takes it once the slot has
	// been up since entry (0x475F00); a key press here always has been.
	if (_countdown <= 0)
		return false;
	_skip = true;
	return true;
}

void GhostSpotRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x422490: OBJ_TITLE_SCOOBY and OBJ_TITLE_DAPHNE sprites under the second
	// layer (game +0x5C; no room has one, so the root) at depths 1 and -1,
	// looping at 3 ticks a step, at the placement's x and y. The class's
	// flags +0xBC and +0xBD would leave Scooby, Daphne, Shaggy, Velma and Fred
	// unbuilt, but the constructor sets both and nothing clears them: Shaggy,
	// Velma and Fred go to the default path.
	if (object == "OBJ_TITLE_SCOOBY" || object == "OBJ_TITLE_DAPHNE") {
		claim.build = Room::Claim::kSprite;
		claim.layer = 1;
		claim.depth = object == "OBJ_TITLE_SCOOBY" ? 1 : -1;
		claim.hitType = 1;
	}
}

} // End of namespace Terraglyph
