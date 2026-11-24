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

#include "terraglyph/rooms/common/factory/factory.h"

#include "terraglyph/rooms/common/accusation.h"
#include "terraglyph/rooms/common/ghost_spot.h"
#include "terraglyph/rooms/common/menu.h"
#include "terraglyph/rooms/common/options.h"
#include "terraglyph/rooms/common/signin.h"
#include "terraglyph/rooms/class.h"
#include "terraglyph/rooms/showdown/story.h"

namespace Terraglyph {

static RoomClass *buildRoomClass(TerraglyphEngine &engine, const Common::String &room) {
	// The factory's classes for the menus, in both Gen 1 titles.
	if (room == "ROOM_Title_Screen")
		return new TitleRoom(engine);
	if (room == "ROOM_Main_Menu")
		return new MainMenuRoom(engine);
	if (room == "ROOM_Difficulty")
		return new DifficultyRoom(engine);
	if (room == "ROOM_Quit")
		return new QuitRoom(engine);
	if (room == "ROOM_Sign_In")
		return new SignInRoom(engine);
	if (room == "ROOM_Options")
		return new OptionsRoom(engine);
	if (room == "ROOM_Credits")
		return new CreditsRoom(engine);
	if (room == "ROOM_Suspect_Chooser")
		return new SuspectChooserRoom(engine);
	if (room == "ROOM_Big_Head")
		return new BigHeadRoom(engine);
	if (room == "ROOM_Ghost_Spot")
		return new GhostSpotRoom(engine);
	if (RoomClass *phantom = createPhantomRoomClass(engine, room))
		return phantom;
	if (RoomClass *showdown = createShowdownRoomClass(engine, room))
		return showdown;
	// The story rooms (story_rooms.cpp); every other room gets the base
	// class for now.
	return createStoryRoom(engine, room);
}

RoomClass *createRoomClass(TerraglyphEngine &engine, const Common::String &room) {
	// The game resets each room it builds (vtable +0x08).
	RoomClass *c = buildRoomClass(engine, room);
	if (c)
		c->reset();
	return c;
}

} // End of namespace Terraglyph
