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

#include "scooby/detection.h"
#include "scooby/scooby.h"

namespace Scooby {

// The suspects in the clue table's order (0x4301E0 numbers them so).
static const char *const kSuspects[] = { "ARTIE", "GUMMY", "ICDOUBLE", "JENNY", "SHERIFF" };

// Each suspect's five clues (0x46FA3C, five a suspect): three of their
// own, then two more.
static const char *const kClues[][5] = {
	{ "ARTIE_ESCAPE_BOOK", "ARTIE_CLASS_LIST", "ARTIE_MAP", "ARTIE_CLOTH", "ARTIE_HATBAND" },
	{ "GUMMY_COWBELL", "GUMMY_HORSESHOE_LIFT", "GUMMY_MAP", "GUMMY_CHOMPTITE", "GUMMY_CACTUS_PIN" },
	{ "IC_BINOCULARS", "IC_EYE_DOCTOR", "IC_MAP", "IC_WATCH_CHAIN", "IC_CLEANING_CLOTH" },
	{ "JENNY_CASSETTE_TAPE", "JENNY_GHOST_TOWN_BOOK", "JENNY_MAP", "JENNY_COW_EARRING", "JENNY_COMPACT" },
	{ "SHERIFF_SPURS", "SHERIFF_FEATHER", "SHERIFF_MAP", "SHERIFF_EL_SECO_RECIEPT", "SHERIFF_HANDCUFFS" }
};

// The red herrings (0x46FAA0), one a suspect.
static const char *const kRedHerrings[] = {
	"RH_ARTIE_BRUSH", "RH_GUMMY_CLOTH", "RH_IC_SOAP", "RH_JENNY_GHOSTLY_TALES", "RH_SHERIFF_GOLD_STAR"
};

static const uint kClueSlots = 5;

static int suspectIndex(const Common::String &name) {
	for (uint i = 0; i < ARRAYSIZE(kSuspects); i++) {
		if (name == kSuspects[i])
			return i;
	}
	return -1;
}

bool ScoobyEngine::isClueFound(const Common::String &clue) const {
	// 0x4301A0 (game vtable +0x68): one of CluesFound0-4 (game +0xB0).
	for (uint i = 0; i < kClueSlots; i++) {
		if (getGameString(Common::String::format("CluesFound%u", i)) == clue)
			return true;
	}
	return false;
}

void ScoobyEngine::giveClue(const Common::String &suspect) {
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
	const Common::String villain = getGameString("Whodunit");
	const int giver = suspectIndex(suspect);
	Common::String clue;
	for (int tries = 0; tries < 1000; tries++) {
		if (suspect == villain || giver < 0) {
			// 0x4301E0: a red herring about neither the villain nor the
			// giver (rand() % 5 until it is neither).
			const int v = suspectIndex(villain);
			uint r;
			do {
				r = getRandomNumber(4);
			} while ((int)r == v || (int)r == giver);
			clue = kRedHerrings[r];
		} else {
			// One of their own three until all three are found; then, at
			// the first level, the fourth or fifth (rand() & 1); above it,
			// a red herring.
			const uint extra = getRandomNumber(1) + 3;
			if (!isClueFound(kClues[giver][0]) || !isClueFound(kClues[giver][1]) || !isClueFound(kClues[giver][2])) {
				clue = kClues[giver][getRandomNumber(2)];
			} else if (_difficulty == 0) {
				clue = kClues[giver][extra];
			} else {
				const int v = suspectIndex(villain);
				uint r;
				do {
					r = getRandomNumber(4);
				} while ((int)r == v || (int)r == giver);
				clue = kRedHerrings[r];
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

} // End of namespace Scooby
