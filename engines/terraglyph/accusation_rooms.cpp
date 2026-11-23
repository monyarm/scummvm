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

// The accusation: the suspects and clues, and Big Head. The addresses are
// Showdown's; Phantom's Big Head is the same code, and its suspects and
// clues the same but for its suspects, movies and scenes.

#include "common/config-manager.h"
#include "common/debug.h"

#include "terraglyph/accusation_rooms.h"
#include "terraglyph/detection.h"
#include "terraglyph/inventory.h"
#include "terraglyph/room.h"
#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

static bool isShowdown(const TerraglyphEngine &engine) {
	return engine.getGameId() == "showdown";
}

// ---------------------------------------------------------------------------
// The suspects and clues (0x426B40 entry, 0x4269E0 tick, 0x427100
// placements, 0x426F30 exit)

static const int kIdleTicks = 360;   // 0x168
static const char *const kCluePrefix = "ANIM_SUSPECT_CLUES";
static const char *const kHotspotPrefix = "ANIM_SUSPECT_CLUES_HOTSPOT";
static const char *const kOpenCursor = "ANIM_CURSOROPEN";
static const char *const kScrappyClue = "Global.Scrappy.Clue.";
// The clues' places, by slot (0x426B40).
static const int kCluePlaces[SuspectChooserRoom::kClues][2] = { { 90, 283 }, { 184, 286 }, { 286, 283 }, { 372, 280 }, { 473, 280 } };

const char *SuspectChooserRoom::getSuspect(const Common::String &gameId, uint picture) {
	static const char *const kShowdown[] = { "SHERIFF", "JENNY", "ICDOUBLE", "ARTIE", "GUMMY" };
	static const char *const kPhantom[] = { "EWAN", "BRAD", "MARIA", "WIZARD", "JESTER" };
	if (picture >= 5)
		return "";
	return gameId == "showdown" ? kShowdown[picture] : kPhantom[picture];
}

int SuspectChooserRoom::getAccusationMovie(const Common::String &gameId, const Common::String &villain) {
	// 0x427C70: the true villain's movie, whoever was accused.
	static const struct {
		const char *name;
		int movie;
	} kMovies[] = {
		{ "SHERIFF", 13 }, { "JENNY", 12 }, { "ICDOUBLE", 11 }, { "ARTIE", 9 }, { "GUMMY", 10 },
		{ "EWAN", 15 }, { "BRAD", 13 }, { "MARIA", 16 }, { "WIZARD", 17 }, { "JESTER", 14 }
	};
	const uint first = gameId == "showdown" ? 0 : 5;
	for (uint i = first; i < first + 5; i++) {
		if (villain == kMovies[i].name)
			return kMovies[i].movie;
	}
	return -1;
}

Common::String SuspectChooserRoom::getRevealScene(const Common::String &gameId, const Common::String &villain, bool right) {
	// 0x427DE0: 720 for the right suspect, 730 for the wrong; a letter for
	// the villain.
	static const struct {
		const char *name;
		char letter;
	} kLetters[] = {
		{ "SHERIFF", 'B' }, { "JENNY", 'C' }, { "ICDOUBLE", 'E' }, { "ARTIE", 'A' }, { "GUMMY", 'D' },
		{ "EWAN", 'A' }, { "BRAD", 'B' }, { "MARIA", 'C' }, { "WIZARD", 'E' }, { "JESTER", 'D' }
	};
	const bool showdown = gameId == "showdown";
	const uint first = showdown ? 0 : 5;
	for (uint i = first; i < first + 5; i++) {
		if (villain == kLetters[i].name)
			return Common::String::format("%s.%s%c_", showdown ? "Street_1" : "Treasury", right ? "720" : "730", kLetters[i].letter);
	}
	return Common::String();
}

bool SuspectChooserRoom::isBusy() const {
	// A new clue (game +0x109) holds input until Scrappy has named it.
	return _engine.hasNewClue();
}

void SuspectChooserRoom::say(const Common::String &line) {
	_lastLine = line;
	_engine.getSound()->play(line);
	debugC(1, kDebugScript, "suspects: %s", line.c_str());
}

void SuspectChooserRoom::enter(const Common::String &from) {
	// The room was built again: the buttons of the last visit are gone.
	clearButtons();
	_clues = 0;
	for (uint i = 0; i < kClues; i++)
		_clueButtons[i] = _hotspots[i] = -1;
	for (uint i = 0; i < 5; i++)
		_pictures[i] = -1;
	_finalHotspot = _finalButton = _info = _exit = -1;
	_intro.clear();
	// Back from the accusation's movie: the verdict.
	if (from == "ROOM_Suspect_Chooser") {
		verdict();
		return;
	}
	Room &room = *_engine.getRoom();
	const bool final = _engine.getGameFlag("FinalClue");
	if (from != "ROOM_Quit")
		_from = from;
	_idle = kIdleTicks;
	_intro = final ? "Suspect_and_Clues.Solve_Game" : "Suspect_and_Clues.SHAGGY_Introduce_Screen";
	say(_intro);

	// The placements (0x427100): the met suspects' pictures as buttons; the
	// clue info, hidden; the final clue's button once it is found; the exit
	// gone once it is. The rest it claims are built below, or not at all.
	const uint placed = room.getSpriteCount();
	for (uint s = 0; s < placed; s++) {
		if (room.isEngineSprite(s))
			continue;
		const Common::String obj = room.getObjectName(s);
		const Common::String anim = room.getAnim(s);
		if (obj == "OBJ_SUSPECT_PICTURES") {
			uint picture = 5;
			if (anim.size() == strlen("ANIM_SUSPECT_PICTURES") + 1 && anim.hasPrefix("ANIM_SUSPECT_PICTURES"))
				picture = anim.lastChar() - '1';
			const char *suspect = getSuspect(_engine.getGameId(), picture);
			if (*suspect && _engine.getInventory()->hasMetSuspect(suspect)) {
				_pictures[picture] = addButton(s);
				room.setDepth(s, -1);
				room.setRollover(s, kOpenCursor);
			} else {
				room.setHidden(s, true);
			}
		} else if (obj == "OBJ_SUSPECT_CLUES" || obj == "OBJ_SUSPECT_CLUES_HOTSPOT") {
			room.setHidden(s, true);
		} else if (obj == "OBJ_SUSPECT_CLUE_INFO") {
			room.setHidden(s, true);
			_info = room.addEngineSprite(room.getLayerSprite(0), obj, anim, 0, room.getX(s), room.getY(s), 1, "clue info");
			room.setHidden(_info, true);
		} else if (obj == "OBJ_SUSPECT_FINAL_CLUE") {
			if (final) {
				_finalButton = addButton(s);
				room.setRollover(s, kOpenCursor);
			} else {
				room.setHidden(s, true);
			}
		} else if (anim.size() >= 8 && anim.hasSuffix("EXITICON")) {
			// The way back (0x427100): gone once the final clue is found; until
			// then a button whose click returns the gang where the board was
			// opened from (leaveBy, below). The port gave it art and the exit
			// cursor but never wired the click, so opening the board from any
			// room's magnifier trapped the player (the WP-079 soft-lock).
			if (final)
				room.setHidden(s, true);
			else
				_exit = addButton(s);
		}
	}

	// The clues found, each a button at its slot's place and a hotspot
	// numbered by how many came before it.
	for (uint i = 0; i < kClues; i++) {
		const Common::String clue = _engine.getGameString(Common::String::format("CluesFound%u", i));
		if (clue.empty())
			continue;
		const int b = room.addEngineSprite(-1, "OBJ_SUSPECT_CLUES", kCluePrefix + clue, 0, kCluePlaces[i][0], kCluePlaces[i][1], 1,
		                                   Common::String::format("clue %u", i));
		room.setRollover(b, kOpenCursor);
		_clueButtons[_clues] = addButton(b);
		const int h = room.addEngineSprite(-1, "OBJ_SUSPECT_CLUES_HOTSPOT", Common::String::format("%s%c", kHotspotPrefix, '1' + _clues), 0,
		                                   0, 0, 2, Common::String::format("clue %u hotspot", i));
		room.setRollover(h, kOpenCursor);
		_hotspots[_clues] = addButton(h);
		room.setHitType(h, 2);
		_clues++;
	}
	if (final) {
		const int h = room.addEngineSprite(-1, "OBJ_SUSPECT_CLUES_HOTSPOT", Common::String::format("%s%c", kHotspotPrefix, '6'), 0, 0, 0, 2,
		                                   "final clue hotspot");
		room.setRollover(h, kOpenCursor);
		_finalHotspot = addButton(h);
		room.setHitType(h, 2);
	}
	debugC(1, kDebugScript, "suspects: %u clues%s, back to %s", _clues, final ? ", the final one" : "", _from.c_str());
}

void SuspectChooserRoom::tick() {
	ButtonRoom::tick();
	Sound &sound = *_engine.getSound();
	// Once the introduction is over, Scrappy names the newest clue.
	if (_engine.hasNewClue() && !sound.isClipPlaying(_intro)) {
		if (_clues)
			say(kScrappyClue + _engine.getGameString(Common::String::format("CluesFound%u", _clues - 1)));
		_engine.setNewClue(false);
	}
	// Every 10 seconds without a click, Shaggy points at the way back, until
	// the final clue is found.
	if (_idle-- == 0 && !_engine.getGameFlag("FinalClue")) {
		_idle = kIdleTicks;
		say("Suspect_and_Clues.SHAGGY.Click_On_Back_Button");
	}
	showInfo();
}

void SuspectChooserRoom::showInfo() {
	// 0x427AA0: a clue under the mouse, or its hotspot, shows its card.
	Room &room = *_engine.getRoom();
	Common::String info;
	for (uint i = 0; i < _clues; i++) {
		if (_buttons[_clueButtons[i]].state || _buttons[_hotspots[i]].state) {
			const Common::String &anim = room.getAnim(_buttons[_clueButtons[i]].sprite);
			info = "ANIM_SUSPECT_CLUE_INFO" + Common::String(anim.c_str() + strlen(kCluePrefix));
			break;
		}
	}
	if (_finalButton >= 0 && (_buttons[_finalButton].state || (_finalHotspot >= 0 && _buttons[_finalHotspot].state)))
		info = "ANIM_SUSPECT_CLUE_INFOFINAL_CLUE";
	if (_info < 0)
		return;
	if (info.empty()) {
		room.setHidden(_info, true);
		return;
	}
	room.setHidden(_info, false);
	if (room.getAnim(_info) != info)
		room.startAnim(_info, info, 1, 0);
}

void SuspectChooserRoom::onClick(int button) {
	// The way back: the exit's click returns the gang where the board was
	// opened from (0x426F30). It is a button so the click reaches here at all
	// — its exit object carries no fixed destination (the return room is the
	// dynamic _from), so the engine's generic exit dispatch never fires on it.
	if (button == _exit) {
		leaveBy("OBJ_SUSPECT_CLUES_EXIT");
		return;
	}
	for (uint i = 0; i < 5; i++) {
		if (button == _pictures[i]) {
			clickSuspect(i);
			return;
		}
	}
	clickClue(button);
}

void SuspectChooserRoom::clickClue(int button) {
	// 0x4278C0: Scrappy on the clue: the final clue's button, a hotspot's
	// clue, or the clue clicked.
	Room &room = *_engine.getRoom();
	Common::String name;
	if (button == _finalButton) {
		name = "FINAL_CLUE";
	} else {
		Common::String anim = room.getAnim(_buttons[button].sprite);
		if (anim.hasPrefix(kHotspotPrefix) && anim.size() == strlen(kHotspotPrefix) + 1) {
			const int i = anim.lastChar() - '1';
			if (i == 5)
				anim = "ANIM_SUSPECT_CLUESFINAL_CLUE";
			else if (i >= 0 && i < (int)_clues)
				anim = room.getAnim(_buttons[_clueButtons[i]].sprite);
		}
		name = anim.size() > strlen(kCluePrefix) ? Common::String(anim.c_str() + strlen(kCluePrefix)) : Common::String();
	}
	_engine.getSound()->stop();
	_idle = kIdleTicks;
	say(kScrappyClue + name);
}

void SuspectChooserRoom::clickSuspect(uint picture) {
	// 0x427600: Scrappy on the suspect, or, with the final clue, the
	// accusation.
	_engine.getSound()->stop();
	if (!_engine.getGameFlag("FinalClue")) {
		static const char *const kShowdown[] = { "Chuck", "Jenny", "ICDouble", "Artie", "Gummy" };
		static const char *const kPhantom[] = { "Ewan", "Brad", "Maria", "Wizard", "Chester" };
		say(Common::String::format("Suspect_and_Clues.SCRAPPY_%s_Info", isShowdown(_engine) ? kShowdown[picture] : kPhantom[picture]));
	} else {
		accuse(getSuspect(_engine.getGameId(), picture));
	}
	_idle = kIdleTicks;
}

void SuspectChooserRoom::accuse(const Common::String &suspect) {
	// 0x427C70: right or wrong, the true villain's movie, then this room
	// again for the verdict.
	const Common::String villain = _engine.getGameString("Whodunit");
	_right = suspect == villain;
	const int movie = getAccusationMovie(_engine.getGameId(), villain);
	debugC(1, kDebugScript, "suspects: %s accused, %s; movie %d", suspect.c_str(), _right ? "right" : "wrong", movie);
	if (movie >= 0)
		_engine.requestRoom("ROOM_Suspect_Chooser", movie);
}

void SuspectChooserRoom::verdict() {
	// 0x427DE0: Big Head reveals the villain, then the credits; the machine
	// remembers the game was finished (Scooby.ini GBH or FBF = Yes).
	const Common::String villain = _engine.getGameString("Whodunit");
	const Common::String scene = getRevealScene(_engine.getGameId(), villain, _right);
	debugC(1, kDebugScript, "suspects: the verdict, %s", scene.empty() ? "no scene" : scene.c_str());
	_engine.startBigHead(scene, isShowdown(_engine) ? "BIG_HEAD_BG_P03" : "BIG_HEAD_BG_P16", "ROOM_Credits");
	ConfMan.setBool("game_finished", true);
	ConfMan.flushToDisk();
}

bool SuspectChooserRoom::leaveBy(const Common::String &exitObject) {
	// 0x426F30: the exit goes back where the room was opened from; leaving
	// stops every sound (+0x10, 0x4270E0).
	if (_from.empty())
		return false;
	_engine.getSound()->stop();
	debugC(1, kDebugScript, "suspects: back to %s", _from.c_str());
	_engine.exitTo(_from);
	return true;
}

// ---------------------------------------------------------------------------
// Big Head (0x403180 entry, 0x403C70 tick, 0x4038F0 opening, 0x403940
// lines; 0x403F90 the partner, 0x404270 the background)

// The speakers looked for, in order (the 20-name table at 0x46A720; room.cpp
// has the same table for placements).
static const char *const kSpeakers[] = {
	"NARRATOR", "DAPHNE", "FRED", "SCOOBY", "SHAGGY", "VELMA", "ARTIE", "JENNY",
	"SHERIFF", "ICDOUBLE", "GUMMY", "DONKEY", "BRAD", "EWAN", "MARIA", "JANE",
	"WIZARD", "JESTER", "PRINCESS", "UNICYCLE"
};

Common::String BigHeadRoom::findSpeaker(int line) const {
	// The first name with a clip <scene><line>_<NAME>.
	Sound &sound = *_engine.getSound();
	for (uint i = 0; i < ARRAYSIZE(kSpeakers); i++) {
		if (sound.findClip(Common::String::format("%s%d_%s", _engine.getBigHeadScene().c_str(), line, kSpeakers[i])))
			return kSpeakers[i];
	}
	return Common::String();
}

void BigHeadRoom::setHead(int sprite, const Common::String &who, const char *part, const char *anim) {
	_engine.getRoom()->setSequence(sprite, Common::String::format("OBJ_%s_%s", who.c_str(), part),
	                               Common::String::format("ANIM_%s_%s", who.c_str(), anim), 0, 0);
}

void BigHeadRoom::enter(const Common::String &from) {
	// The scene is the room's opening (+0x30 sets it, and marks it unplayed);
	// the first line's speaker is shown from the start. If no line has one,
	// the last name tried stands.
	Room &room = *_engine.getRoom();
	_from = from;
	_line = 0;
	_clip.clear();
	_done = _left = false;
	_playing = true;
	// The scene is the room's opening: the base room starts it 100 ticks
	// after entry (+0x74), holding input meanwhile (0x40C7B0).
	_started = false;
	_delay = 100;
	_speaker = findSpeaker(1);
	if (_speaker.empty())
		_speaker = kSpeakers[ARRAYSIZE(kSpeakers) - 1];
	_background = room.getLayerSprite(0);
	if (_background < 0)
		_background = room.addGroupSprite(-1, -10000, 0, 0, "big head background");
	// The mouth over the body, and the partner's, under the first layer,
	// stopped.
	auto head = [&](const char *part, const char *anim, const char *label) {
		const int s = room.addEngineSprite(_background, Common::String::format("OBJ_%s_%s", _speaker.c_str(), part),
		                                   Common::String::format("ANIM_%s_%s", _speaker.c_str(), anim), 0, 0, 0, 1, label);
		room.setPlayer(s, 0, 0);
		return s;
	};
	_mouth = head("BIG_MOUTH", "BIG_MOUTHTALKS", "big head mouth");
	_body = head("BIG_BODY", "BIG_BODYA", "big head body");
	_partnerMouth = head("BIG_MOUTH", "BIG_MOUTHTALKS", "big head partner mouth");
	_partnerBody = head("BIG_BODY", "BIG_BODYA", "big head partner body");
	setPartner();
	_redraw = true;
	showBackground();
	debugC(1, kDebugScript, "big head: %s on %s, then %s; %s first", _engine.getBigHeadScene().c_str(), _engine.getBigHeadBackground().c_str(),
	       _engine.getBigHeadNext().c_str(), _speaker.c_str());
}

void BigHeadRoom::setPartner() {
	// 0x403F90: Shaggy talks with Scooby beside him, Fred with Daphne, and
	// the other way round; the partner in front (1) or behind (-1).
	Room &room = *_engine.getRoom();
	int depth = 0;
	_partner.clear();
	if (_speaker == "SHAGGY") {
		_partner = "SCOOBY";
		depth = 1;
	} else if (_speaker == "SCOOBY") {
		_partner = "SHAGGY";
		depth = -1;
	} else if (_speaker == "DAPHNE") {
		_partner = "FRED";
		depth = -1;
	} else if (_speaker == "FRED") {
		_partner = "DAPHNE";
		depth = 1;
	}
	if (_partner.empty()) {
		room.setHidden(_partnerMouth, true);
		room.setHidden(_partnerBody, true);
		_partnerShown = false;
		return;
	}
	setHead(_partnerMouth, _partner, "BIG_MOUTH", "BIG_MOUTHTALKS");
	room.setDepth(_partnerMouth, depth);
	room.setHidden(_partnerMouth, false);
	setHead(_partnerBody, _partner, "BIG_BODY", "BIG_BODYA");
	room.setDepth(_partnerBody, depth);
	room.setHidden(_partnerBody, false);
	_partnerShown = true;
}

void BigHeadRoom::showBackground() {
	// 0x404270: the background drawn for the speaker, or its DEFAULT.
	if (!_redraw)
		return;
	Room &room = *_engine.getRoom();
	const Common::String &bg = _engine.getBigHeadBackground();
	Common::String anim = "ANIM_" + bg + _speaker;
	if (!room.hasAnim(anim))
		anim = "ANIM_" + bg + "DEFAULT";
	room.setSequence(_background, "OBJ_" + bg, anim, 1, 0);
	_backgroundAnim = anim;
	_redraw = false;
}

void BigHeadRoom::nextLine() {
	// 0x403940 (the scene step, room vtable +0x38): with no line playing,
	// the next line's speaker comes up and speaks; with none left, the
	// opening counts as played.
	if (!_playing || !_clip.empty())
		return;
	Room &room = *_engine.getRoom();
	_line++;
	const Common::String speaker = findSpeaker(_line);
	if (speaker.empty()) {
		_playing = false;
		_done = true;
		debugC(1, kDebugScript, "big head: over after %d lines", _line - 1);
		return;
	}
	_speaker = speaker;
	_clip = Common::String::format("%s%d_%s", _engine.getBigHeadScene().c_str(), _line, _speaker.c_str());
	room.setHidden(_mouth, true);
	room.setHidden(_body, true);
	setHead(_mouth, _speaker, "BIG_MOUTH", "BIG_MOUTHTALKS");
	setHead(_body, _speaker, "BIG_BODY", "BIG_BODYA");
	setPartner();
	_redraw = true;
	showBackground();
	_engine.getSound()->play(_clip);
	debugC(1, kDebugScript, "big head: line %d, %s says %s", _line, _speaker.c_str(), _clip.c_str());
}

void BigHeadRoom::tick() {
	if (_playing && !_started) {
		if (_delay-- != 0)
			return;
		// 0x40B400: the scene starts, and the music goes down (0x434F10).
		// Big Head's own line player never ends it the base room's way, so
		// the music only comes back up with the next room.
		_started = true;
		_engine.getSound()->fadeMusic(90, 2, false);
		debugC(1, kDebugScript, "big head: the scene starts");
	}
	nextLine();
	Room &room = *_engine.getRoom();
	Sound &sound = *_engine.getSound();
	// 0x403C70: the lip sync's low nibble moves the mouth; its high nibble,
	// when set, shows that clue picture of the villain's instead of the
	// heads.
	uint shape, clue;
	if (!_clip.empty() && sound.getLipShape(_clip, shape, nullptr, &clue)) {
		if (shape)
			room.setPlayer(_mouth, 0, shape - 1);
		if (clue == 0) {
			room.setHidden(_mouth, false);
			room.setHidden(_body, false);
			showBackground();
			if (_partnerShown) {
				room.setHidden(_partnerMouth, false);
				room.setHidden(_partnerBody, false);
			}
		} else {
			room.setHidden(_mouth, true);
			room.setHidden(_body, true);
			room.setHidden(_partnerMouth, true);
			room.setHidden(_partnerBody, true);
			const Common::String villain = _engine.getGameString("Whodunit");
			room.setSequence(_background, "OBJ_BIG_HEAD_CLUES_" + villain,
			                 Common::String::format("ANIM_BIG_HEAD_CLUES_%s%u", villain.c_str(), clue), 1, 0);
			_redraw = true;
		}
	} else if (!_clip.empty() && !sound.isClipPlaying(_clip)) {
		_clip.clear();
	}
	// The scene over: the room after.
	if (_done && !_left) {
		_left = true;
		debugC(1, kDebugScript, "big head: to %s", _engine.getBigHeadNext().c_str());
		_engine.exitTo(_engine.getBigHeadNext());
	}
}

bool BigHeadRoom::skip() {
	// The skip key ends the scene, as it does the other rooms' openings.
	if (!_playing)
		return false;
	if (!_clip.empty())
		_engine.getSound()->stopClip(_clip);
	_clip.clear();
	_playing = false;
	_done = true;
	return true;
}

} // End of namespace Terraglyph
