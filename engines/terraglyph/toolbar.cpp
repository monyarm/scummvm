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

#include "terraglyph/toolbar.h"

namespace Terraglyph {

// The rooms whose class's entry (room vtable +0x0C) calls 0x407CC0 after
// the base entry, or never runs the base entry, which builds the toolbar.
// Traced through each title's room factory (Showdown 0x42E0E0; Phantom's
// room-name compares start at 0x42ED7F) to the class constructor, its
// vtable and that vtable's +0x0C. Rooms the factories don't name get the
// base class, which shows the toolbar.
static const char *const kHiddenRooms[] = {
	// Both titles. Suspect_Chooser, Quit and Cheat never build it.
	"ROOM_Sign_In", "ROOM_Difficulty", "ROOM_Suspect_Chooser", "ROOM_Options", "ROOM_Main_Menu", "ROOM_Quit",
	"ROOM_Title_Screen", "ROOM_Big_Head", "ROOM_Credits", "ROOM_Cheat", "ROOM_Ghost_Spot",
	// Showdown
	"ROOM_P32_Pie_Noon", "ROOM_P06_Stage_Trapdoor", "ROOM_P24_Chase_B1", "ROOM_P25_Chase_B2", "ROOM_P26_Chase_B3",
	"ROOM_P27_Chase_B4", "ROOM_P28_Chase_B5", "ROOM_P29_Chase_B6", "ROOM_P30_Horseshoe_Corral", "ROOM_P33_Chase_C1",
	"ROOM_P37_Chase_C2", "ROOM_P38_Chase_C3", "ROOM_P39_Chase_C4", "ROOM_P34_Chase_C5", "ROOM_P35_Chase_C6",
	"ROOM_Artie", "ROOM_Artifact_Bowl_09", "ROOM_Artifact_Bowl_12", "ROOM_Artifact_Bowl_15", "ROOM_Artifact_Deer_09",
	"ROOM_Artifact_Deer_12", "ROOM_Artifact_Deer_15", "ROOM_Artifact_Vase_09", "ROOM_Artifact_Vase_12",
	"ROOM_Artifact_Vase_15", "ROOM_Sheriff", "ROOM_Gummy", "ROOM_Vault_Clock", "ROOM_Open_Safe", "ROOM_SB1", "ROOM_SB2",
	"ROOM_SB3", "ROOM_SB4", "ROOM_SB5", "ROOM_Test_Scroll",
	// Phantom. P31_KST never builds it.
	"ROOM_P31_KST", "ROOM_P30_WFR", "ROOM_Ewan", "ROOM_Wizards_Skull", "ROOM_Brad_SPOOKY", "ROOM_Brad_SPOOKIER",
	"ROOM_Brad_SPOOKTACULAR", "ROOM_Maria", "ROOM_Hedge_Maze", "ROOM_Maze_Map", "ROOM_Garden_Plaque"
};

bool isToolbarHidden(const Common::String &room) {
	for (uint i = 0; i < ARRAYSIZE(kHiddenRooms); i++) {
		if (room == kHiddenRooms[i])
			return true;
	}
	return false;
}

} // End of namespace Terraglyph
