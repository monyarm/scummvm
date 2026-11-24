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

// The script-level check of Phantom's Knights of the Snack Table (WP-073),
// against an independent reference written from the trace.

#include "common/debug.h"

#include "terraglyph/formats/art_archive.h"
#include "terraglyph/detection.h"
#include "terraglyph/rooms/phantomknight/kst.h"
#include "terraglyph/graphics/room.h"
#include "terraglyph/rooms/room_class.h"
#include "terraglyph/rooms/room_script.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/audio/sound.h"

namespace Terraglyph {

void TerraglyphEngine::checkKST() {
	const char *const kKST = "ROOM_P31_KST";
	bool found = false;
	for (uint i = 0; i < _art->getRoomCount(); i++)
		found = found || _art->getRoomName(i) == kKST;
	if (!found) {
		debugC(3, kDebugScript, "kst check: no kst");
		return;
	}
	_checking = true;
	_script->setGhostChance(nullptr, 0);

	auto story = [&](int level) {
		newGameForCheck();
		setDifficulty(level);
		_sound->stop();
	};
	auto has = [&](const char *name) {
		return _sound->findClip(name) != nullptr ? 1 : 0;
	};

	// The class.
	{
		RoomClass *a = createRoomClass(*this, kKST);
		debugC(3, kDebugScript, "kst check classes: KST %s", dynamic_cast<KstRoom *>(a) ? "knights of the snack table" : "-");
		delete a;
	}

	// The marker placements (0x41A970), each sorted ascending by x.
	{
		story(0);
		showRoom(kKST);
		KstRoom *kst = dynamic_cast<KstRoom *>(getRoomClass(kKST));
		Common::String out = Common::String::format("Shaggy %d %d", (int)kst->getShaggyX(), (int)kst->getShaggyY());
		out += "; knights";
		for (uint i = 0; i < kst->getKnightCount(); i++)
			out += Common::String::format("%s %d %d", i ? "," : "", (int)kst->getKnightX(i), (int)kst->getKnightY(i));
		out += "; bench-end";
		for (uint i = 0; i < KstRoom::kMarkers; i++)
			out += Common::String::format("%s %d %d", i ? "," : "", (int)kst->getBenchEndX(i), (int)kst->getBenchEndY(i));
		out += "; bench-floor";
		for (uint i = 0; i < KstRoom::kMarkers; i++)
			out += Common::String::format("%s %d", i ? "," : "", (int)kst->getBenchFloorY(i));
		out += "; drink-spawn";
		for (uint i = 0; i < KstRoom::kMarkers; i++)
			out += Common::String::format("%s %d %d", i ? "," : "", (int)kst->getDrinkX(i), (int)kst->getDrinkY(i));
		out += "; bench-start";
		for (uint i = 0; i < KstRoom::kMarkers; i++)
			out += Common::String::format("%s %d %d", i ? "," : "", (int)kst->getBenchStartX(i), (int)kst->getBenchStartY(i));
		debugC(3, kDebugScript, "kst check markers: %s", out.c_str());
	}

	// The sprites the claim built: the three table overlays and the two counter digits.
	{
		story(0);
		showRoom(kKST);
		Room &room = *_currentRoom;
		KstRoom *kst = dynamic_cast<KstRoom *>(getRoomClass(kKST));
		Common::String out;
		for (uint i = 0; i < 3; i++) {
			const int s = kst->getOverlay(i);
			out += Common::String::format("%soverlay%u depth %d", i ? ", " : "", i + 1, s >= 0 ? room.getDepth(s) : -9999);
		}
		const int tens = kst->getTens(), ones = kst->getOnes();
		out += Common::String::format("; counter tens %d ones %d depth %d",
		                              tens >= 0 ? room.getOffsetX(tens) : -9999, ones >= 0 ? room.getOffsetX(ones) : -9999,
		                              tens >= 0 ? room.getDepth(tens) : -9999);
		debugC(3, kDebugScript, "kst check sprites: %s", out.c_str());
	}

	// The level's quota, the narrator's intro, the ambience.
	{
		Common::String out = "knights to serve";
		Common::String narrator;
		for (int l = 0; l < 3; l++) {
			story(l);
			showRoom(kKST);
			KstRoom *kst = dynamic_cast<KstRoom *>(getRoomClass(kKST));
			out += Common::String::format(" %d", kst->getToServe());
			narrator = kst->getNarrator();
		}
		out += Common::String::format("; narrator %s found %d; ambience Knights.Ambience found %d; music Knights.Music found %d",
		                              narrator.c_str(), has(narrator.c_str()), has("Knights.Ambience"), has("Knights.Music"));
		debugC(3, kDebugScript, "kst check enter: %s", out.c_str());
	}

	// The narrator's round-complete lines, and the exit.
	{
		Common::String out;
		for (int n = 1; n <= 6; n++)
			out += Common::String::format("%sCorrect%d found %d", n > 1 ? ", " : "", n, has(Common::String::format("Knights_of_the_Snack_Table.Correct%d_NARRATOR", n).c_str()));
		out += "; Zoinks found " + Common::String::format("%d", has("Knights.Zoinks"));
		out += "; exit to ROOM_P12_Dining_Room";
		debugC(3, kDebugScript, "kst check correct: %s", out.c_str());
	}

	_checking = false;
}

} // End of namespace Terraglyph
