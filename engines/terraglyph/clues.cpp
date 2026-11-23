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

// The clue a suspect gives (Showdown's game vtable +0x44, 0x42F990), WP-034.

#include "common/debug.h"

#include "terraglyph/detection.h"
#include "terraglyph/terraglyph.h"

namespace Terraglyph {

// Each title's clue tables. Showdown's: the suspects in the clue table's
// order (0x4301E0 numbers them so); each suspect's five clues (0x46FA3C,
// five a suspect): three of their own, then two more; the red herrings
// (0x46FAA0), one a suspect. Phantom's (0x4305C0, 0x430F00, WP-061): the
// same, its five clues a suspect at 0x46E88C, and two red herrings a
// suspect at 0x46E8F0, one picked by rand() & 1.
struct ClueTables {
	const char *suspects[5];
	const char *clues[5][5];
	const char *redHerrings[5][2];
	uint herringsEach;
};

static const ClueTables kShowdownClues = {
	{ "ARTIE", "GUMMY", "ICDOUBLE", "JENNY", "SHERIFF" },
	{
		{ "ARTIE_ESCAPE_BOOK", "ARTIE_CLASS_LIST", "ARTIE_MAP", "ARTIE_CLOTH", "ARTIE_HATBAND" },
		{ "GUMMY_COWBELL", "GUMMY_HORSESHOE_LIFT", "GUMMY_MAP", "GUMMY_CHOMPTITE", "GUMMY_CACTUS_PIN" },
		{ "IC_BINOCULARS", "IC_EYE_DOCTOR", "IC_MAP", "IC_WATCH_CHAIN", "IC_CLEANING_CLOTH" },
		{ "JENNY_CASSETTE_TAPE", "JENNY_GHOST_TOWN_BOOK", "JENNY_MAP", "JENNY_COW_EARRING", "JENNY_COMPACT" },
		{ "SHERIFF_SPURS", "SHERIFF_FEATHER", "SHERIFF_MAP", "SHERIFF_EL_SECO_RECIEPT", "SHERIFF_HANDCUFFS" }
	},
	{
		{ "RH_ARTIE_BRUSH", nullptr }, { "RH_GUMMY_CLOTH", nullptr }, { "RH_IC_SOAP", nullptr },
		{ "RH_JENNY_GHOSTLY_TALES", nullptr }, { "RH_SHERIFF_GOLD_STAR", nullptr }
	},
	1
};

static const ClueTables kPhantomClues = {
	{ "BRAD", "EWAN", "JESTER", "MARIA", "WIZARD" },
	{
		{ "BRAD_COUPON", "BRAD_GLOVE", "BRAD_OIL", "BRAD_COAT", "BRAD_SHEATH" },
		{ "EWAN_COUPON", "EWAN_BELLOWS", "EWAN_K", "EWAN_FILM", "EWAN_SPOON" },
		{ "JESTER_COUPON", "JESTER_SIGN", "JESTER_BOOK", "JESTER_MAKEUP", "JESTER_BELL" },
		{ "MARIA_LICENSE", "MARIA_COUPON", "MARIA_KEYS", "MARIA_BOOTS", "MARIA_BOW" },
		{ "WIZARD_SMOKEBOMBS", "WIZARD_COUPON", "WIZARD_SCROLL", "WIZARD_BOOK", "WIZARD_WAND" }
	},
	{
		{ "RH_BRAD_SCHEDULE", "RH_BRAD_HELMET" }, { "RH_EWAN_PAN", "RH_EWAN_SPATULA" },
		{ "RH_JESTER_CLOTH", "RH_JESTER_TAP_SHOES" }, { "RH_MARIA_BOOK", "RH_MARIA_MUFFLER" },
		{ "RH_WIZARD_POTION", "RH_WIZARD_TESTTUBE" }
	},
	2
};

static const uint kClueSlots = 5;

static int suspectIndex(const ClueTables &t, const Common::String &name) {
	for (uint i = 0; i < ARRAYSIZE(t.suspects); i++) {
		if (name == t.suspects[i])
			return i;
	}
	return -1;
}

bool TerraglyphEngine::isClueFound(const Common::String &clue) const {
	// 0x4301A0 (game vtable +0x68): one of CluesFound0-4 (game +0xB0).
	for (uint i = 0; i < kClueSlots; i++) {
		if (getGameString(Common::String::format("CluesFound%u", i)) == clue)
			return true;
	}
	return false;
}

void TerraglyphEngine::giveClue(const Common::String &suspect) {
	// 0x42F990: a suspect gives one clue, once.
	if (isClueOwner(suspect))
		return;
	int slot = -1;
	for (uint i = 0; i < kClueSlots && slot < 0; i++) {
		if (getGameString(Common::String::format("CluesFound%u", i)).empty())
			slot = i;
	}
	if (slot < 0)
		return;
	const ClueTables &t = getGameId() == "showdown" ? kShowdownClues : kPhantomClues;
	const Common::String villain = getGameString("Whodunit");
	const int giver = suspectIndex(t, suspect);
	// 0x4301E0 (Phantom 0x430F00): a red herring about neither the villain
	// nor the giver (rand() % 5 until it is neither); Phantom's has two a
	// suspect, rand() & 1.
	auto redHerring = [&]() {
		const int v = suspectIndex(t, villain);
		uint r;
		do {
			r = getRandomNumber(4);
		} while ((int)r == v || (int)r == giver);
		return Common::String(t.redHerrings[r][t.herringsEach == 2 ? getRandomNumber(1) : 0]);
	};
	Common::String clue;
	for (int tries = 0; tries < 1000; tries++) {
		if (suspect == villain || giver < 0) {
			clue = redHerring();
		} else {
			// One of their own three until all three are found; then, at
			// the first level, the fourth or fifth (rand() & 1); above it,
			// a red herring.
			const uint extra = getRandomNumber(1) + 3;
			if (!isClueFound(t.clues[giver][0]) || !isClueFound(t.clues[giver][1]) || !isClueFound(t.clues[giver][2])) {
				clue = t.clues[giver][getRandomNumber(2)];
			} else if (_difficulty == 0) {
				clue = t.clues[giver][extra];
			} else {
				clue = redHerring();
			}
		}
		// One already found is drawn again.
		if (!isClueFound(clue))
			break;
	}
	setGameString(Common::String::format("CluesFound%d", slot), clue);
	setGameString(Common::String::format("ClueOwners%d", slot), suspect);
	setNewClue(true);
	debugC(1, kDebugScript, "Generated clue %d (%s) from %s, the villain %s", slot, clue.c_str(), suspect.c_str(), villain.c_str());
}

} // End of namespace Terraglyph
