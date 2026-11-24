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

#include "common/util.h"

#include "terraglyph/rooms/common/setup/phantomknight.h"

namespace Terraglyph {

const RoomSetup kPhantom[] = {
	{ "ROOM_P31_KST", "Knights.Music", "Knights_of_the_Snack_Table.350X_", 0 },
	{ "ROOM_P07_Royal_Gallery", "1", "Royal_Gallery.210_", 0 },
	{ "ROOM_P10_Throne_Room", "1", "Throne_Room.160_", 0 },
	{ "ROOM_P11_Room_Of_Doors", "1", "Room_Of_Doors.140_", 0 },
	{ "ROOM_P12_Dining_Room", "1", "", 0 },
	{ "ROOM_P13_Kitchen", "1", "", 0 },
	{ "ROOM_Title_Screen", "Title_Screen.Music", "", 0 },
	{ "ROOM_P01_Drawbridge", "Drawbridge.Music", "Drawbridge.100A_", 0 },
	{ "ROOM_P03_Souvenir_Shop", "1", "", 0 },
	{ "ROOM_P04_Town", "Bagpipes.Music", "Town.110_", 0 },
	{ "ROOM_P05_Gardeners_House", "1", "", 0 },
	{ "ROOM_P06_Garden", "1", "", 0 },
	{ "ROOM_P08_Magic_Mirror_Room", "1", "Magic_Mirror_Room.310_", 0 },
	{ "ROOM_P09_Arena1", "1", "Arena_1.170_", 0 },
	{ "ROOM_P14_Arena2", "1", "Arena_2.180A_", 0 },
	{ "ROOM_P15_Secret_Passage1", "1", "", 0 },
	{ "ROOM_P16_Treasury", "1", "", 0 },
	{ "ROOM_P17_Kitchen_Storage", "1", "", 0 },
	{ "ROOM_P18_Storage", "1", "Storage_Room.370_", 0 },
	{ "ROOM_P19_Secret_Passage2", "1", "Secret_Passage_2.380_", 0 },
	{ "ROOM_P20_Kings_Armory", "1", "", 0 },
	{ "ROOM_P20_Kings_Armory_Slide", "1", "", 0 },
	{ "ROOM_P21_Mermaids_Cavern", "1", "Mermaids_Cavern.340_", 0 },
	{ "ROOM_P22_Well_In_Maze", "1", "", 0 },
	{ "ROOM_P23_Maze_Center", "1", "", 0 },
	{ "ROOM_P24_Wizards_Bedroom", "1", "", 0 },
	{ "ROOM_P25_Wizards_Library", "1", "Wizards_Library.300_", 0 },
	{ "ROOM_P26_Castle_Balcony", "Princess_Bedroom.Music", "", 0 },
	{ "ROOM_P27_Below_Balcony", "1", "", 0 },
	{ "ROOM_P27A_Below_Balcony_Door", "Princess_Bedroom.Music", "", 0 },
	{ "ROOM_P28_Veranda", "Princess_Bedroom.Music", "", 0 },
	{ "ROOM_P29_Princess_Bedroom", "Princess_Bedroom.Music", "Princess_Bedroom.280_", 0 },
	{ "ROOM_P44_Fountain_Closeup", "1", "Fountain.250D_", 0 },
	{ "ROOM_Ewan", "1", "Ewan.NOTEBOOKPAGE_INTRO_", 0 },
	{ "ROOM_Sign_In", "1", "", 0 },
	{ "ROOM_Difficulty", "1", "", 0 },
	{ "ROOM_Suspect_Chooser", "Track6.Music", "", 0 },
	{ "ROOM_Options", "Track6.Music", "", 0 },
	{ "ROOM_Main_Menu", "Track6.Music", "", 0 },
	{ "ROOM_Quit", "Track6.Music", "", 0 },
	{ "ROOM_Wizards_Skull", "Skull_Puzzle.Music", "", 0 },
	{ "ROOM_Brad_SPOOKY", "1", "", 0 },
	{ "ROOM_Brad_SPOOKIER", "1", "", 0 },
	{ "ROOM_Brad_SPOOKTACULAR", "1", "", 0 },
	{ "ROOM_Maria", "1", "", 0 },
	{ "ROOM_Hedge_Maze", "Hedge_Maze.Music", "", 0 },
	{ "ROOM_Maze_Map", "1", "", 0 },
	{ "ROOM_Garden_Plaque", "", "Gardeners_House.130A_", 0 },
	{ "ROOM_Credits", "1", "Credits.10002_", 0 },
	{ "ROOM_Cheat", "6", "", 0 },
	{ "ROOM_Ghost_Spot", "Ghost_Spot.Music", "", 0 },
};

const uint kPhantomCount = ARRAYSIZE(kPhantom);

} // namespace Terraglyph
