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

#include "terraglyph/rooms/showdown/alley.h"
#include "terraglyph/rooms/showdown/boot_hill.h"
#include "terraglyph/rooms/showdown/chase.h"
#include "terraglyph/rooms/showdown/corral.h"
#include "terraglyph/rooms/showdown/gummy.h"
#include "terraglyph/rooms/showdown/hotel.h"
#include "terraglyph/rooms/showdown/jail.h"
#include "terraglyph/rooms/showdown/office.h"
#include "terraglyph/rooms/showdown/pienoon.h"
#include "terraglyph/rooms/showdown/post.h"
#include "terraglyph/rooms/showdown/saloon.h"
#include "terraglyph/rooms/showdown/sheriff.h"
#include "terraglyph/rooms/showdown/stage.h"
#include "terraglyph/rooms/showdown/station.h"
#include "terraglyph/rooms/showdown/store.h"
#include "terraglyph/rooms/showdown/strongbox.h"
#include "terraglyph/rooms/showdown/tunnel.h"
#include "terraglyph/rooms/showdown/vault.h"

namespace Terraglyph {

RoomClass *createShowdownRoomClass(TerraglyphEngine &engine, const Common::String &room) {
	// Showdown's chase (chase_rooms.cpp).
	if (RoomClass *chase = createChaseRoom(engine, room))
		return chase;
	if (RoomClass *vault = createVaultRoom(engine, room))
		return vault;
	if (RoomClass *hill = createBootHillRoom(engine, room))
		return hill;
	if (RoomClass *tunnel = createTunnelRoom(engine, room))
		return tunnel;
	if (RoomClass *hotel = createHotelRoom(engine, room))
		return hotel;
	if (RoomClass *alley = createAlleyRoom(engine, room))
		return alley;
	if (RoomClass *pie = createPieNoonRoom(engine, room))
		return pie;
	if (RoomClass *corral = createCorralRoom(engine, room))
		return corral;
	if (RoomClass *box = createStrongBoxRoom(engine, room))
		return box;
	if (RoomClass *sheriff = createSheriffRoom(engine, room))
		return sheriff;
	if (RoomClass *saloon = createSaloonRoom(engine, room))
		return saloon;
	if (RoomClass *gummy = createGummyRoom(engine, room))
		return gummy;
	if (RoomClass *station = createStationRoom(engine, room))
		return station;
	if (RoomClass *post = createPostRoom(engine, room))
		return post;
	if (RoomClass *office = createOfficeRoom(engine, room))
		return office;
	if (RoomClass *jail = createJailRoom(engine, room))
		return jail;
	if (RoomClass *store = createStoreRoom(engine, room))
		return store;
	if (RoomClass *stage = createStageRoom(engine, room))
		return stage;
	return nullptr;
}

} // End of namespace Terraglyph
