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

#ifndef SCOOBY_BELOW_BALCONY_ROOMS_H
#define SCOOBY_BELOW_BALCONY_ROOMS_H

#include "scooby/doors_rooms.h" // JesterControl
#include "scooby/room_class.h"

namespace Scooby {

/**
 * Phantom's below-the-balcony and its door (WP-076), the last of the audit's
 * group-7 (maze) rooms. Reached from the maze map's BALCONY marker (WP-074)
 * and the ghost-spot skip. The addresses are Phantom's; Showdown has neither
 * room.
 */

/**
 * ROOM_P27_Below_Balcony (class 0x40F220, vtable 0x45E750): the pocket of the
 * maze below the balcony. The vines over the way to P26 cycle their lines on a
 * plain click and warn when the rusty (unsharpened) shears are tried; the sharp
 * shears cut them (movie 5), which opens the way on; and clicking the vines
 * enough reveals the ball below the balcony, which animates into view with
 * SFX_P27_BELOW_BALCONY_BALL and a line (100A_). Music "1", no opening.
 */
class BelowBalconyRoom : public RoomClass {
public:
	explicit BelowBalconyRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void tick() override;                                                            // 0x40F5A0
	void event(const Common::String &object, bool success) override;                 // 0x40F2B0
	void reset() override;                                                           // 0x40F280
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x40F380
	void saveKeys(Common::INIFile &profile) override;                                // 0x40F6E0
	void loadKeys(const Common::INIFile &profile) override;                          // 0x40F760
	bool isBusy() const override { return _ballScene; } // input held during the reveal (game +0x09)

	static const char *kWrongTool;  // Below_Balcony.260F_ (the rusty shears tried on the vine)
	static const char *kBallLine;   // Below_Balcony.100A_ (the ball revealed)
	static const char *kBallSfx;    // SFX_P27_BELOW_BALCONY_BALL
	static const char *kVineReveal; // Below_Balcony.260_ (the click that reveals the ball)
	static const char *kVineLine;   // Below_Balcony.260D_ (a plain vine click)
	static const char *kRustyCursor; // ANIM_CURSORRUSTY_SHEARS

	// For the check: the saved state (the room's own keys).
	bool showBallPending() const { return _showBall; }
	int getVineClicks() const { return _vineClicks; }
	bool playBallScene() const { return _playBallScene; }

private:
	int findSprite(const Common::String &object) const;
	void startReveal(const Common::String &scene, bool *doneFlag);

	bool _showBall = false;     // +0xBC, saved ShowBall: the reveal is queued
	int _ball = -1;             // +0xC0: the ball sprite
	bool _playBallScene = true; // +0xC4, saved PlayBallScene: the reveal is still to come
	bool _ballScene = false;    // +0xC5: the reveal is playing
	int _vineClicks = 0;        // +0xC8, saved VineClicks: the plain-click cycle

	// The 260_ scene's done flag, resolved as it ends (the original's 0x40B400
	// done-flag; it sets _showBall when the reveal line finishes).
	Common::String _scene;
	bool *_doneFlag = nullptr;
};

/**
 * ROOM_P27A_Below_Balcony_Door (class 0x4106B0, vtable 0x45E850): the door back
 * out of the balcony pocket, where the blue ball is found. The shared Jester
 * (as the room of doors and the royal gallery keep him) takes the blue ball
 * once found; the blue ball clicked plays movie 2 the first time (67 once the
 * yellow ball is found), is marked found, and queues a return to the royal gallery
 * (movie 54 from the gallery, 57 from elsewhere) unless the full bottle has
 * been used. Exits to ROOM_P07_Royal_Gallery. Music "Princess_Bedroom.Music".
 */
class BelowBalconyDoorRoom : public RoomClass {
public:
	explicit BelowBalconyDoorRoom(ScoobyEngine &engine) : RoomClass(engine) {}

	void tick() override;                                                            // 0x4107D0
	void enter(const Common::String &from) override;                                 // 0x4107F0
	void reset() override;                                                           // 0x410740
	void event(const Common::String &object, bool success) override;                 // 0x410760
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x4108C0
	bool isBusy() const override { return _jester.isHolding(); }
	bool click(int sprite) override { _jester.click(sprite); return false; }

	enum {
		kMovieBlueFirst = 2,       // the blue ball, first time (YellowBallFound clear)
		kMovieBlueAgain = 0x43,    // 67: the blue ball, once the yellow is found
		kMovieReturnGallery = 0x36, // 54: the queued return, came from the gallery
		kMovieReturnOther = 0x39    // 57: the queued return, from elsewhere
	};
	static const char *kGallery; // ROOM_P07_Royal_Gallery

	// For the check.
	bool returnPending() const { return _returnPending; }
	JesterControl &getJester() { return _jester; }

private:
	JesterControl _jester;       // +0xB8: the Jester, as the gallery and doors keep him
	bool _returnPending = false; // +0xCC: a return to the gallery is queued
};

/** The below-the-balcony room class for @p room, or nullptr. */
RoomClass *createBelowBalconyRoom(ScoobyEngine &engine, const Common::String &room);

} // End of namespace Scooby

#endif // SCOOBY_BELOW_BALCONY_ROOMS_H
