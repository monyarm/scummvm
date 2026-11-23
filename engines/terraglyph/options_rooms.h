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

#ifndef TERRAGLYPH_OPTIONS_ROOMS_H
#define TERRAGLYPH_OPTIONS_ROOMS_H

#include "terraglyph/room_class.h"

namespace Terraglyph {

/**
 * The options room (Showdown 0x4235D0, vtable 0x45EF6C; Phantom's is the
 * same but for its sample sound): three volume sliders, each a knob
 * dragged along the line between two of the six markers placed in the room
 * (OBJ_OPTIONS_NAIL, Phantom OBJ_OPTIONS_SWORDS). From the top: music,
 * sound effects, voices.
 */
class OptionsRoom : public RoomClass {
public:
	enum Slider { kMusic, kSfx, kVoice, kSliders };

	explicit OptionsRoom(TerraglyphEngine &engine) : RoomClass(engine) {}
	void enter(const Common::String &from) override;
	void tick() override;
	bool click(int sprite) override;

	int getMarkerCount() const { return _markers; }
	/** A slider's ends, left then right: the markers sorted by y, each pair by x. */
	void getEnds(uint slider, float &x1, float &y1, float &x2, float &y2) const;
	int getKnob(uint slider) const { return _knobs[slider].sprite; }
	/** Where a knob is along its slider, 0 to 1, and where it is drawn. */
	float getValue(uint slider) const { return _knobs[slider].value; }
	float getKnobX(uint slider) const { return _knobs[slider].x; }
	float getKnobY(uint slider) const { return _knobs[slider].y; }
	bool isDragging(uint slider) const { return _knobs[slider].dragging; }
	/** The sound a slider's release plays, or empty. */
	const char *getSample(uint slider) const;

	/** ScummVM's volume (0-256) for a slider position: the original's gain 0.7 + 0.3 t. */
	static int toVolume(float value);
	/** A slider position for a ScummVM volume, (gain - 0.7) / 0.3, kept within 0-1. */
	static float toValue(int volume);
	static const char *getVolumeKey(uint slider);

private:
	// The slider (0x4080F0, vtable 0x45E568).
	struct Knob {
		int sprite = -1;
		float x1 = 0, y1 = 0, x2 = 0, y2 = 0; // +0x70-+0x7C
		float nx = 0, ny = 0;                 // +0x80, +0x84: the unit normal
		float x = 0, y = 0;                   // +0x0C, +0x10
		float grabX = 0, grabY = 0;           // +0x88, +0x8C
		float value = 0;
		bool dragging = false;                // +0x6C
	};

	void addMarker(float x, float y);     // 0x423C20
	void update(uint slider);             // 0x4082A0
	void changed(uint slider, bool released);

	float _markerX[6], _markerY[6];       // +0xC0, +0xD8
	int _markers = 0;                     // +0xBC
	Knob _knobs[kSliders];                // +0xF4, +0xF0, +0xF8
};

/**
 * The credits (0x421740, vtable 0x45EE68; the same code in both titles):
 * after the accusation, the villain's movie and movie 58, then the three
 * companies' logos and scrolling text while the gang talk, then Sign In.
 * Leaving after movie 58 ends the game: the player's profile goes, and a
 * new game starts with every movie seen.
 */
class CreditsRoom : public RoomClass {
public:
	explicit CreditsRoom(TerraglyphEngine &engine) : RoomClass(engine) {}
	void enter(const Common::String &from) override;
	void tick() override;
	bool skip() override;
	void reset() override { _moviePlayed = false; }
	/** Input is held and the cursor hidden for the whole roll. */
	bool isBusy() const override { return _rolling; }

	/** The villain's movie (0x21-0x31), or -1 for none. */
	static int getVillainMovie(const Common::String &villain);
	int getStage() const { return _stage; }
	int getRemaining() const { return _remaining; }
	int getLogo() const { return _logo; }
	int getText() const { return _text; }
	float getTextY() const { return _textY; }
	const Common::String &getScene(uint stage) const;

private:
	void show(const char *object, const char *logo, const char *text);
	void leave();                          // the exit, with +0x10 (0x422230)

	bool _moviePlayed = false;  // +0xD8
	bool _rolling = false;
	bool _left = false;
	bool _skipped = false;
	int _logo = -1, _text = -1; // +0xB8, +0xBC
	float _textY = 0;
	int _remaining = 0;         // +0xC0
	int _stage = 0;             // +0xC4
	uint _ticks = 0;            // 0x475F0C, a global: every third tick
};

} // End of namespace Terraglyph

#endif // TERRAGLYPH_OPTIONS_ROOMS_H
