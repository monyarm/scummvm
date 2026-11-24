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

#include "terraglyph/rooms/common/factory/room_factory.h"

#include "terraglyph/rooms/phantomknight/arena.h"
#include "terraglyph/rooms/phantomknight/armory.h"
#include "terraglyph/rooms/phantomknight/below_balcony.h"
#include "terraglyph/rooms/phantomknight/brad.h"
#include "terraglyph/rooms/phantomknight/dining.h"
#include "terraglyph/rooms/phantomknight/doors.h"
#include "terraglyph/rooms/phantomknight/drawbridge.h"
#include "terraglyph/rooms/phantomknight/ewan.h"
#include "terraglyph/rooms/phantomknight/gallery.h"
#include "terraglyph/rooms/phantomknight/garden.h"
#include "terraglyph/rooms/phantomknight/kitchen.h"
#include "terraglyph/rooms/phantomknight/kst.h"
#include "terraglyph/rooms/phantomknight/maria.h"
#include "terraglyph/rooms/phantomknight/maze_interior.h"
#include "terraglyph/rooms/phantomknight/maze.h"
#include "terraglyph/rooms/phantomknight/passage.h"
#include "terraglyph/rooms/phantomknight/princess.h"
#include "terraglyph/rooms/phantomknight/souvenir.h"
#include "terraglyph/rooms/phantomknight/storage.h"
#include "terraglyph/rooms/phantomknight/throne.h"
#include "terraglyph/rooms/phantomknight/wizards_bedroom.h"
#include "terraglyph/rooms/phantomknight/wizards.h"

namespace Terraglyph {

RoomClass *createPhantomRoomClass(TerraglyphEngine &engine, const Common::String &room) {
	// Phantom's drawbridge and town (drawbridge_rooms.cpp).
	if (RoomClass *drawbridge = createDrawbridgeRoom(engine, room))
		return drawbridge;
	if (RoomClass *souvenir = createSouvenirRoom(engine, room))
		return souvenir;
	if (RoomClass *garden = createGardenRoom(engine, room))
		return garden;
	if (RoomClass *doors = createDoorsRoom(engine, room))
		return doors;
	if (RoomClass *throne = createThroneRoom(engine, room))
		return throne;
	if (RoomClass *dining = createDiningRoom(engine, room))
		return dining;
	if (RoomClass *maria = createMariaRoom(engine, room))
		return maria;
	if (RoomClass *kitchen = createKitchenRoom(engine, room))
		return kitchen;
	if (RoomClass *ewan = createEwanRoom(engine, room))
		return ewan;
	if (RoomClass *storage = createStorageRoom(engine, room))
		return storage;
	if (RoomClass *armory = createArmoryRoom(engine, room))
		return armory;
	if (RoomClass *arena = createArenaRoom(engine, room))
		return arena;
	if (RoomClass *brad = createBradRoom(engine, room))
		return brad;
	if (RoomClass *passage = createPassageRoom(engine, room))
		return passage;
	if (RoomClass *gallery = createGalleryRoom(engine, room))
		return gallery;
	if (RoomClass *kst = createKstRoom(engine, room))
		return kst;
	if (RoomClass *maze = createMazeRoom(engine, room))
		return maze;
	if (RoomClass *mazeInterior = createMazeInteriorRoom(engine, room))
		return mazeInterior;
	if (RoomClass *belowBalcony = createBelowBalconyRoom(engine, room))
		return belowBalcony;
	if (RoomClass *wizards = createWizardsRoom(engine, room))
		return wizards;
	if (RoomClass *princess = createPrincessRoom(engine, room))
		return princess;
	if (RoomClass *wizardsBedroom = createWizardsBedroomRoom(engine, room))
		return wizardsBedroom;
	return nullptr;
}

} // End of namespace Terraglyph
