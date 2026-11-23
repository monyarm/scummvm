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

// The script-level check of Phantom's wizard's skull and library (WP-077)
// against an independent reference written from the trace. Showdown has neither
// room.

#include "common/debug.h"

#include "terraglyph/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"
#include "terraglyph/wizards_rooms.h"

namespace Terraglyph {

static const char *wizBuildName(Room::Claim::Build b) {
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

void TerraglyphEngine::checkWizards() {
	const char *const kSkull = "ROOM_Wizards_Skull";
	const char *const kLibrary = "ROOM_P25_Wizards_Library";

	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kSkull;
	if (!found) {
		debugC(3, kDebugScript, "wizards check: no wizard's skull");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	// Every clip here is a dotted voice line (it ships with a speaker suffix), so
	// they match by prefix; reading exactly would say "found 0" of a name that is
	// right — the WP-074 trap.
	auto pfx = [&](const char *name) { return _sound->hasClipPrefix(name) ? 1 : 0; };

	auto claimOf = [&](RoomClass *rc, const char *object) {
		Room::Claim claim;
		rc->claimPlacement(object, claim);
		return claim;
	};

	// The classes the factory builds.
	{
		RoomClass *a = createRoomClass(*this, kSkull);
		RoomClass *b = createRoomClass(*this, kLibrary);
		debugC(3, kDebugScript, "wizards check classes: Wizards_Skull %s, Wizards_Library %s",
		       dynamic_cast<WizardsSkullRoom *>(a) ? "skull" : a ? "other" : "-",
		       dynamic_cast<WizardsLibraryRoom *>(b) ? "library" : b ? "other" : "-");
		delete a;
		delete b;
	}

	// -----------------------------------------------------------------------
	// The wizard's skull.
	{
		RoomClass *rc = createRoomClass(*this, kSkull);
		WizardsSkullRoom *sk = dynamic_cast<WizardsSkullRoom *>(rc);

		debugC(3, kDebugScript, "wizards check Skull clips: intro %s %d; demo %s %d; slide %s %d",
		       WizardsSkullRoom::kIntro, pfx(WizardsSkullRoom::kIntro), WizardsSkullRoom::kDemo, pfx(WizardsSkullRoom::kDemo),
		       WizardsSkullRoom::kSlide, pfx(WizardsSkullRoom::kSlide));

		newGameForCheck();
		// Fresh: every skull sits on face A (rotation 0).
		const Room::Claim s1 = claimOf(rc, "OBJ_SKULL1");
		const Room::Claim hand = claimOf(rc, "OBJ_SKULL_HAND");
		const Room::Claim exit = claimOf(rc, "OBJ_SKULL_EXIT");
		debugC(3, kDebugScript, "wizards check Skull claims: SKULL1 %s anim '%s'; HAND %s hidden %d; EXIT %s",
		       wizBuildName(s1.build), s1.anim.c_str(), wizBuildName(hand.build), hand.hidden ? 1 : 0, wizBuildName(exit.build));

		// The saved rotations drive the skull faces: a loaded Skull1=2 shows face C.
		Common::INIFile saved;
		saved.setKey("Skull1", kSkull, "2");
		saved.setKey("Skull2", kSkull, "0");
		saved.setKey("Skull3", kSkull, "3");
		saved.setKey("Skull4", kSkull, "1");
		saved.setKey("IntroDone", kSkull, "Yes");
		sk->loadKeys(saved);
		const int r0 = sk->getState(0), r1 = sk->getState(1), r2 = sk->getState(2), r3 = sk->getState(3);
		const bool introRead = sk->introDone();
		const Room::Claim s1c = claimOf(rc, "OBJ_SKULL1");
		sk->reset();
		Common::INIFile none;
		sk->loadKeys(none); // missing keys read as the reset defaults
		debugC(3, kDebugScript, "wizards check Skull state: rotations read %d,%d,%d,%d reset %d,%d,%d,%d; IntroDone read %d reset %d; SKULL1 at %d -> anim '%s'",
		       r0, r1, r2, r3, sk->getState(0), sk->getState(1), sk->getState(2), sk->getState(3),
		       introRead ? 1 : 0, sk->introDone() ? 1 : 0, 2, s1c.anim.c_str());

		debugC(3, kDebugScript, "wizards check Skull flow: attempt flag SkullPuzzleAttempted; demo flag SkullMovie; solved -> %s movie 6 + SkullPuzzle; demo done -> %s movie 31; combo keys SkullCombo0-3",
		       "ROOM_P24_Wizards_Bedroom", "ROOM_P08_Magic_Mirror_Room");
		delete rc;
	}

	// -----------------------------------------------------------------------
	// The wizard's library.
	{
		RoomClass *rc = createRoomClass(*this, kLibrary);

		debugC(3, kDebugScript, "wizards check Library clips: reveal %s %d; taunt %s %d / %s %d; hint %s %d / %s %d / %s %d; hotspot1 %s %d; hotspot3 %s %d",
		       WizardsLibraryRoom::kRevealVoice, pfx(WizardsLibraryRoom::kRevealVoice),
		       WizardsLibraryRoom::kTaunt[0], pfx(WizardsLibraryRoom::kTaunt[0]), WizardsLibraryRoom::kTaunt[1], pfx(WizardsLibraryRoom::kTaunt[1]),
		       WizardsLibraryRoom::kShelfHint[0], pfx(WizardsLibraryRoom::kShelfHint[0]), WizardsLibraryRoom::kShelfHint[1], pfx(WizardsLibraryRoom::kShelfHint[1]),
		       WizardsLibraryRoom::kShelfHint[2], pfx(WizardsLibraryRoom::kShelfHint[2]),
		       WizardsLibraryRoom::kHotspot1, pfx(WizardsLibraryRoom::kHotspot1), WizardsLibraryRoom::kHotspot3, pfx(WizardsLibraryRoom::kHotspot3));

		newGameForCheck();
		const Room::Claim underHidden = claimOf(rc, "OBJ_P25_WIZARDS_LIBRARY_UNDERLAY");
		const Room::Claim shelfOpen = claimOf(rc, "OBJ_P25_WIZARDS_LIBRARY_SHELF");
		const Room::Claim p24Hidden = claimOf(rc, "OBJ_P25_TO_P24");
		setGameFlag("SkullPuzzle", true);
		const Room::Claim underSolved = claimOf(rc, "OBJ_P25_WIZARDS_LIBRARY_UNDERLAY");
		const Room::Claim shelfSolved = claimOf(rc, "OBJ_P25_WIZARDS_LIBRARY_SHELF");
		const Room::Claim p24Solved = claimOf(rc, "OBJ_P25_TO_P24");
		debugC(3, kDebugScript, "wizards check Library claims: UNDERLAY unsolved %s solved %s (depth %d); SHELF unsolved %s solved %s; TO_P24 unsolved %s solved %s",
		       wizBuildName(underHidden.build), wizBuildName(underSolved.build), underSolved.depth,
		       wizBuildName(shelfOpen.build), wizBuildName(shelfSolved.build),
		       wizBuildName(p24Hidden.build), wizBuildName(p24Solved.build));

		debugC(3, kDebugScript, "wizards check Library reveal: first (SkullMovie clear) -> Big Head %s on %s; after -> %s; movie 61",
		       WizardsLibraryRoom::kRevealVoice, WizardsLibraryRoom::kRevealBg, WizardsLibraryRoom::kSkullRoom);
		delete rc;
	}

	_checking = false;
}

} // End of namespace Terraglyph
