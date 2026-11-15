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

#include "common/debug.h"

#include "scooby/art_archive.h"
#include "scooby/detection.h"
#include "scooby/room_class.h"
#include "scooby/room_script.h"
#include "scooby/room_setup.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

// Each class constructor's two strings for 0x40B0F0, by room name, read
// from the factories (Showdown 0x42E0E0; Phantom's names from 0x42ED7F)
// and the constructors they call. Town Center's reset (0x42D030) needs
// one snack. Rooms not listed get the base class's empty strings. The
// Credits room's opening is its factory's first scene (0x40B0F0's third
// argument); its other two are the class's own (+0xC8, +0xD0).
static const RoomSetup kShowdown[] = {
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

static const RoomSetup kPhantom[] = {
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

RoomSetup getRoomSetup(const Common::String &gameId, const Common::String &room) {
	const bool phantom = gameId == "phantomknight";
	const RoomSetup *table = phantom ? kPhantom : kShowdown;
	const uint count = phantom ? ARRAYSIZE(kPhantom) : ARRAYSIZE(kShowdown);
	for (uint i = 0; i < count; i++) {
		if (room == table[i].room)
			return table[i];
	}
	const RoomSetup none = { "", "", "", 0 };
	return none;
}

void Music::enterRoom(const Common::String &music) {
	// 0x40B920: a number is a group; anything else (an empty string too)
	// leaves no group and plays that clip, if it exists.
	const int group = atoi(music.c_str());
	if (group < 1) {
		setGroup(-1);
		if (!music.empty() && _sound.play(music, true)) {
			_clip = music;
			debugC(1, kDebugAudio, "music %s", music.c_str());
		}
		return;
	}
	setGroup(group);
}

void Music::setGroup(int group) {
	// 0x40AA10: nothing while the group is the same; else its next variant,
	// back to 1 when that one doesn't exist. Stop it, then play it.
	if (group == _group)
		return;
	_group = group;
	if (group < 0)
		return;
	int &variant = _variants.getOrCreateVal(group);
	variant++;
	Common::String clip = Common::String::format("Group%d_%d.Music", group, variant);
	if (!_sound.findClip(clip)) {
		variant = 1;
		clip = Common::String::format("Group%d_%d.Music", group, variant);
	}
	_clip = clip;
	_sound.stopClip(clip);
	_sound.play(clip, true);
	debugC(1, kDebugAudio, "music group %d: %s", group, clip.c_str());
}

void Music::tick() {
	// 0x409CB3: with a group playing, its clip over picks the next variant.
	if (_group < 0 || _sound.isClipPlaying(_clip))
		return;
	const int group = _group;
	setGroup(-1);
	setGroup(group);
}

void ScoobyEngine::saveRoomKeys(Common::INIFile &profile) {
	// 0x42F3A0: every room in the game's list writes its own section: the
	// base room's keys (0x40C410); its class's follow (vtable +0x1C).
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String room = _art->getRoomName(i);
		const RoomScript::Memory &m = _script->getMemory(room);
		profile.setKey("OpeningPlayed", room, m.openingPlayed ? "Yes" : "No");
		profile.setKey("SnacksGiven", room, Common::String::format("%u", m.snacksGiven));
	}
}

void ScoobyEngine::loadRoomKeys(const Common::INIFile &profile) {
	// 0x42F640 -> each room's vtable +0x20, the base part (0x40C470): only
	// Yes is true, and a missing key reads as empty: No, 0 (atoi).
	for (uint i = 0; i < _art->getRoomCount(); i++) {
		const Common::String room = _art->getRoomName(i);
		RoomScript::Memory &m = _script->getMemory(room);
		Common::String v;
		m.openingPlayed = profile.getKey("OpeningPlayed", room, v) && v == "Yes";
		m.snacksGiven = profile.getKey("SnacksGiven", room, v) ? (uint)MAX(0, atoi(v.c_str())) : 0;
	}
}

} // End of namespace Scooby