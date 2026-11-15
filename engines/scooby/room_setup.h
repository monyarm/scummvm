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

#ifndef SCOOBY_ROOM_SETUP_H
#define SCOOBY_ROOM_SETUP_H

#include "common/hashmap.h"
#include "common/str.h"

namespace Scooby {

class Sound;

/**
 * What a room's class gives the base room (0x40B0F0) when the factory builds
 * it: the music (+0x68) and the opening scene (+0x78). Its reset (vtable
 * +0x08) may set the snacks Scooby needs here (+0x24). A room without a
 * class of its own gets empty strings and 0.
 */
struct RoomSetup {
	const char *room;
	const char *music;
	const char *opening;
	int snacksNeeded;
};

/** @p room's setup in the title @p gameId ("showdown" or "phantomknight"). */
RoomSetup getRoomSetup(const Common::String &gameId, const Common::String &room);

/**
 * The game's music (game +0x1E4, +0x20C, +0x210): a group's next variant,
 * Group<g>_<n>.Music, each time the group changes, or a clip by name.
 */
class Music {
public:
	explicit Music(Sound &sound) : _sound(sound) {}

	/** A room's music on entry (0x40B920): a group number, or a clip name. */
	void enterRoom(const Common::String &music);
	/** Once a tick (0x409CB3): when a group's clip has ended, its next variant. */
	void tick();
	/** The music clip last started, or empty. */
	const Common::String &getClip() const { return _clip; }
	int getGroup() const { return _group; }
	/**
	 * 0x40AA10 at full volume: nothing while @p group is the current one;
	 * -1 leaves no group (the clip playing goes on). Story rooms call it
	 * (Street 3's hotspot).
	 */
	void setGroup(int group);

private:
	Sound &_sound;
	int _group = -1;                         // +0x20C
	Common::HashMap<int, int> _variants;     // +0x1E4: the last variant of each group
	Common::String _clip;                    // +0x210
};

} // End of namespace Scooby

#endif // SCOOBY_ROOM_SETUP_H
