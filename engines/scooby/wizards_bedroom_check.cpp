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

// The script-level check of Phantom's wizard's bedroom and veranda (WP-081)
// against an independent reference written from the trace. Showdown has neither
// room.

#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/detection.h"
#include "scooby/inventory.h"
#include "scooby/room.h"
#include "scooby/room_class.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"
#include "scooby/wizards_bedroom_rooms.h"

namespace Scooby {

static const char *bedroomBuildName(Room::Claim::Build b) {
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

void ScoobyEngine::checkWizardsBedroom() {
	const char *const kBedroom = "ROOM_P24_Wizards_Bedroom";
	const char *const kVeranda = "ROOM_P28_Veranda";

	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kBedroom;
	if (!found) {
		debugC(3, kDebugScript, "wizard's bedroom check: no wizard's bedroom");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	// The named scene lines ship with a speaker suffix, so they match by prefix
	// (reading exactly would say "found 0" of a name that is right — the WP-074
	// trap).
	auto pfx = [&](const char *name) { return _sound->hasClipPrefix(name) ? 1 : 0; };

	auto claimOf = [&](RoomClass *rc, const char *object) {
		Room::Claim claim;
		rc->claimPlacement(object, claim);
		return claim;
	};

	// The classes the factory builds.
	{
		RoomClass *a = createRoomClass(*this, kBedroom);
		RoomClass *b = createRoomClass(*this, kVeranda);
		debugC(3, kDebugScript, "wizard's bedroom check classes: Wizards_Bedroom %s; Veranda %s",
		       dynamic_cast<WizardsBedroomRoom *>(a) ? "bedroom" : a ? "other" : "-",
		       dynamic_cast<VerandaRoom *>(b) ? "veranda" : b ? "other" : "-");
		delete a;
		delete b;
	}

	// -----------------------------------------------------------------------
	// The wizard's bedroom: the scroll and the accusation lines, and the
	// maze-map scroll's claim.
	{
		RoomClass *rc = createRoomClass(*this, kBedroom);
		const Room::Claim mazeMap = claimOf(rc, "OBJ_INV_MAZE_MAP");
		// Global.Thank_You_ is a scene (the accusation the dining room and arenas
		// also use), not a sound clip, so it is named, not prefix-probed — a
		// prefix probe would read "0" of a name that is right (the WP-074 trap).
		debugC(3, kDebugScript, "wizard's bedroom check Wizards_Bedroom: scroll %s %d; accusation scene %s -> ROOM_Suspect_Chooser; MAZE_MAP %s (music \"1\", no opening)",
		       "Wizards_Bedroom.Scroll", pfx("Wizards_Bedroom.Scroll"),
		       "Global.Thank_You_",
		       bedroomBuildName(mazeMap.build));
		delete rc;
	}

	// -----------------------------------------------------------------------
	// The veranda: the first-entry and well lines (its Big Head background
	// BIG_HEAD_BG_P44P22 is an art asset the live draw verifies, not a clip).
	{
		RoomClass *rc = createRoomClass(*this, kVeranda);
		debugC(3, kDebugScript, "wizard's bedroom check Veranda clips: intro %s %d; well %s %d; well1 %s %d; well2 %s %d; well3 %s %d (music \"Princess_Bedroom.Music\", Big Head BIG_HEAD_BG_P44P22)",
		       "Veranda.270D_", pfx("Veranda.270D_"),
		       "Veranda.290_", pfx("Veranda.290_"),
		       "Veranda.290B_", pfx("Veranda.290B_"),
		       "Veranda.290X1_", pfx("Veranda.290X1_"),
		       "Veranda.290X2_", pfx("Veranda.290X2_"));
		delete rc;
	}

	_checking = false;
}

} // End of namespace Scooby
