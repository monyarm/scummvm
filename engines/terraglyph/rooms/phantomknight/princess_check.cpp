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

// The script-level check of Phantom's Princess's bedroom and Mermaid's cavern
// (WP-078) against an independent reference written from the trace. Showdown has
// neither room.

#include "common/debug.h"

#include "terraglyph/formats/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/graphics/inventory.h"
#include "terraglyph/rooms/phantomknight/princess.h"
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

void TerraglyphEngine::checkPrincess() {
	const char *const kBedroom = "ROOM_P29_Princess_Bedroom";
	const char *const kCavern = "ROOM_P21_Mermaids_Cavern";

	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kBedroom;
	if (!found) {
		debugC(3, kDebugScript, "princess check: no princess bedroom");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	// The named scene lines ship with a speaker suffix, so they match by prefix
	// (reading exactly would say "found 0" of a name that is right — the WP-074
	// trap). The spell book's toolbar/cursor ids are animations, not clips, so
	// the live draw verifies them, not the sound index.
	auto pfx = [&](const char *name) { return _sound->hasClipPrefix(name) ? 1 : 0; };

	auto claimOf = [&](RoomClass *rc, const char *object) {
		Room::Claim claim;
		rc->claimPlacement(object, claim);
		return claim;
	};

	// The classes the factory builds, and which room echoes.
	{
		RoomClass *a = createRoomClass(*this, kBedroom);
		RoomClass *b = createRoomClass(*this, kCavern);
		debugC(3, kDebugScript, "princess check classes: Princess_Bedroom %s echo %d; Mermaids_Cavern %s echo %d",
		       dynamic_cast<PrincessBedroomRoom *>(a) ? "bedroom" : a ? "other" : "-", a && a->isEcho() ? 1 : 0,
		       dynamic_cast<MermaidsCavernRoom *>(b) ? "cavern" : b ? "other" : "-", b && b->isEcho() ? 1 : 0);
		delete a;
		delete b;
	}

	// -----------------------------------------------------------------------
	// The Princess's bedroom.
	{
		RoomClass *rc = createRoomClass(*this, kBedroom);
		debugC(3, kDebugScript, "princess check Princess_Bedroom clips: opening %s %d; hotspot %s %d; book-got %s %d; take %s %d (toolbar/cursor anims ANIM_TOOLBARSPELL_BOOK / ANIM_CURSORSPELL_BOOK drawn, not clips)",
		       "Princess_Bedroom.280_", pfx("Princess_Bedroom.280_"),
		       PrincessBedroomRoom::kHotspotLine, pfx(PrincessBedroomRoom::kHotspotLine),
		       PrincessBedroomRoom::kBookGot, pfx(PrincessBedroomRoom::kBookGot),
		       PrincessBedroomRoom::kTakeScene, pfx(PrincessBedroomRoom::kTakeScene));

		newGameForCheck();
		const Room::Claim hotspot = claimOf(rc, "OBJ_P29_PRINCESS_BEDROOM_HOTSPOT1");
		const Room::Claim bookFresh = claimOf(rc, "OBJ_INV_SPELL_BOOK");
		_inventory->addItem("OBJ_INV_SPELL_BOOK", "ANIM_TOOLBARSPELL_BOOK", "ANIM_CURSORSPELL_BOOK");
		const Room::Claim bookHad = claimOf(rc, "OBJ_INV_SPELL_BOOK");
		debugC(3, kDebugScript, "princess check Princess_Bedroom claims: HOTSPOT1 %s depth %d; SPELL_BOOK fresh %s had %s",
		       buildName(hotspot.build), hotspot.depth, buildName(bookFresh.build), buildName(bookHad.build));
		delete rc;
	}

	// -----------------------------------------------------------------------
	// The Mermaid's cavern: the opening, the hotspot, and the echo lines its
	// room feeds the wrong-item selector.
	{
		RoomClass *rc = createRoomClass(*this, kCavern);
		debugC(3, kDebugScript, "princess check Mermaids_Cavern clips: opening %s %d; hotspot %s; wrongs dry %d; echo WRONGS %d WRONGV %d WRONGSC %d",
		       "Mermaids_Cavern.340_", pfx("Mermaids_Cavern.340_"), MermaidsCavernRoom::kHotspot,
		       pfx("Global.WRONGS_"), pfx("Global.WRONGS_Echo_"), pfx("Global.WRONGV_Echo_"), pfx("Global.WRONGSC_Echo_"));
		delete rc;
	}

	_checking = false;
}

} // End of namespace Terraglyph
