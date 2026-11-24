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

#ifndef TERRAGLYPH_ACCUSATION_H
#define TERRAGLYPH_ACCUSATION_H

#include "terraglyph/rooms/class.h"

namespace Terraglyph {

/**
 * The suspects and clues (Showdown 0x426910, vtable 0x45F068; Phantom's is
 * the same code with its own suspects, movies and scenes): the clues found,
 * the suspects met, Scrappy on each, and, once the final clue is found, the
 * accusation. Its exit goes back where it was opened from.
 */
class SuspectChooserRoom : public ButtonRoom {
public:
	static const uint kClues = 5;

	explicit SuspectChooserRoom(TerraglyphEngine &engine) : ButtonRoom(engine) {}
	void enter(const Common::String &from) override;
	void tick() override;
	bool leaveBy(const Common::String &exitObject) override;
	bool isBusy() const override;

	/** The suspect (OBJ_SUSPECT_PICTURES' ANIM_SUSPECT_PICTURES1-5) a picture is, by title. */
	static const char *getSuspect(const Common::String &gameId, uint picture);
	/** The true villain's movie after an accusation (0x427C70), or -1. */
	static int getAccusationMovie(const Common::String &gameId, const Common::String &villain);
	/** The Big Head scene that reveals the villain (0x427DE0), or empty. */
	static Common::String getRevealScene(const Common::String &gameId, const Common::String &villain, bool right);

	const Common::String &getReturnRoom() const { return _from; }
	const Common::String &getIntroLine() const { return _intro; }
	uint getClueCount() const { return _clues; }
	int getClueButton(uint i) const { return _clueButtons[i]; }
	int getHotspotButton(uint i) const { return _hotspots[i]; }
	int getFinalHotspot() const { return _finalHotspot; }
	int getFinalButton() const { return _finalButton; }
	/** The back-exit button (OBJ_SUSPECT_CLUES_EXIT), or -1 once the final clue is found. */
	int getExitButton() const { return _exit; }
	int getInfoSprite() const { return _info; }
	int getIdle() const { return _idle; }
	int getPictureButton(uint picture) const { return picture < 5 ? _pictures[picture] : -1; }
	/** The last line Scrappy was asked for. */
	const Common::String &getLastLine() const { return _lastLine; }

protected:
	void onClick(int button) override;

private:
	void clickClue(int button);     // 0x4278C0
	void clickSuspect(uint picture); // 0x427600
	void accuse(const Common::String &suspect); // 0x427C70
	void verdict();                 // 0x427DE0
	void showInfo();                // 0x427AA0
	void say(const Common::String &line);

	Common::String _from;           // +0xE8
	Common::String _intro;          // +0xF4
	int _idle = 0;                  // +0xF0
	uint _clues = 0;                // +0xF8
	int _clueButtons[kClues];       // +0xB8: button indexes
	int _hotspots[kClues];          // +0xCC
	int _finalHotspot = -1;         // +0xE0
	int _finalButton = -1;          // +0xE4
	int _exit = -1;                 // the back-exit button (OBJ_SUSPECT_CLUES_EXIT)
	int _info = -1;                 // +0x100: a sprite
	int _pictures[5];               // button indexes, by picture
	bool _right = false;            // +0xFC
	Common::String _lastLine;
};

/**
 * Big Head (Showdown 0x403070, vtable 0x45E44C; the same code in both
 * titles): a scene played with the speakers' big heads, a mouth over a
 * body, on a background drawn for each speaker. The game gives it the scene,
 * the background and the room to go to after (game +0x220, +0x228, +0x230);
 * a line's lip sync can show the villain's clue pictures instead.
 */
class BigHeadRoom : public RoomClass {
public:
	explicit BigHeadRoom(TerraglyphEngine &engine) : RoomClass(engine) {}
	void enter(const Common::String &from) override;
	void tick() override;
	bool skip() override;
	bool isBusy() const override { return _playing; }

	const Common::String &getSpeaker() const { return _speaker; }
	const Common::String &getPartner() const { return _partner; }
	int getLine() const { return _line; }
	const Common::String &getLineClip() const { return _clip; }
	int getMouth() const { return _mouth; }
	int getBody() const { return _body; }
	int getPartnerMouth() const { return _partnerMouth; }
	int getPartnerBody() const { return _partnerBody; }
	int getBackground() const { return _background; }
	/** The background last drawn for a speaker (0x404270). */
	const Common::String &getBackgroundAnim() const { return _backgroundAnim; }
	bool isDone() const { return _done; }
	bool isStarted() const { return _started; }

private:
	void nextLine();                // vtable +0x38, 0x403940
	void setPartner();              // 0x403F90
	void showBackground();          // 0x404270
	void setHead(int sprite, const Common::String &who, const char *part, const char *anim);
	Common::String findSpeaker(int line) const;

	Common::String _from;           // +0xCC
	Common::String _speaker;        // +0xD4
	Common::String _partner;
	Common::String _clip;           // +0xC8: the line playing
	Common::String _backgroundAnim;
	int _line = 0;                  // +0x1C
	int _mouth = -1, _body = -1;    // +0xB8, +0xBC
	int _partnerMouth = -1, _partnerBody = -1; // +0xC0, +0xC4
	int _background = -1;           // game +0x58, layer 0
	bool _partnerShown = false;     // +0xDC
	bool _redraw = false;           // +0xDD: the background needs drawing
	bool _playing = false;
	bool _started = false;          // the opening has started (0x40B400)
	int _delay = 0;                 // +0x74: the opening's delay
	bool _done = false;             // +0x10
	bool _left = false;
};

} // End of namespace Terraglyph

#endif // TERRAGLYPH_ACCUSATION_H
