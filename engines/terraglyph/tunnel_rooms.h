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

#ifndef TERRAGLYPH_TUNNEL_ROOMS_H
#define TERRAGLYPH_TUNNEL_ROOMS_H

#include "terraglyph/room_class.h"

namespace Terraglyph {

/**
 * Showdown's dark tunnel, ROOM_P14_Tunnel_Dark (0x4194D0, vtable 0x45EB1C):
 * Shaggy and Scooby in the dark. The way out says only a line; the crate is
 * the way through. Clicked, Shaggy can't move it alone the first time; the
 * next, he and Scooby push it and the gang comes out in the post office.
 */
class TunnelDarkRoom : public RoomClass {
public:
	explicit TunnelDarkRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x4199C0
	void tick() override;                                                           // 0x419610
	bool isBusy() const override { return _crateClicked; }                          // game +0x24
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x419950
	void event(const Common::String &object, bool success) override;               // 0x419920
	bool leaveBy(const Common::String &exitObject) override;                        // 0x419530 (+0x24)

	int getCrateSprite() const { return _crate; }
	int getPushes() const { return _pushes; }
	bool isCrateClicked() const { return _crateClicked; }
	const Common::String &getLastScene() const { return _lastScene; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	int _crate = -1;             // +0xB8: the crate, a requires object
	int _exitLine = 0;           // +0xBC: which line the way out gets
	bool _crateClicked = false;  // +0xC0
	bool _pushing = false;       // +0xC1: Shaggy is at it
	int _pushes = 0;             // +0xC4: tries this visit
	Common::String _lastScene, _lastSound;
};

/**
 * Showdown's lit tunnel, ROOM_P18_Tunnel_Lit (0x4199F0, vtable 0x45EB5C):
 * the first way out the gang takes says its scene (the way to Boot Hill has
 * Tunnel_Light.180A_); after that, none does. BootHillCommentPlayed is
 * saved.
 */
class TunnelLitRoom : public RoomClass {
public:
	explicit TunnelLitRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void reset() override { _commentPlayed = false; }                               // 0x419BB0
	void leaving(const Common::String &destination, Common::String &scene) override; // 0x419A50 (+0x24)
	void saveKeys(Common::INIFile &profile) override;                               // 0x419BD0
	void loadKeys(const Common::INIFile &profile) override;                         // 0x419C10

	bool isCommentPlayed() const { return _commentPlayed; }

private:
	bool _commentPlayed = false; // +0xB8: BootHillCommentPlayed
};

/** The tunnel's room class the factory builds for @p room, or nullptr. */
RoomClass *createTunnelRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_TUNNEL_ROOMS_H
