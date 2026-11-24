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

#include "terraglyph/rooms/common/setup/showdown.h"

namespace Terraglyph {

// Each class constructor's two strings for 0x40B0F0, by room name, read
// from the factories (Showdown 0x42E0E0; Phantom's names from 0x42ED7F)
// and the constructors they call. Town Center's reset (0x42D030) needs
// one snack. Rooms not listed get the base class's empty strings. The
// Credits room's opening is its factory's first scene (0x40B0F0's third
// argument); its other two are the class's own (+0xC8, +0xD0).
const RoomSetup kShowdown[] = {
	{ "ROOM_P01_Town_Center", "2", "Town_Center.100B_", 1 },
	{ "ROOM_P02_Sheriffs_Office", "5", "", 0 },
	{ "ROOM_P03_Street1", "2", "", 0 },
	{ "ROOM_P04_Street2", "2", "", 0 },
	{ "ROOM_P05_Stage", "4", "", 0 },
	{ "ROOM_P06_Stage_Trapdoor", "4", "", 0 },
	{ "ROOM_P07_Jail", "5", "", 0 },
	{ "ROOM_P08_Post_Office", "2", "", 0 },
	{ "ROOM_P09_Railroad_Station", "2", "", 0 },
	{ "ROOM_P10_Front_Of_General_Store", "2", "", 0 },
	{ "ROOM_P11_Street3", "2", "Street_3.320A_", 0 },
	{ "ROOM_P12_Saloon", "3", "Saloon.360A_", 0 },
	{ "ROOM_P13_Bank", "5", "", 0 },
	{ "ROOM_P14_Tunnel_Dark", "5", "Tunnel_Dark.170A_", 0 },
	{ "ROOM_P15_General_Store_Interior", "5", "", 0 },
	{ "ROOM_P16_Bank_Vault", "5", "", 0 },
	{ "ROOM_P17_Hotel_Hall", "3", "", 0 },
	{ "ROOM_P18_Tunnel_Lit", "5", "", 0 },
	{ "ROOM_P19_Back_Alley", "5", "Back_Alley.350A_", 0 },
	{ "ROOM_P20_Hotel_Room", "3", "", 0 },
	{ "ROOM_P21_Boot_Hill1", "6", "", 0 },
	{ "ROOM_P22_Boot_Hill2", "6", "", 0 },
	{ "ROOM_P23_Boot_Hill3", "6", "", 0 },
	{ "ROOM_P25A_Chase_B2_Closeup", "Track9.Music", "", 0 },
	{ "ROOM_P24_Chase_B1", "Chase.Music", "", 0 },
	{ "ROOM_P25_Chase_B2", "Chase.Music", "", 0 },
	{ "ROOM_P26_Chase_B3", "Chase.Music", "", 0 },
	{ "ROOM_P27_Chase_B4", "Chase.Music", "", 0 },
	{ "ROOM_P28_Chase_B5", "Chase.Music", "", 0 },
	{ "ROOM_P29_Chase_B6", "Chase.Music", "", 0 },
	{ "ROOM_P30_Horseshoe_Corral", "Horseshoe_Corral.Music", "", 0 },
	{ "ROOM_P33_Chase_C1", "Chase.Music", "", 0 },
	{ "ROOM_P37_Chase_C2", "Chase.Music", "", 0 },
	{ "ROOM_P38_Chase_C3", "Chase.Music", "", 0 },
	{ "ROOM_P39_Chase_C4", "Chase.Music", "", 0 },
	{ "ROOM_P34_Chase_C5", "Chase.Music", "", 0 },
	{ "ROOM_P35_Chase_C6", "Chase.Music", "", 0 },
	{ "ROOM_Sign_In", "1", "", 0 },
	{ "ROOM_Difficulty", "1", "", 0 },
	{ "ROOM_Suspect_Chooser", "Suspect.Music", "", 0 },
	{ "ROOM_Artie", "3", "", 0 },
	{ "ROOM_Artifact_Bowl_09", "Track9.Music", "", 0 },
	{ "ROOM_Artifact_Bowl_12", "Track9.Music", "", 0 },
	{ "ROOM_Artifact_Bowl_15", "Track9.Music", "", 0 },
	{ "ROOM_Artifact_Deer_09", "Track9.Music", "", 0 },
	{ "ROOM_Artifact_Deer_12", "Track9.Music", "", 0 },
	{ "ROOM_Artifact_Deer_15", "Track9.Music", "", 0 },
	{ "ROOM_Artifact_Vase_09", "Track9.Music", "", 0 },
	{ "ROOM_Artifact_Vase_12", "Track9.Music", "", 0 },
	{ "ROOM_Artifact_Vase_15", "Track9.Music", "", 0 },
	{ "ROOM_P40_Chase_Start", "Chase.Music", "", 0 },
	{ "ROOM_Sheriff", "5", "", 0 },
	{ "ROOM_Options", "1", "", 0 },
	{ "ROOM_Main_Menu", "1", "", 0 },
	{ "ROOM_Quit", "1", "", 0 },
	{ "ROOM_Gummy", "Gummy.Music", "Gummy_Puzzle.220B_", 0 },
	{ "ROOM_Title_Screen", "Title_Screen.Music", "", 0 },
	{ "ROOM_Credits", "1", "Credits.1100A2_", 0 },
	{ "ROOM_Vault_Clock", "5", "", 0 },
	{ "ROOM_Cheat", "6", "", 0 },
	{ "ROOM_Ghost_Spot", "Ghost_Spot.Music", "", 0 },
	{ "ROOM_Test_Scroll", "1", "", 0 },
};

const uint kShowdownCount = ARRAYSIZE(kShowdown);

} // namespace Terraglyph
