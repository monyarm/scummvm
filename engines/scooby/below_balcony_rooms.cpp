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

// Phantom of the Knight's below-the-balcony and its door (WP-076), the last of
// the maze's group-7 rooms. The addresses are Phantom's.

#include "common/debug.h"

#include "scooby/below_balcony_rooms.h"
#include "scooby/character.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

static const char *const kBalcony = "ROOM_P27_Below_Balcony";
static const char *const kDoor = "ROOM_P27A_Below_Balcony_Door";

static const char *const kOverlay = "OBJ_P27_BELOW_BALCONY_OVERLAY";
static const char *const kToP26 = "OBJ_P27_TO_P26";
static const char *const kBallObject = "OBJ_P27_BELOW_BALCONY_BALL";
static const char *const kSharpShears = "OBJ_INV_SHARP_SHEARS";
static const char *const kBallBlue = "OBJ_INV_BALL_BLUE";
static const char *const kJesterObject = "OBJ_JESTER";
static const char *const kBottleFull = "OBJ_INV_BOTTLE_FULL";

// 0x4070C0 pair (the gallery's isMirrorPlaced idiom, as the maze interior uses
// it): the item is held but no longer in a toolbar slot, i.e. it has been used.
static bool used(ScoobyEngine &engine, const char *item) {
	if (!engine.getInventory()->hasItem(item))
		return false;
	for (uint i = 0; i < Inventory::kSlotCount; i++)
		if (engine.getInventory()->getSlot(i) == item)
			return false;
	return true;
}

// ---------------------------------------------------------------------------
// Below the balcony

const char *BelowBalconyRoom::kWrongTool = "Below_Balcony.260F_";
const char *BelowBalconyRoom::kBallLine = "Below_Balcony.100A_";
const char *BelowBalconyRoom::kBallSfx = "SFX_P27_BELOW_BALCONY_BALL";
const char *BelowBalconyRoom::kVineReveal = "Below_Balcony.260_";
const char *BelowBalconyRoom::kVineLine = "Below_Balcony.260D_";
const char *BelowBalconyRoom::kRustyCursor = "ANIM_CURSORRUSTY_SHEARS";

int BelowBalconyRoom::findSprite(const Common::String &object) const {
	const Room &room = *_engine.getRoom();
	for (uint s = 0; s < room.getSpriteCount(); s++)
		if (room.getObjectName(s) == object)
			return s;
	return -1;
}

void BelowBalconyRoom::startReveal(const Common::String &scene, bool *doneFlag) {
	// 0x40B400 on the room, holding input, with @p doneFlag as its done flag:
	// the 260_ line sets ShowBall as it ends, which fires the tick reveal.
	if (_engine.getScript()->startScene(scene, true) && doneFlag) {
		_scene = scene;
		_doneFlag = doneFlag;
	}
}

void BelowBalconyRoom::reset() {
	// 0x40F280, after the base reset: PlayBallScene is armed, the rest clear.
	_playBallScene = true;
	_showBall = false;
	_ballScene = false;
	_vineClicks = 0;
}

void BelowBalconyRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x40F380: the vines over the way to P26 are a requires object until the
	// sharp shears cut them, then they and the way's gate are gone; the way to
	// P26 is suppressed until the shears are used; the ball below the balcony is
	// a plain animated sprite the tick reveal drives.
	if (object == kOverlay) {
		// Built while the sharp shears are not yet used; gone once they are.
		claim.build = used(_engine, kSharpShears) ? Room::Claim::kSkip : Room::Claim::kRequires;
	} else if (object == kToP26) {
		// No way on until the vines are cut.
		if (!used(_engine, kSharpShears))
			claim.build = Room::Claim::kSkip;
	} else if (object == kBallObject) {
		claim.build = Room::Claim::kSprite;
	}
}

void BelowBalconyRoom::event(const Common::String &object, bool success) {
	// 0x40F2B0: the sharp shears used on the vines cut them (movie 5, the shears
	// spent); the rusty (unsharpened) shears tried on them warn (260F_, the
	// original polls this in tick by the held cursor); a plain click cycles 260D_
	// and, once armed, 260_, whose end queues the ball's reveal.
	if (object != kOverlay)
		return;
	if (success) {
		_engine.requestRoom(kBalcony, 5);
		_engine.getInventory()->removeItem(kSharpShears);
		debugC(1, kDebugScript, "below balcony: the sharp shears cut the vines");
		return;
	}
	// The rusty shears tried on the vines (0x40F5A0's tick branch, folded here
	// to the click the engine dispatches it on): a warning line, no cut.
	if (_engine.getInventory()->getCursor() == kRustyCursor) {
		_engine.getScript()->startScene(kWrongTool, true);
		return;
	}
	// A plain click: 260_ once armed (a prior click, the reveal still to come,
	// and the blue ball not yet seen at the door), else 260D_.
	_vineClicks++;
	if (_vineClicks != 1 && _playBallScene && !_engine.getGameFlag("BlueBallSeen"))
		startReveal(kVineReveal, &_showBall);
	else
		_engine.getScript()->startScene(kVineLine, true);
}

void BelowBalconyRoom::tick() {
	// 0x40F5A0, after the base tick: the ball's reveal. Queued by the 260_ line's
	// end (ShowBall), the ball animates into view with its sfx, input held; when
	// its animation plays out, a line (100A_) and the reveal is spent.
	RoomScript &script = *_engine.getScript();
	Room &room = *_engine.getRoom();

	// The 260_ reveal line's done flag, set as it ends.
	if (_doneFlag && script.getScene() != _scene) {
		*_doneFlag = true;
		_doneFlag = nullptr;
		_scene.clear();
	}

	if (_ball < 0)
		_ball = findSprite(kBallObject);

	if (_showBall) {
		_showBall = false;
		_ballScene = true; // holds input (game +0x09) through isBusy()
		if (_ball >= 0)
			room.setPlayer(_ball, 3, 3, -1);
		_engine.getSound()->play(kBallSfx);
		debugC(1, kDebugScript, "below balcony: the ball revealed");
	}
	if (_ballScene && _ball >= 0 && room.getMode(_ball) == 0) {
		_ballScene = false;
		_playBallScene = false;
		script.startScene(kBallLine, true);
		debugC(1, kDebugScript, "below balcony: the ball shown");
	}
}

void BelowBalconyRoom::saveKeys(Common::INIFile &profile) {
	// 0x40F6E0, after the base keys: the room's own three.
	profile.setKey("ShowBall", kBalcony, _showBall ? "Yes" : "No");
	profile.setKey("VineClicks", kBalcony, Common::String::format("%d", _vineClicks));
	profile.setKey("PlayBallScene", kBalcony, _playBallScene ? "Yes" : "No");
}

void BelowBalconyRoom::loadKeys(const Common::INIFile &profile) {
	// 0x40F760 -> 0x40C560/0x40C670: a missing key reads as its reset default.
	Common::String v;
	_showBall = profile.getKey("ShowBall", kBalcony, v) && v == "Yes";
	_vineClicks = profile.getKey("VineClicks", kBalcony, v) ? atoi(v.c_str()) : 0;
	_playBallScene = !profile.getKey("PlayBallScene", kBalcony, v) || v == "Yes";
}

// ---------------------------------------------------------------------------
// Below the balcony's door

const char *BelowBalconyDoorRoom::kGallery = "ROOM_P07_Royal_Gallery";

void BelowBalconyDoorRoom::reset() {
	// 0x410740, after the base reset.
	_returnPending = false;
}

void BelowBalconyDoorRoom::claimPlacement(const Common::String &object, Room::Claim &claim) {
	// 0x4108C0: the blue ball is there only once found; the Jester stands until
	// then (he holds the ball). Both gate on BlueBallFound.
	const bool found = _engine.getGameFlag("BlueBallFound");
	if (object == kBallBlue) {
		if (!found)
			claim.build = Room::Claim::kSkip;
	} else if (object == kJesterObject || object.hasPrefix(kJesterObject)) {
		if (found)
			claim.build = Room::Claim::kSkip;
	}
}

void BelowBalconyDoorRoom::event(const Common::String &object, bool success) {
	// 0x410760: the blue ball clicked plays its movie (2 the first time, 67
	// once the yellow ball is found), is marked found, and queues the return the
	// next entry resolves. The Jester takes it (his tick); it is not removed here.
	if (object != kBallBlue)
		return;
	const int movie = _engine.getGameFlag("YellowBallFound") ? kMovieBlueAgain : kMovieBlueFirst;
	_engine.requestRoom(kDoor, movie);
	_engine.setGameFlag("BlueBallFound", true);
	_returnPending = true;
	debugC(1, kDebugScript, "below balcony door: the blue ball found, movie %d", movie);
}

void BelowBalconyDoorRoom::enter(const Common::String &from) {
	// 0x4107F0, after the base entry: a queued return bounces back the way it
	// came (movie 54 from the royal gallery, 57 from elsewhere), unless the full
	// bottle has been used; otherwise the Jester takes the stage and the blue ball
	// is marked seen (BlueBallSeen), which ends the balcony's reveal line.
	if (_returnPending) {
		_returnPending = false;
		if (!used(_engine, kBottleFull)) {
			const int movie = from == kGallery ? kMovieReturnGallery : kMovieReturnOther;
			debugC(1, kDebugScript, "below balcony door: return to %s, movie %d", from.c_str(), movie);
			_engine.requestRoom(from, movie);
			return;
		}
	}
	_jester.enter(_engine);
	_engine.setGameFlag("BlueBallSeen", true);
	debugC(1, kDebugScript, "below balcony door: from %s", from.c_str());
}

void BelowBalconyDoorRoom::tick() {
	// 0x4107D0, after the base tick: the Jester.
	_jester.tick(_engine);
}

// ---------------------------------------------------------------------------

RoomClass *createBelowBalconyRoom(ScoobyEngine &engine, const Common::String &room) {
	if (room == kBalcony)
		return new BelowBalconyRoom(engine);
	if (room == kDoor)
		return new BelowBalconyDoorRoom(engine);
	return nullptr;
}

} // End of namespace Scooby
