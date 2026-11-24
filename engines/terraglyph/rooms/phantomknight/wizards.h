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

#ifndef TERRAGLYPH_WIZARDS_H
#define TERRAGLYPH_WIZARDS_H

#include "terraglyph/rooms/class.h"

namespace Terraglyph {

/**
 * Phantom's wizard's skull and library (WP-077), the first of the audit's
 * group-8 (Upstairs) rooms. Reached from the magic mirror room (WP-072 builds
 * OBJ_P08_TO_WIZARDS_SKULL). The addresses are Phantom's; Showdown has neither
 * room. The source asserts name C:\Scooby\FBF\Wizards_Skull_Room.cpp.
 */

/**
 * ROOM_Wizards_Skull (class 0x42D7B0, vtable 0x45F4C4): the Skull Puzzle room.
 * Four skulls (OBJ_SKULL1..4) are clickables that rotate through four faces
 * (ANIM_SKULL{n}A..D); the gang turns each to the per-game SkullCombo{n} and,
 * once all four match, the way to the wizard's bedroom opens (movie 6 to
 * ROOM_P24_Wizards_Bedroom) and SkullPuzzle is set. The exit OBJ_SKULL_EXIT
 * leads back to the magic mirror room. Entered from the mirror room the
 * wizard's hand demonstrates the combination (OBJ_SKULL_HAND, the HANDLIFT /
 * HANDTURN / HANDRETURN loop), sets SkullMovie, then returns to the mirror room
 * (movie 31); entered from anywhere else the gang attempts the puzzle itself
 * (SkullPuzzleAttempted; the first time the wizard's 300D_ intro plays). Music
 * "Skull_Puzzle.Music", no opening.
 */
class WizardsSkullRoom : public RoomClass {
public:
	explicit WizardsSkullRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void tick() override;                                                            // 0x42E100
	void enter(const Common::String &from) override;                                 // 0x42D910
	void roomLeft() override;                                                        // 0x42DBD0
	void reset() override;                                                           // 0x42D8B0
	void event(const Common::String &object, bool success) override;                 // 0x42E060
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x42DBF0
	void saveKeys(Common::INIFile &profile) override;                                // 0x42E120
	void loadKeys(const Common::INIFile &profile) override;                          // 0x42E1D0
	bool isBusy() const override { return _handMode != 0; } // input held during the hand demo (game +0x24)

	static const char *kIntro;      // Wizards_Library.300D_  (first attempt, from the mirror room)
	static const char *kDemo;       // Magic_Mirror_Room.Wizard_  (the hand demo)
	static const char *kSlide;      // Wizards_Skull.Skull_Slide  (sfx on a skull click)
	static const char *kHandLift;   // ANIM_SKULL_HANDLIFT
	static const char *kHandTurn;   // ANIM_SKULL_HANDTURN
	static const char *kHandReturn; // ANIM_SKULL_HANDRETURN

	// For the check.
	int getState(int i) const { return _state[i]; }
	bool introDone() const { return _introDone; }
	int handMode() const { return _handMode; }

private:
	int findSprite(const Common::String &object) const;

	int _skull[4] = { -1, -1, -1, -1 }; // +0xB8/BC/C0/C4: the four skull clickables
	int _state[4] = { 0, 0, 0, 0 };     // +0xC8/CC/D0/D4, saved Skull1..4: the rotation (0..3)
	bool _introDone = false;            // +0x114, saved IntroDone
	bool _recheck = false;              // +0xF4: a skull was slid, re-check the combo

	// The wizard-hand demonstration (0x42E3F0), run when entered from off the
	// mirror room: an easing state machine moving OBJ_SKULL_HAND at 5 px/tick.
	int _handMode = 0;   // +0xE8: 0 idle; 1 approach; 2 lift; 3 turn; 4 return; 5 settle; 6 done
	int _handStep = 0;   // +0xE4: which skull the hand is at (0..4)
	int _turnCount = 0;  // +0xF0: turns done on the current skull
	int _hand = -1;      // +0xD8: the hand sprite
	float _handX = 0;    // the hand's position, eased toward the target each tick
	float _handY = 0;
	int _handRelX = 0;   // +0xDC/+0xE0: the hand's rest offset from a skull
	int _handRelY = 0;
};

/**
 * ROOM_P25_Wizards_Library (class 0x42D1F0, vtable 0x45F484): the library whose
 * shelf (OBJ_P25_WIZARDS_LIBRARY_SHELF) takes the spell book (from the
 * Princess's bedroom). Used on the shelf the first time it reveals the skull
 * combination on Big Head (BIG_HEAD_BG_SKULLS, Wizards_Library.300C_), then
 * routes to the skull room (movie 61); after SkullMovie it goes straight there.
 * Once the puzzle is solved (SkullPuzzle) the shelf gives way to a settled
 * underlay and the way to the wizard's bedroom (OBJ_P25_TO_P24) opens. Names
 * OBJ_INV_SPELL_BOOK, ROOM_Big_Head and ROOM_Wizards_Skull. Music "1", opening
 * Wizards_Library.300_.
 */
class WizardsLibraryRoom : public RoomClass {
public:
	explicit WizardsLibraryRoom(TerraglyphEngine &engine) : RoomClass(engine) {}

	void enter(const Common::String &from) override;                                 // 0x42D580
	void reset() override;                                                           // 0x42D250
	void event(const Common::String &object, bool success) override;                 // 0x42D270
	void claimPlacement(const Common::String &object, Room::Claim &claim) override;  // 0x42D600

	static const char *kRevealVoice;  // Wizards_Library.300C_
	static const char *kRevealBg;     // BIG_HEAD_BG_SKULLS
	static const char *kTaunt[2];     // Wizards_Library.300C1_ / 300C2_
	static const char *kShelfHint[3]; // Wizards_Library.300F_ / 300F1_ / 300F2_
	static const char *kHotspot1;     // Wizards_Library.HOTSPOT1_
	static const char *kHotspot3;     // Wizards_Library.HOTSPOT3_
	static const char *kSkullRoom;    // ROOM_Wizards_Skull

	// For the check.
	int getHintCycle() const { return _hint; }

private:
	void revealOrEnterSkull(int movie);

	int _hint = 0; // +0xBC: the shelf-hint cycle (300F_ / 300F1_ / 300F2_)
};

/** The wizard-rooms class for @p room, or nullptr. */
RoomClass *createWizardsRoom(TerraglyphEngine &engine, const Common::String &room);

} // End of namespace Terraglyph

#endif // TERRAGLYPH_WIZARDS_H
