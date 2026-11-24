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

#ifndef TERRAGLYPH_DRAWBRIDGE_H
#define TERRAGLYPH_DRAWBRIDGE_H

#include "terraglyph/rooms/class.h"

namespace Terraglyph {

/**
 * Phantom's drawbridge, ROOM_P01_Drawbridge (0x426730, vtable 0x45F214):
 * four flags to raise to the four banners' colours. Each flag click lowers
 * its pennant and raises the next; with all four right, a fanfare, then the
 * drawbridge comes down and the way to the town is built.
 */
class DrawbridgeRoom : public RoomClass, public Room::Actor {
public:
	explicit DrawbridgeRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override;                                                         // 0x426790
	void enter(const Common::String &from) override;                               // 0x426840
	void tick() override;                                                          // 0x4268D0
	bool click(int sprite) override;                                               // game +0x20 bit 4, read by the flags
	void event(const Common::String &object, bool success) override;              // 0x427270
	bool leaveBy(const Common::String &exitObject) override;                       // 0x426EA0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x426A80
	void saveKeys(Common::INIFile &profile) override;                              // 0x427010
	void loadKeys(const Common::INIFile &profile) override;                        // 0x427140
	void actorUpdate(uint sprite) override;                                        // the flags' +0x0C, 0x427430
	Common::String idleLine(RoomScript &script, const Common::String &name) override; // 0x4272D0

	static const uint kFlags = 4;

	/** A flag (0x427380, vtable 0x45F254, 0x70 bytes). */
	struct Flag {
		int sprite = -1;
		int state = 0;        // +0x5C: 1-4 waving a colour; 5-12 lowering one, raising the next
		int next = 0;         // +0x60: the state a click or the sequence's end goes to
		int target = 0;       // +0x64: its banner's colour
		int number = 0;       // +0x6C: 1-4
		bool enabled = true;  // +0x68: clickable, until the puzzle is done
	};
	const Flag &getFlag(uint i) const { return _flags[i]; }
	int getBanner(uint i) const { return _banners[i]; }
	int getBannerState(uint i) const { return _bannerState[i]; }
	int getFlagState(uint i) const { return _flagState[i]; }
	void setStates(const int banner[kFlags], const int flag[kFlags]);
	int getDoor() const { return _door; }
	bool isPuzzleDone() const { return _puzzleDone; }
	void setPuzzleDone(bool done) { _puzzleDone = done; }
	const Common::String &getFanfare() const { return _fanfare; }
	/** The sounds this class played, as "name found N", for the checks. */
	const Common::String &getSounds() const { return _sounds; }
	void clearSounds() { _sounds.clear(); }

private:
	/** 0x427490: a flag to a state, with its sequence and sounds. */
	void setFlagState(uint i, int state);
	void playSound(const Common::String &name);
	int findSprite(const Common::String &object) const;

	Flag _flags[kFlags];           // +0xB8
	int _banners[kFlags] = { -1, -1, -1, -1 }; // +0xC8: OBJ_P01_DRAWBRIDGE_SP1-4
	int _door = -1;                // +0xD8
	int _bannerState[kFlags] = {}; // +0xDC: 1-4, saved as BannerState0-3
	int _flagState[kFlags] = {};   // +0xEC: 1-4, saved as FlagState0-3
	bool _puzzleDone = false;      // +0xFC, saved as PuzzleDone
	int _signLine = 0;             // +0x100: which of the sign's two lines is next
	Common::String _fanfare;       // +0x104: the second fanfare, until it is over
	int _clicked = -1;             // the sprite clicked since the last tick
	Common::Array<Common::String> _claimed; // the placements claimed, in order
	Common::String _sounds;
};

/**
 * Phantom's town, ROOM_P04_Town (0x4180F0, vtable 0x45EC5C): who walks in
 * by the room left, and while the story has Jane screaming, a scream.
 */
class TownRoom : public RoomClass {
public:
	explicit TownRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override; // 0x418150

	/** The scream this visit played, or empty. */
	const Common::String &getScream() const { return _scream; }

private:
	Common::String _scream;
};

/** The class for one of these rooms, or nullptr. */
RoomClass *createDrawbridgeRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_DRAWBRIDGE_H
