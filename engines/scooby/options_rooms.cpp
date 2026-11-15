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

// Options and Credits. The addresses are Showdown's; Phantom's classes are
// the same code (Options with its fields 4 bytes lower and its own sample
// sound), and Phantom's factory gives Credits its own scenes.

#include "audio/mixer.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/textconsole.h"

#include "scooby/detection.h"
#include "scooby/options_rooms.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/scooby.h"
#include "scooby/sound.h"

namespace Scooby {

// ---------------------------------------------------------------------------
// Options (0x423650 entry, 0x423B80 placements, 0x423C20 markers; the
// slider 0x4080F0, its update 0x4082A0; callbacks 0x423CB0, 0x423CF0,
// 0x423D50)

static const float kGainBase = 0.7f;        // 0x45EFB0
static const float kGainRange = 0.3f;       // 0x45EFB4
static const float kGainScale = 3.3333333f; // 0x45EFAC: 1 / 0.3
static const int kKnobDepth = 5000;

const char *OptionsRoom::getVolumeKey(uint slider) {
	// Scooby.ini's Scooby section has Music, Sfx and Voice (0x423A90);
	// here they are ScummVM's own volumes.
	static const char *const kKeys[] = { "music_volume", "sfx_volume", "speech_volume" };
	return kKeys[slider];
}

int OptionsRoom::toVolume(float value) {
	const float gain = value * kGainRange + kGainBase;
	const int v = (int)(gain * Audio::Mixer::kMaxMixerVolume + 0.5f);
	return CLIP<int>(v, 0, Audio::Mixer::kMaxMixerVolume);
}

float OptionsRoom::toValue(int volume) {
	// The original reads its own 0.7-1.0 gains; ScummVM's volume can be
	// lower, which would put the knob before the line's start.
	const float gain = (float)volume / Audio::Mixer::kMaxMixerVolume;
	const float t = (gain - kGainBase) * kGainScale;
	return CLIP<float>(t, 0.0f, 1.0f);
}

const char *OptionsRoom::getSample(uint slider) const {
	// The sound effects' slider plays a splat (Phantom: glass breaking), the
	// voices' Shaggy; the music's replays the room's music.
	if (slider == kSfx)
		return _engine.getGameId() == "showdown" ? "Pie_Noon.Splat.0" : "Knights.Glass_Breaks.3";
	if (slider == kVoice)
		return "Level_Select.SHAGGY.Spooky";
	return "";
}

void OptionsRoom::addMarker(float x, float y) {
	// 0x423C20 keeps the markers sorted by y: the new one goes before the
	// first that is lower down, and the rest move along.
	if (_markers > 5) {
		warning("More than 6 nails found in the options screen");
		return;
	}
	for (int i = 0; i < _markers; i++) {
		if (y < _markerY[i]) {
			SWAP(x, _markerX[i]);
			SWAP(y, _markerY[i]);
		}
	}
	_markerX[_markers] = x;
	_markerY[_markers] = y;
	_markers++;
}

void OptionsRoom::getEnds(uint slider, float &x1, float &y1, float &x2, float &y2) const {
	const Knob &k = _knobs[slider];
	x1 = k.x1;
	y1 = k.y1;
	x2 = k.x2;
	y2 = k.y2;
}

void OptionsRoom::enter(const Common::String &from) {
	// The placements claim the six markers (0x423B80) for their places
	// only; they aren't drawn.
	Room &room = *_engine.getRoom();
	_markers = 0;
	Common::String obj, anim;
	for (uint s = 0; s < room.getSpriteCount(); s++) {
		const Common::String &o = room.getObjectName(s);
		if (o != "OBJ_OPTIONS_NAIL" && o != "OBJ_OPTIONS_SWORDS")
			continue;
		if (obj.empty()) {
			obj = o;
			anim = room.getAnim(s);
		}
		addMarker(room.getX(s), room.getY(s));
		room.setHidden(s, true);
	}
	for (uint i = 0; i < kSliders; i++)
		_knobs[i] = Knob();
	if (_markers != 6) {
		warning("There must be 6 nails placed in the options screen");
		return;
	}

	// Each pair runs left to right; from the top, music, sound effects and
	// voices.
	for (uint i = 0; i < kSliders; i++) {
		float &ax = _markerX[2 * i], &bx = _markerX[2 * i + 1];
		if (bx < ax) {
			SWAP(ax, bx);
			SWAP(_markerY[2 * i], _markerY[2 * i + 1]);
		}
	}
	for (uint i = 0; i < kSliders; i++) {
		Knob &k = _knobs[i];
		k.x1 = _markerX[2 * i];
		k.y1 = _markerY[2 * i];
		k.x2 = _markerX[2 * i + 1];
		k.y2 = _markerY[2 * i + 1];
		k.value = toValue(ConfMan.getInt(getVolumeKey(i)));
		// 0x4080F0: the knob at the value along the line, and the line's
		// unit normal.
		k.x = (float)(((double)k.x2 - k.x1) * k.value + k.x1);
		k.y = (float)(((double)k.y2 - k.y1) * k.value + k.y1);
		const double len = sqrt(((double)k.x2 - k.x1) * ((double)k.x2 - k.x1) + ((double)k.y2 - k.y1) * ((double)k.y2 - k.y1));
		k.nx = (float)(((double)k.y1 - k.y2) / len);
		k.ny = (float)(((double)k.x2 - k.x1) / len);
		k.sprite = room.addEngineSprite(room.getLayerSprite(1), obj, anim, kKnobDepth, (int)k.x, (int)k.y, 1,
		                                Common::String::format("options knob %u", i));
	}
	debugC(1, kDebugScript, "options: knobs at %.3f %.3f %.3f", _knobs[0].value, _knobs[1].value, _knobs[2].value);
}

bool OptionsRoom::click(int sprite) {
	// 0x4082A0: the button going down on a knob grabs it, where it was hit.
	for (uint i = 0; i < kSliders; i++) {
		Knob &k = _knobs[i];
		if (sprite < 0 || k.sprite != sprite || k.dragging)
			continue;
		const Common::Point mouse = _engine.getMousePos();
		k.dragging = true;
		k.grabX = (float)(mouse.x - (double)k.x);
		k.grabY = (float)(mouse.y - (double)k.y);
		debugC(1, kDebugScript, "options: slider %u grabbed", i);
		return true;
	}
	return false;
}

void OptionsRoom::tick() {
	for (uint i = 0; i < kSliders; i++)
		update(i);
}

void OptionsRoom::update(uint slider) {
	// 0x4082A0: the mouse, less where the knob was grabbed, brought onto the
	// line; the knob goes there, held within its ends. The value is read
	// along the longer of the line's x and y.
	Knob &k = _knobs[slider];
	if (!k.dragging)
		return;
	const Common::Point mouse = _engine.getMousePos();
	const double px = mouse.x - (double)k.grabX;
	const double py = mouse.y - (double)k.grabY;
	const double d = (py - k.y) * k.ny + (px - k.x) * k.nx;
	const double dx = (double)k.x2 - k.x1;
	const double dy = (double)k.y2 - k.y1;
	float t;
	if (fabs(dx) <= fabs(dy))
		t = (float)(((py - d * k.ny) - k.y1) / dy);
	else
		t = (float)(((px - d * k.nx) - k.x1) / dx);
	if (t < 0.0f)
		t = 0.0f;
	else if (t > 1.0f)
		t = 1.0f;
	k.value = t;
	k.x = (float)(dx * t + k.x1);
	k.y = (float)(dy * t + k.y1);
	_engine.getRoom()->setOffset(k.sprite, (int)k.x, (int)k.y);
	changed(slider, false);
	if (!_engine.isMouseDown()) {
		k.dragging = false;
		changed(slider, true);
	}
}

void OptionsRoom::changed(uint slider, bool released) {
	// The music follows the knob as it moves (0x423CB0, for both); the
	// others change when it is let go, with a sample (0x423CF0, 0x423D50).
	if (slider != kMusic && !released)
		return;
	const int volume = toVolume(_knobs[slider].value);
	ConfMan.setInt(getVolumeKey(slider), volume);
	_engine.syncSoundSettings();
	if (!released)
		return;
	// The original writes Scooby.ini as the room is left (+0x10, 0x423A90).
	ConfMan.flushToDisk();
	debugC(1, kDebugScript, "options: slider %u let go at %.3f, volume %d", slider, _knobs[slider].value, volume);
	const char *sample = getSample(slider);
	if (*sample) {
		Sound &sound = *_engine.getSound();
		sound.stopClip(sample);
		sound.play(sample);
	}
}

// ---------------------------------------------------------------------------
// Credits (0x4218C0 entry, 0x421C60 tick, 0x422230 +0x10)

static const int kEndMovie = 58;      // 0x3A
static const int kTextStart = 480;    // 0x43F00000
static const int kTextExtra = 350;    // 0x15E
static const int kScroll = 6;         // 0x45EEA8, every third tick

int CreditsRoom::getVillainMovie(const Common::String &villain) {
	// Both titles' villains are in the one table.
	static const struct {
		const char *name;
		int movie;
	} kMovies[] = {
		{ "ARTIE", 0x21 }, { "GUMMY", 0x22 }, { "ICDOUBLE", 0x23 }, { "JENNY", 0x24 }, { "SHERIFF", 0x25 },
		{ "BRAD", 0x2D }, { "JESTER", 0x2E }, { "EWAN", 0x2F }, { "MARIA", 0x30 }, { "WIZARD", 0x31 }
	};
	for (uint i = 0; i < ARRAYSIZE(kMovies); i++) {
		if (villain == kMovies[i].name)
			return kMovies[i].movie;
	}
	return -1;
}

const Common::String &CreditsRoom::getScene(uint stage) const {
	// The factory's scenes (Showdown 0x42EF50, Phantom 0x42FE49): the room's
	// entry scene, then the WB's and the TGI's (+0xC8, +0xD0).
	static const Common::String kShowdown[] = { "Credits.1100A2_", "Credits.1100D3_", "Credits.1100D4_" };
	static const Common::String kPhantom[] = { "Credits.10002_", "Credits.1000A_", "Credits.1000B_" };
	return _engine.getGameId() == "showdown" ? kShowdown[stage] : kPhantom[stage];
}

void CreditsRoom::enter(const Common::String &from) {
	_rolling = _left = _skipped = false;
	if (from == "ROOM_Big_Head") {
		// After the accusation: the villain's movie, then this room again,
		// without the base entry. No movie for an unknown villain.
		const Common::String villain = _engine.getGameString("Whodunit");
		const int movie = getVillainMovie(villain);
		debugC(1, kDebugScript, "credits: %s's movie %d", villain.c_str(), movie);
		if (movie >= 0)
			_engine.requestRoom("ROOM_Credits", movie);
		return;
	}
	if (from == "ROOM_Credits" && !_moviePlayed) {
		_moviePlayed = true;
		debugC(1, kDebugScript, "credits: movie %d", kEndMovie);
		_engine.requestRoom("ROOM_Credits", kEndMovie);
		return;
	}
	// The roll: input held, the cursor hidden, the toolbar hidden (by the
	// table), TLC first; the room's entry scene.
	_rolling = true;
	_logo = _text = -1;
	_stage = 0;
	show("OBJ_CREDITS_TLC", "ANIM_CREDITS_TLCLOGO", "ANIM_CREDITS_TLCTEXT");
	_engine.getScript()->startScene(getScene(0), false);
	debugC(1, kDebugScript, "credits: rolling, %d to scroll", _remaining);
}

void CreditsRoom::show(const char *object, const char *logo, const char *text) {
	// The logo at (0, 0) under the second layer (game +0x5C) at depth 1; the
	// text under the first (+0x58) at depth 0, from the screen's bottom. It
	// scrolls for its first cel's height and 350 more.
	Room &room = *_engine.getRoom();
	if (_text >= 0)
		room.setHidden(_text, true);
	if (_logo >= 0)
		room.setHidden(_logo, true);
	_logo = room.addEngineSprite(room.getLayerSprite(1), object, logo, 1, 0, 0, 1, "credits logo");
	_text = room.addEngineSprite(room.getLayerSprite(0), object, text, 0, 0, kTextStart, 1, "credits text");
	_textY = kTextStart;
	int w, h;
	room.getFirstCelSize(_text, w, h);
	_remaining = h + kTextExtra;
}

void CreditsRoom::tick() {
	if (!_rolling || _left)
		return;
	// Every third tick the text rises 6 pixels.
	if (++_ticks == 3) {
		_ticks = 0;
		_textY -= kScroll;
		_engine.getRoom()->setOffset(_text, 0, (int)_textY);
		_remaining -= kScroll;
	}
	if (_skipped) {
		debugC(1, kDebugScript, "credits: skipped");
		leave();
		return;
	}
	if (_remaining < 1) {
		const int stage = _stage++;
		debugC(1, kDebugScript, "credits: part %d done", stage);
		if (stage == 0) {
			show("OBJ_CREDITS_WB", "ANIM_CREDITS_WBLOGO", "ANIM_CREDITS_WBTEXT");
			_engine.getScript()->startScene(getScene(1), false);
		} else if (stage == 1) {
			show("OBJ_CREDITS_TGI", "ANIM_CREDITS_TGILOGO", "ANIM_CREDITS_TGITEXT");
			_engine.getScript()->startScene(getScene(2), false);
		} else if (stage == 2) {
			leave();
		}
	}
}

bool CreditsRoom::skip() {
	// Input slots 5 and 4 go to Sign In.
	if (!_rolling || _left)
		return false;
	_skipped = true;
	return true;
}

void CreditsRoom::leave() {
	// Leaving after movie 58 (game +0x90) ends the game (0x422230): the
	// player's profile is deleted, a new game starts (+0x4C, 0x435780,
	// +0x50), and every movie counts as seen (0x40B0D0). The original does
	// this as the room is left; here just before.
	_left = true;
	if (_engine.getLastMovie() == kEndMovie) {
		debugC(1, kDebugScript, "credits: the game is over for %s", _engine.getPlayerName().c_str());
		if (_engine.getPlayerSlot() >= 0)
			_engine.deletePlayer(_engine.getPlayerSlot());
		_engine.newGame();
		_engine.markAllMoviesSeen();
	}
	_engine.exitTo("ROOM_Sign_In");
}

} // End of namespace Scooby
