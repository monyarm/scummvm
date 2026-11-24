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

// The script-level check of Phantom's below-the-balcony and its door (WP-076)
// against an independent reference written from the trace. Showdown has neither
// room.

#include "common/debug.h"

#include "terraglyph/formats/art_archive.h"
#include "terraglyph/rooms/phantomknight/below_balcony.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_class.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

static const char *buildName(Room::Claim::Build b) {
	switch (b) {
	case Room::Claim::kDefault: return "default";
	case Room::Claim::kSkip: return "skip";
	case Room::Claim::kRequires: return "requires";
	case Room::Claim::kClickable: return "clickable";
	case Room::Claim::kSprite: return "sprite";
	case Room::Claim::kInventory: return "inventory";
	case Room::Claim::kExit: return "exit";
	}
	return "?";
}

void TerraglyphEngine::checkBelowBalcony() {
	const char *const kBalcony = "ROOM_P27_Below_Balcony";
	const char *const kDoor = "ROOM_P27A_Below_Balcony_Door";

	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kBalcony;
	if (!found) {
		debugC(3, kDebugScript, "below balcony check: no below balcony");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	// sfx ships under its exact name; the numbered scene lines ship with a
	// speaker suffix, so they match by prefix (reading exactly would say
	// "found 0" of a name that is right — the WP-074 trap).
	auto has = [&](const char *name) { return _sound->findClip(name) != nullptr ? 1 : 0; };
	auto pfx = [&](const char *name) { return _sound->hasClipPrefix(name) ? 1 : 0; };

	auto claimOf = [&](RoomClass *rc, const char *object) {
		Room::Claim claim;
		rc->claimPlacement(object, claim);
		return claim;
	};

	// The classes the factory builds.
	{
		RoomClass *a = createRoomClass(*this, kBalcony);
		RoomClass *b = createRoomClass(*this, kDoor);
		debugC(3, kDebugScript, "below balcony check classes: Below_Balcony %s, Door %s",
		       dynamic_cast<BelowBalconyRoom *>(a) ? "balcony" : a ? "other" : "-",
		       dynamic_cast<BelowBalconyDoorRoom *>(b) ? "door" : b ? "other" : "-");
		delete a;
		delete b;
	}

	// -----------------------------------------------------------------------
	// Below the balcony.
	{
		RoomClass *rc = createRoomClass(*this, kBalcony);
		debugC(3, kDebugScript, "below balcony check Below_Balcony clips: vine %s %d; reveal %s %d; wrong-tool %s %d; ball-line %s %d; ball-sfx %s %d",
		       BelowBalconyRoom::kVineLine, pfx(BelowBalconyRoom::kVineLine), BelowBalconyRoom::kVineReveal, pfx(BelowBalconyRoom::kVineReveal),
		       BelowBalconyRoom::kWrongTool, pfx(BelowBalconyRoom::kWrongTool), BelowBalconyRoom::kBallLine, pfx(BelowBalconyRoom::kBallLine),
		       BelowBalconyRoom::kBallSfx, has(BelowBalconyRoom::kBallSfx));

		newGameForCheck();
		const Room::Claim overlayFresh = claimOf(rc, "OBJ_P27_BELOW_BALCONY_OVERLAY");
		const Room::Claim p26Fresh = claimOf(rc, "OBJ_P27_TO_P26");
		const Room::Claim ball = claimOf(rc, "OBJ_P27_BELOW_BALCONY_BALL");
		_inventory->addItem("OBJ_INV_SHARP_SHEARS", "ANIM_TOOLBARSHARP_SHEARS", "ANIM_CURSORSHARP_SHEARS");
		_inventory->removeItem("OBJ_INV_SHARP_SHEARS"); // used: in the history, out of the slots
		const Room::Claim overlayCut = claimOf(rc, "OBJ_P27_BELOW_BALCONY_OVERLAY");
		const Room::Claim p26Cut = claimOf(rc, "OBJ_P27_TO_P26");
		debugC(3, kDebugScript, "below balcony check Below_Balcony claims: OVERLAY fresh %s shears-used %s; TO_P26 fresh %s shears-used %s; BALL %s",
		       buildName(overlayFresh.build), buildName(overlayCut.build), buildName(p26Fresh.build), buildName(p26Cut.build), buildName(ball.build));

		// The room's own save keys round-trip, and their reset defaults.
		BelowBalconyRoom *bb = dynamic_cast<BelowBalconyRoom *>(rc);
		Common::INIFile saved;
		saved.setKey("ShowBall", kBalcony, "Yes");
		saved.setKey("VineClicks", kBalcony, "3");
		saved.setKey("PlayBallScene", kBalcony, "No");
		bb->loadKeys(saved);
		const bool showRead = bb->showBallPending();
		const int vineRead = bb->getVineClicks();
		const bool playRead = bb->playBallScene();
		bb->reset();
		Common::INIFile none;
		bb->loadKeys(none); // missing keys read as the reset defaults
		debugC(3, kDebugScript, "below balcony check Below_Balcony state: ShowBall read %d reset %d; VineClicks read %d reset %d; PlayBallScene read %d reset %d",
		       showRead ? 1 : 0, bb->showBallPending() ? 1 : 0, vineRead, bb->getVineClicks(), playRead ? 1 : 0, bb->playBallScene() ? 1 : 0);
		delete rc;
	}

	// -----------------------------------------------------------------------
	// Below the balcony's door.
	{
		RoomClass *rc = createRoomClass(*this, kDoor);

		newGameForCheck();
		const Room::Claim blueHidden = claimOf(rc, "OBJ_INV_BALL_BLUE");
		const Room::Claim jesterPresent = claimOf(rc, "OBJ_JESTER");
		setGameFlag("BlueBallFound", true);
		const Room::Claim blueFound = claimOf(rc, "OBJ_INV_BALL_BLUE");
		const Room::Claim jesterGone = claimOf(rc, "OBJ_JESTER");
		debugC(3, kDebugScript, "below balcony check Door claims: BALL_BLUE hidden %s found %s; JESTER present %s found %s",
		       buildName(blueHidden.build), buildName(blueFound.build), buildName(jesterPresent.build), buildName(jesterGone.build));
		debugC(3, kDebugScript, "below balcony check Door movies: blue first %d again %d; return gallery %d other %d; exit %s",
		       (int)BelowBalconyDoorRoom::kMovieBlueFirst, (int)BelowBalconyDoorRoom::kMovieBlueAgain,
		       (int)BelowBalconyDoorRoom::kMovieReturnGallery, (int)BelowBalconyDoorRoom::kMovieReturnOther, BelowBalconyDoorRoom::kGallery);
		delete rc;
	}

	_checking = false;
}

} // End of namespace Terraglyph
