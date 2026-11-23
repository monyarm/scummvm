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

#ifndef TERRAGLYPH_CORRAL_ROOMS_H
#define TERRAGLYPH_CORRAL_ROOMS_H

#include "common/array.h"

#include "terraglyph/room.h"
#include "terraglyph/room_class.h"
#include "terraglyph/room_script.h"

namespace Terraglyph {

/**
 * Showdown's horseshoe corral, ROOM_P30_Horseshoe_Corral (0x41DA10, vtable
 * 0x45ED00): Scooby and Rascal pitch horseshoes at three pegs, ten each.
 * Scooby's strength is where the player stops a sweeping meter; Rascal's
 * is random. Each of Scooby's ringers is a snack. After the last throws,
 * Chase B5. The room has no tick of its own: Scooby (0x41E290, vtable
 * 0x45ED40), Rascal (0x41D770, 0x45ECD8) and each horseshoe in the air
 * (0x41D210, 0x45ECB0) are sprites with updates of their own.
 */
class CorralRoom : public RoomClass, public Room::Actor {
public:
	explicit CorralRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                // 0x41DA80
	void roomLeft() override;                                                       // 0x41DCE0
	void claimPlacement(const Common::String &object, Room::Claim &claim) override; // 0x41DD00
	void actorUpdate(uint sprite) override;

	/** A horseshoe in the air (0x45ECB0). */
	struct Arc {
		int sprite = -1;
		bool rascal = false;     // +0x70
		int peg = 0;             // +0x6C: the peg it rings, 1-3, or 0
		int state = 0;           // +0x68
		int countdown = 0;       // +0x74
		Common::String anim;
		Common::String sound;
	};

	/** For a check: throw a horseshoe as 0x41D210 does, its arc's two digits @p digits (Scooby's; -1 to draw them). */
	void throwArc(bool rascal, int strength, int digits = -1);
	/** The snacks counter shown (0x41E1A0). */
	void showCounter();

	int getTurn() const { return _turn; }
	int getScooby() const { return _scooby; }
	int getMeter() const { return _meter; }
	int getScoobyState() const { return _scoobyState; }
	int getStrength() const { return _strength; }
	bool isScoobyDone() const { return _scoobyDone; }
	int getRascal() const { return _rascal; }
	int getRascalBody() const { return _rascalBody; }
	int getRascalState() const { return _rascalState; }
	int getRascalIdle() const { return _rascalIdle; }
	bool isRascalDone() const { return _rascalDone; }
	int getScoobyPile() const { return _scoobyPile; }
	int getRascalPile() const { return _rascalPile; }
	int getPeg(uint i) const { return _pegs[i]; }
	int getTens() const { return _tens; }
	int getOnes() const { return _ones; }
	int getMeterX() const { return _meterX; }
	int getMeterY() const { return _meterY; }
	const Common::Array<Arc> &getArcs() const { return _arcs; }
	const Common::String &getLastSound() const { return _lastSound; }

private:
	void playSound(const Common::String &name);
	/** 0x41E540: Scooby to @p state, with what that state starts. */
	void setScooby(int state);
	/** 0x41D920: Rascal to @p state, with what that state starts. */
	void setRascal(int state);
	/** 0x41E3A0, Scooby's update (vtable +0x0C). */
	void updateScooby();
	/** 0x41D860, Rascal's update (vtable +0x0C). */
	void updateRascal();
	/** 0x41D5B0, a horseshoe's update (vtable +0x0C). */
	void updateArc(Arc &arc);
	/** 0x41E140: a throw over, the other's turn, or Chase B5 after the last. */
	void turnOver();
	int meterTicks() const;

	int _turn = 0;           // +0xB8: 0 Scooby's throw, 1 Rascal's
	int _scoobyPile = -1;    // +0xC4
	int _rascalPile = -1;    // +0xC8
	int _pegs[3] = { -1, -1, -1 }; // +0xCC
	int _meterX = 0, _meterY = 0;  // +0xE0, +0xE4: where the meter's arrow is placed
	int _tens = -1, _ones = -1;    // +0xE8, +0xEC: the counter's digits
	int _numbers = 0;              // the counter's numbers claimed this load
	// Scooby (+0xBC): his sprite, then his fields.
	int _scooby = -1;
	int _scoobyState = 0;    // +0x5C
	int _meter = -1;         // +0x68
	int _strength = 0;       // +0x6C: the meter's step when stopped
	bool _scoobyDone = false; // +0x70: his horseshoe has landed
	bool _forward = true;    // +0x74: the meter's next sweep is backward when set
	// Rascal (+0xC0): his head's sprite, then his fields.
	int _rascal = -1;
	int _rascalBody = -1;    // +0x64
	int _rascalState = 0;    // +0x5C
	int _rascalIdle = 0;     // +0x68
	bool _rascalDone = false; // +0x6C
	Common::Array<Arc> _arcs;
	Common::String _lastSound;
};

/** The corral's room class the factory builds for @p room, or nullptr. */
RoomClass *createCorralRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_CORRAL_ROOMS_H
