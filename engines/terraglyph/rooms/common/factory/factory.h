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

#ifndef TERRAGLYPH_ROOM_FACTORY_H
#define TERRAGLYPH_ROOM_FACTORY_H

#include "common/str.h"

namespace Terraglyph {

class RoomClass;
class TerraglyphEngine;

/** The room's class, or one with only the base behavior. */
RoomClass *createRoomClass(TerraglyphEngine &engine, const Common::String &room);

/** @p room's Phantom Knight class, or nullptr if the title has none for it. */
RoomClass *createPhantomRoomClass(TerraglyphEngine &engine, const Common::String &room);

/** @p room's Showdown class, or nullptr if the title has none for it. */
RoomClass *createShowdownRoomClass(TerraglyphEngine &engine, const Common::String &room);

} // namespace Terraglyph

#endif
