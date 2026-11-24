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

#include "common/debug.h"

#include "terraglyph/character.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"
#include "terraglyph/rooms/showdown/tunnel.h"

namespace Terraglyph {

static const char *const kDark = "ROOM_P14_Tunnel_Dark";
static const char *const kLit = "ROOM_P18_Tunnel_Lit";
static const char *const kCrate = "OBJ_P14_TUNNEL_DARK_CRATE";

// ---------------------------------------------------------------------------
// The dark tunnel

void TunnelDarkRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x419950: the crate, a requires object (0x408C80 with -1: layer 0).
	if (object == kCrate) {
		claim.build = Room::Claim::kRequires;
		claim.layer = 0;
	}
}

void TunnelDarkRoom::enter(const Common::String &from) {
	// 0x4199C0, after the base entry: the way out's line, the tries and the
	// crate's flags start again.
	Room &room = *_engine.getRoom();
	_crate = -1;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		if (room.getObjectName(s) == kCrate)
			_crate = s;
	}
	_exitLine = 0;
	_pushes = 0;
	_crateClicked = false;
	_pushing = false;
	_lastScene.clear();
	_lastSound.clear();
}

void TunnelDarkRoom::event(const Common::String &object, bool success) {
	// 0x419920: the crate with the arrow (its requiredanim).
	if (object == kCrate && success) {
		_crateClicked = true;
		debugC(1, kDebugScript, "dark tunnel: the crate clicked at tick %u", _engine.getScript()->getTick());
	}
}

bool TunnelDarkRoom::leaveBy(const Common::String &exitObject) {
	// 0x419530 (+0x24): in the dark, a way out only says a line,
	// Tunnel_Dark.170A2_ and 170A3_ in turn, without holding input; the
	// gang stays.
	const char *scene = _exitLine == 0 ? "Tunnel_Dark.170A2_" : "Tunnel_Dark.170A3_";
	_exitLine = _exitLine == 0 ? 1 : 0;
	if (_engine.getScript()->startScene(scene, false))
		_lastScene = scene;
	return true;
}

void TunnelDarkRoom::tick() {
	// 0x419610, after the base tick, while the crate is clicked; input held.
	if (!_crateClicked)
		return;
	Room &room = *_engine.getRoom();
	RoomScript &script = *_engine.getScript();
	Sound &sound = *_engine.getSound();
	Character *shaggy = script.getCharacter("SHAGGY");
	Character *scooby = script.getCharacter("SCOOBY");
	// With Shaggy and Scooby idle and nothing playing: the first try this
	// visit, Shaggy alone can't; after that, both push, the crate and
	// Scooby hidden.
	if (!_pushing && shaggy && scooby && shaggy->getState() == Character::kStateIdle && scooby->getState() == Character::kStateIdle &&
	    !script.isScenePlaying() && script.isOpeningDone()) {
		if (_pushes++ == 0) {
			shaggy->special(room, sound, "OBJ_SHAGGY_P14_NO_PUSH_CRATE");
			_lastSound = "Shaggy.Cant.Push.Crate";
		} else {
			if (_crate >= 0)
				room.setHidden(_crate, true);
			if (scooby->getSprite() >= 0)
				room.setHidden(scooby->getSprite(), true);
			shaggy->special(room, sound, "OBJ_SHAGGY_P14_PUSH_CRATE");
			_lastSound = "Shaggy.Scooby.Push.Crate";
		}
		sound.play(_lastSound);
		_pushing = true;
		debugC(1, kDebugScript, "dark tunnel: try %d at tick %u, %s", _pushes, script.getTick(), _lastSound.c_str());
	}
	// Shaggy done: alone, input comes back. Pushed, the crate rolls away,
	// Shaggy stays at the end of his push (a sprite in layer 0 at depth 500
	// on his last step, where he stood), and the gang is in the post office.
	if (_pushing && shaggy && shaggy->getState() == Character::kStateIdle) {
		if (_pushes != 1) {
			if (_crate >= 0) {
				room.startAnim(_crate, "ANIM_P14_TUNNEL_DARK_CRATEB", 1, -1);
				room.setHidden(_crate, false);
			}
			const int s = shaggy->getSprite();
			if (s >= 0) {
				room.setHidden(s, true);
				const int layer = room.getLayerSprite(0);
				const uint push = room.addEngineSprite(layer, "OBJ_SHAGGY_P14_PUSH_CRATE", "ANIM_SHAGGY_P14_PUSH_CRATE1", 500,
				                                       room.getX(s), room.getY(s), 1, "Shaggy pushed the crate");
				room.setPlayer(push, 5, -1);
			}
			debugC(1, kDebugScript, "dark tunnel: pushed at tick %u; to the post office", script.getTick());
			_engine.requestRoom("ROOM_P08_Post_Office");
		}
		_pushing = false;
		_crateClicked = false;
	}
}

// ---------------------------------------------------------------------------
// The lit tunnel

void TunnelLitRoom::leaving(const Common::String &destination, Common::String &scene) {
	// 0x419A50 (+0x24): the first way out keeps its scene; after that, none.
	if (!_commentPlayed)
		_commentPlayed = true;
	else
		scene.clear();
}

void TunnelLitRoom::saveKeys(Common::INIFile &profile) {
	// 0x419BD0: the base keys, then BootHillCommentPlayed, in the room's section.
	profile.setKey("BootHillCommentPlayed", kLit, _commentPlayed ? "Yes" : "No");
}

void TunnelLitRoom::loadKeys(const Common::INIFile &profile) {
	// 0x419C10 -> 0x40C560: a missing key reads as false.
	Common::String v;
	_commentPlayed = profile.getKey("BootHillCommentPlayed", kLit, v) && v == "Yes";
}

RoomClass *createTunnelRoom(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's factory (0x42E0E0).
	if (room == kDark)
		return new TunnelDarkRoom(engine);
	if (room == kLit)
		return new TunnelLitRoom(engine);
	return nullptr;
}

} // End of namespace Terraglyph
