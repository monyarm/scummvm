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

#include "common/debug.h"
#include "common/random.h"

#include "scooby/detection.h"
#include "scooby/room.h"
#include "scooby/room_script.h"
#include "scooby/sound.h"

namespace Scooby {

// The exit keys, in Gang order (0x401850 reads fred=, velma=, daphne=,
// shaggy= and scooby=; never character= or direction=).
static const char *const kGangNames[] = { "FRED", "VELMA", "DAPHNE", "SHAGGY", "SCOOBY" };
static const char *const kGangKeys[] = { "fred", "velma", "daphne", "shaggy", "scooby" };

// The walk-off starts the others in this order (0x40BE20).
static const RoomScript::Gang kFollowOrder[] = {
	RoomScript::kFred, RoomScript::kDaphne, RoomScript::kVelma, RoomScript::kShaggy, RoomScript::kScooby
};

// After the room's characters comes an invisible narrator (0x409670).
static const char *const kNarrator = "OBJ_NARRATOR_A";

// A scene runs out of lines long before this.
static const int kMaxLines = 99;

// A clickable plays its state once at 3 ticks per step, then rests stopped
// on step 0 of its next state (0x401440).
static const int kModeStopped = 0;
static const int kModeOnce = 3;

// When the ghost has appeared, the game goes here (0x409BCA).
static const char *const kGhostRoom = "ROOM_Ghost_Spot";

// Entering any room from the ghost spot, its opening is one of these
// (0x40B920, rand() % 3).
static const char *const kGhostOpenings[] = { "Global.Ghost_Spot_", "Global.Ghost_SpotB_", "Global.Ghost_SpotC_" };

// The opening starts this many ticks after entry (+0x74, from the
// constructor and the room's leave hook 0x40BB70).
static const int kOpeningDelay = 100;

static Common::String getIniString(const Common::INIFile &ini, const Common::String &section, const char *key) {
	Common::String value;
	if (!ini.getKey(key, section, value))
		return Common::String();
	value.trim();
	return value;
}

RoomScript::RoomScript(Room &room, Sound &sound, const Common::INIFile &objectIni) :
	_room(room), _sound(sound), _ini(objectIni), _line(0), _speaker(-1), _exitParked(false), _leader(-1),
	_busy(false), _done(false), _tick(0), _ghost(-1), _ghostPlaying(false), _ghostDone(false), _random(nullptr),
	_ghostChance(0) {
	_exit.movie = -1;
	for (uint g = 0; g < kGangCount; g++)
		_lengths[g] = -1;
}

void RoomScript::enter() {
	// The constructor's switch to IDLE_MASTER (0x4048E0) is skipped: 258 of
	// the 261 character placements already play it, and the other three are
	// set pieces their rooms' own code drives. Its idle countdown is kept,
	// drawn in placement order, then the narrator's.
	_characters.clear();
	const Common::Array<uint> &sprites = _room.getCharacters();
	for (uint i = 0; i < sprites.size(); i++)
		_characters.push_back(Character(_room.getObjectName(sprites[i]), sprites[i]));
	_characters.push_back(Character(kNarrator, -1));
	for (uint i = 0; i < _characters.size(); i++)
		_characters[i].startIdles(this);

	_scene.clear();
	_line = 0;
	_speaker = -1;
	if (_sceneHoldsLock)
		releaseSpeechLock();
	_sceneHoldsLock = false;
	_exitParked = false;
	_exit = Exit();
	_exit.movie = -1;
	_leader = -1;
	for (uint g = 0; g < kGangCount; g++)
		_lengths[g] = -1;
	_busy = false;
	_done = false;
	_tick = 0;
	_walkLog.clear();
	// setup() fills these in.
	_memory = nullptr;
	_opening.clear();
	_snacksNeeded = 0;
	_sceneIsOpening = false;

	// The default clickables (0x401000): the state is the placement's ANIM,
	// or the one the save file holds, shown stopped on step 0.
	_clickables.clear();
	for (uint s = 0; s < _room.getSpriteCount(); s++) {
		if (!_room.isClickable(s))
			continue;
		Clickable c;
		c.sprite = s;
		c.object = _room.getObjectName(s);
		c.state = _room.getAnim(s);
		c.playing = false;
		c.clicked = false;
		if (_clickableStates.contains(c.object)) {
			c.state = _clickableStates[c.object];
			_room.startAnim(s, c.state, kModeStopped, -1);
		}
		// A state the art file doesn't name gets the last letter A.
		if (!_room.hasAnim(c.state) && !c.state.empty())
			c.state.setChar('A', c.state.size() - 1);
		setSoundName(c);
		_clickables.push_back(c);
	}
	_clickLog.clear();

	_ghost = _room.findGhost();
	_ghostPlaying = false;
	_ghostDone = false;
}

uint32 RoomScript::draw(uint n) {
	return _rng ? _rng->getRandomNumber(n - 1) : 0;
}

void RoomScript::restartIdle() {
	// (rand() % 30 + 25) * 36, as the room's own entry sets it.
	_idleDelay = (draw(30) + 25) * Room::kTicksPerSecond;
}

Common::String RoomHooks::idleLine(RoomScript &script, const Common::String &name) {
	return script.getBaseIdleLine(name);
}

Common::String RoomScript::getIdleLine(const Common::String &name) {
	return _hooks ? _hooks->idleLine(*this, name) : getBaseIdleLine(name);
}

Common::String RoomScript::getBaseIdleLine(const Common::String &name) {
	// 0x40CB50: the room's name without its first five letters (ROOM_).
	const Common::String &room = _room.getName();
	Common::String line = Common::String(room.c_str() + MIN<uint>(5, room.size())) + ".Idle." + name;
	if (_sound.findClip(line))
		return line;
	// Else r = rand() % r from 9: at 0 Global.Idle.<NAME>; above it the
	// literal Global.Idle%d.<NAME>, tried and not found.
	uint r = 9;
	do {
		r = _idleRandom ? _idleRandom->getRandomNumber(r - 1) : 0;
		line = (r == 0 ? Common::String("Global.Idle.") : Common::String("Global.Idle%d.")) + name;
	} while (!_sound.findClip(line) && r != 0);
	return line;
}

void RoomScript::logState(const Character &c) {
	if (!_stateLog)
		return;
	const int s = c.getSprite();
	const Common::String &line = c.getState() == Character::kStateTalk ? c.getClipName() : c.getIdleSound();
	_stateLog->push_back(Common::String::format("%s %d %s mode %d line %s countdown %d", c.getName().c_str(), c.getState(),
	                                            s >= 0 ? _room.getAnim(s).c_str() : "-", s >= 0 ? _room.getMode(s) : 0,
	                                            line.empty() ? "-" : line.c_str(), c.getIdleCountdown()));
}

bool RoomScript::clickCharacter(uint sprite) {
	for (uint i = 0; i < _characters.size(); i++) {
		if (_characters[i].getSprite() == (int)sprite) {
			_characters[i].click();
			return true;
		}
	}
	return false;
}

void RoomScript::setup(const Common::String &opening, uint snacksNeeded, const Common::String &from) {
	_memory = &getMemory(_room.getName());
	_opening = opening;
	_snacksNeeded = snacksNeeded;
	_openingDelay = kOpeningDelay;
	_sceneIsOpening = false;
	// 0x40B920: the class's +0x30 first, which may set this visit's opening.
	if (_hooks) {
		_hooks->prepare();
		_hooks->arrive(from);
	}
	// 0x40B920: coming from the ghost spot, the gang talks about it first.
	if (from == kGhostRoom) {
		_opening = kGhostOpenings[draw(ARRAYSIZE(kGhostOpenings))];
		_memory->openingPlayed = false;
	}
	checkOpening();
	_idleDelay = (draw(30) + 25) * Room::kTicksPerSecond;
	debugC(1, kDebugScript, "room %s: opening '%s'%s, %u snacks needed, %u given", _room.getName().c_str(), _opening.c_str(),
	       _memory->openingPlayed ? " (played)" : "", _snacksNeeded, _memory->snacksGiven);
}

void RoomScript::checkOpening() {
	// 0x40C7B0: an opening someone here has a first line of holds input until
	// it starts; one nobody can say counts as played.
	if (!_memory || _opening.empty() || _memory->openingPlayed)
		return;
	bool lines = false;
	for (uint i = 0; i < _characters.size() && !lines; i++)
		lines = _sound.findClip(Common::String::format("%s1_%s", _opening.c_str(), _characters[i].getName().c_str())) != nullptr;
	if (lines) {
		_busy = true;
	} else {
		_opening.clear();
		_memory->openingPlayed = true;
	}
}

void RoomScript::setOpening(const Common::String &scene) {
	_opening = scene;
	if (_memory)
		_memory->openingPlayed = false;
}

bool RoomScript::isOpeningDone() const {
	return _opening.empty() || (_memory && _memory->openingPlayed);
}

void RoomScript::sendEvent(const Common::String &object, bool success) {
	// Game vtable +0x40 (0x40A850) -> the room's vtable +0x14.
	debugC(1, kDebugScript, "room event %s %d", object.c_str(), success ? 1 : 0);
	if (_hooks)
		_hooks->event(object, success);
}

void RoomScript::takeExit(const Exit &exit) {
	// The exit's click (0x402130) hands it to the room's vtable +0x24.
	if (_hooks && _hooks->leaveBy(exit.object)) {
		debugC(1, kDebugScript, "exit %s: the room's class keeps the gang here", exit.object.c_str());
		return;
	}
	Exit e = exit;
	if (_hooks) {
		_hooks->leaving(e.destination, e.scene);
		_hooks->exitMovie(e.destination, e.movie);
	}
	startExit(e);
}

bool RoomScript::skip() {
	bool skipped = false;
	// 0x40B490: the opening's hold ends, and it counts as played.
	if (_memory && !_opening.empty() && !_memory->openingPlayed) {
		_memory->openingPlayed = true;
		if (!_exitParked)
			_busy = false;
		skipped = true;
	}
	// 0x40C810: the line playing stops, and the scene ends.
	if (!_scene.empty()) {
		if (_speaker >= 0)
			_sound.stopClip(_characters[_speaker].getClipName());
		debugC(1, kDebugScript, "scene %s skipped at line %d", _scene.c_str(), _line);
		endScene();
		skipped = true;
	}
	return skipped;
}

bool RoomScript::snackEaten() {
	// 0x40B490: +0x88 counts the snacks eaten here; the line waits for +0x24.
	if (!_memory)
		return true;
	_memory->snacksGiven++;
	return _memory->snacksGiven >= _snacksNeeded;
}

void RoomScript::tickOpening() {
	// 0x40B490: not played, no exit parked, no scene: the delay runs out, and
	// the opening starts, blocking. Its end marks it played (+0x80 -> +0x10).
	if (!_memory || _memory->openingPlayed || _exitParked || !_scene.empty())
		return;
	if (_openingDelay-- != 0)
		return;
	if (startScene(_opening, true)) {
		_sceneIsOpening = true;
		debugC(1, kDebugScript, "room %s: opening %s at tick %u", _room.getName().c_str(), _opening.c_str(), _tick);
	}
}

void RoomScript::tickIdle() {
	// 0x40B490: 25-54 seconds after the last scene, with the opening done,
	// the room's idle chatter (vtable +0x34).
	if (_idleDelay-- >= 1 || !_scene.empty() || !isOpeningDone())
		return;
	debugC(1, kDebugScript, "room %s: idle chatter at tick %u", _room.getName().c_str(), _tick);
	if (_hooks)
		_hooks->idle();
	_idleDelay = (draw(30) + 25) * Room::kTicksPerSecond;
}

void RoomScript::setGhostChance(Common::RandomSource *random, uint chance) {
	_random = random;
	_ghostChance = chance;
}

void RoomScript::setSoundName(Clickable &c) {
	// 0x401650: "SFX_" and the state without its "ANIM_" prefix.
	c.sound = c.state.size() > 5 ? Common::String("SFX_") + Common::String(c.state.c_str() + 5) : Common::String();
}

RoomScript::Clickable *RoomScript::findClickable(uint sprite) {
	for (uint i = 0; i < _clickables.size(); i++) {
		if (_clickables[i].sprite == sprite)
			return &_clickables[i];
	}
	return nullptr;
}

bool RoomScript::clickClickable(uint sprite) {
	Clickable *c = findClickable(sprite);
	if (!c)
		return false;
	c->clicked = true;
	return true;
}

bool RoomScript::isClickablePlaying(uint sprite) const {
	for (uint i = 0; i < _clickables.size(); i++) {
		if (_clickables[i].sprite == sprite)
			return _clickables[i].playing;
	}
	return false;
}

Common::String RoomScript::getClickableState(uint sprite) const {
	for (uint i = 0; i < _clickables.size(); i++) {
		if (_clickables[i].sprite == sprite)
			return _clickables[i].state;
	}
	return Common::String();
}

void RoomScript::setClickable(uint sprite, const Common::String &state) {
	// 0x401780: +0x60 is the state, A for one the art file lacks; the
	// sound name follows it.
	if (Clickable *c = findClickable(sprite)) {
		c->state = state;
		if (!_room.hasAnim(c->state) && !c->state.empty())
			c->state.setChar('A', c->state.size() - 1);
		_clickableStates[c->object] = c->state;
		setSoundName(*c);
	}
}

void RoomScript::advanceClickable(Clickable &c) {
	// 0x401650: the last character + 1, unless that makes a digit 1-9; a
	// name the art file doesn't have goes back to A.
	if (c.state.empty())
		return;
	const char next = c.state.lastChar() + 1;
	if (next < '1' || next > '9') {
		c.state.setChar(next, c.state.size() - 1);
		if (!_room.hasAnim(c.state))
			c.state.setChar('A', c.state.size() - 1);
		// The destructor (0x401350) saves it under the same rule.
		_clickableStates[c.object] = c.state;
	}
	setSoundName(c);
}

void RoomScript::tickClickable(Clickable &c) {
	// 0x401440, once a tick after the sprite's player. A click while the
	// object is playing is ignored.
	const bool clicked = c.clicked;
	c.clicked = false;
	if (clicked && !c.playing) {
		// The ghost can come instead (0x40A990): 1 in 30, if the room has one.
		if (_random && _ghostChance && _random->getRandomNumber(_ghostChance - 1) == 0 && startGhost()) {
			_clickLog = "ghost";
			debugC(1, kDebugScript, "clickable %s: the ghost appears at tick %u", c.object.c_str(), _tick);
		} else {
			const Common::String state = c.state;
			const Common::String sound = c.sound;
			_room.startAnim(c.sprite, state, kModeOnce, -1);
			const uint32 steps = _room.getStepCount(c.sprite);
			// Stop, then play, its sound (0x434CB0, 0x434C70), if it exists.
			const bool hasSound = _sound.findClip(sound) != nullptr;
			if (hasSound)
				_sound.play(sound);
			advanceClickable(c);
			_clickLog = Common::String::format("anim %s steps %u sound %s next %s", state.c_str(), steps,
			                                   hasSound ? sound.c_str() : "-", c.state.c_str());
			debugC(1, kDebugScript, "clickable %s clicked at tick %u: %s", c.object.c_str(), _tick, _clickLog.c_str());
			c.playing = true;
			// The room's handler (vtable +0x14, through game +0x40, with 0)
			// does nothing in the default room; story rooms override it.
			sendEvent(c.object, false);
			if (_hooks)
				_hooks->clicked(c.sprite);
		}
	}

	// At the end of its sequence it rests on step 0 of the new state.
	if (c.playing && _room.isStopped(c.sprite)) {
		_room.startAnim(c.sprite, c.state, kModeStopped, -1);
		c.playing = false;
		debugC(1, kDebugScript, "clickable %s rests on %s at tick %u", c.object.c_str(), c.state.c_str(), _tick);
	}
}

bool RoomScript::startGhost() {
	// 0x40A990: show the ghost and play it once, play Ghost_Appears, and
	// scare every character. Input stays blocked until the room changes.
	if (_ghost < 0 || _ghostPlaying)
		return false;
	_room.setHidden(_ghost, false);
	_room.startAnim(_ghost, _room.getAnim(_ghost), kModeOnce, -1);
	_ghostPlaying = true;
	_busy = true;
	_sound.play("Ghost_Appears");
	for (uint i = 0; i < _characters.size(); i++)
		_characters[i].setState(_room, _sound, Character::kStateStayScared);
	return true;
}

RoomScript::Exit RoomScript::readExit(const Common::String &object) const {
	Exit e;
	e.object = object;
	e.destination = getIniString(_ini, object, "destinationroom");
	e.scene = getIniString(_ini, object, "scene");
	const Common::String movie = getIniString(_ini, object, "movie");
	e.movie = movie.empty() ? -1 : atoi(movie.c_str());
	for (uint g = 0; g < kGangCount; g++)
		e.directions[g] = getIniString(_ini, object, kGangKeys[g]);
	return e;
}

Character *RoomScript::findCharacter(const char *name) {
	// 0x40A800: the first character with the name.
	for (uint i = 0; i < _characters.size(); i++) {
		if (_characters[i].getName().equalsIgnoreCase(name))
			return &_characters[i];
	}
	return nullptr;
}

bool RoomScript::hasCharacter(const char *name) {
	const Character *c = findCharacter(name);
	return c && c->getSprite() >= 0;
}

bool RoomScript::startScene(const Common::String &scene, bool block) {
	// 0x40B400: only if no scene is playing and no exit is parked. A
	// blocking scene stops input until it is over.
	if (scene.empty() || !_scene.empty() || _exitParked)
		return false;
	_scene = scene;
	_line = 0;
	_speaker = -1;
	_sceneBlocks = block;
	if (block)
		_busy = true;
	// 0x40B400 turns the music down while a scene plays (0x434F10).
	_sound.fadeMusic(90, 2, false);
	debugC(1, kDebugScript, "scene %s starts%s", scene.c_str(), block ? "" : " (input stays on)");
	return true;
}

void RoomScript::startExit(const Exit &exit) {
	// The cursor fades out and input stops (0x435150, game +0x24).
	_busy = true;
	startScene(exit.scene);
	_exit = exit;
	_exitParked = true;
	_leader = -1;
	_done = false;
	debugC(1, kDebugScript, "exit %s: scene '%s', then %s", exit.object.c_str(), exit.scene.c_str(), exit.destination.c_str());
}

void RoomScript::tick() {
	// The sprite tree ticks first (0x431130): each character's update runs
	// after its player. Then the room's own tick (0x40B490).
	for (uint i = 0; i < _clickables.size(); i++)
		tickClickable(_clickables[i]);
	for (uint i = 0; i < _characters.size(); i++)
		_characters[i].update(_room, _sound);
	// 0x409BCA: when the ghost's sequence ends, go to the ghost room.
	if (_ghostPlaying && !_ghostDone && _room.isStopped(_ghost)) {
		_ghostDone = true;
		debugC(1, kDebugScript, "the ghost is gone at tick %u: going to %s", _tick, kGhostRoom);
	}
	if (!_scene.empty()) {
		// 0x40C810: the idle timer starts again while a scene plays.
		_idleDelay = (draw(30) + 25) * Room::kTicksPerSecond;
		tickScene();
	}
	tickOpening();
	// 0x40B490: the walk-off waits for the speech lock.
	if (_scene.empty() && _exitParked && !_done && !_speechLock)
		tickExit();
	tickIdle();
	_tick++;
}

bool RoomScript::acquireSpeechLock() {
	// 0x40A230: game +0x15C.
	if (_speechLock)
		return false;
	_speechLock = true;
	debugC(2, kDebugScript, "Acquired Speech Lock");
	return true;
}

void RoomScript::releaseSpeechLock() {
	_speechLock = false;
	debugC(2, kDebugScript, "Released Speech Lock");
}

Character *RoomScript::getCharacter(const char *name) {
	Character *c = findCharacter(name);
	return c && c->getSprite() >= 0 ? c : nullptr;
}

void RoomScript::tickScene() {
	// 0x40C810: a scene holds the speech lock from its first line to its
	// end, and waits while someone else (Scrappy) holds it.
	if (!_sceneHoldsLock) {
		if (!acquireSpeechLock())
			return;
		_sceneHoldsLock = true;
	}
	// Wait for the speaker, then look for the next line.
	if (_speaker >= 0 && _characters[_speaker].isBusy())
		return;
	_line++;
	if (_line <= kMaxLines) {
		for (uint i = 0; i < _characters.size(); i++) {
			const Common::String clip = Common::String::format("%s%d_%s", _scene.c_str(), _line, _characters[i].getName().c_str());
			if (_sound.findClip(clip)) {
				_speaker = i;
				debugC(1, kDebugScript, "scene %s line %d: %s says %s at tick %u", _scene.c_str(), _line,
				       _characters[i].getName().c_str(), clip.c_str(), _tick);
				_characters[i].say(_room, _sound, clip);
				return;
			}
		}
	}

	debugC(1, kDebugScript, "scene %s over after %d lines at tick %u", _scene.c_str(), _line - 1, _tick);
	endScene();
}

void RoomScript::endScene() {
	// 0x40C810's end, played out or skipped: the music back up (0x434F30).
	_sound.fadeMusic(100, 4, true);
	_scene.clear();
	_speaker = -1;
	if (!_exitParked && _sceneBlocks)
		_busy = false;
	if (_sceneHoldsLock) {
		_sceneHoldsLock = false;
		releaseSpeechLock();
	}
	// The opening's done flag (+0x80 points at +0x10).
	if (_sceneIsOpening && _memory)
		_memory->openingPlayed = true;
	_sceneIsOpening = false;
}

void RoomScript::tickExit() {
	// 0x40BE20: the longest walk leads; the others start so that they all
	// end with it, and the room changes on the leader's last step.
	if (_leader < 0) {
		int longest = 0;
		for (uint g = 0; g < kGangCount; g++) {
			_lengths[g] = -1;
			Character *c = findCharacter(kGangNames[g]);
			if (!c || c->getSprite() < 0)
				continue;
			Common::String obj, anim;
			c->getWalk(_exit.directions[g], obj, anim);
			_lengths[g] = _room.getSequenceSteps(obj, anim);
			if (_lengths[g] != -1 && longest < _lengths[g]) {
				_leader = g;
				longest = _lengths[g];
			}
		}
	}

	if (_leader >= 0) {
		Character *leader = findCharacter(kGangNames[_leader]);
		if (leader->getState() != Character::kStateWalk) {
			leader->walk(_room, _sound, _exit.directions[_leader]);
			_walkLog += Common::String::format("%s %s %d@%u ", kGangNames[_leader], _exit.directions[_leader].c_str(), _lengths[_leader], _tick);
			debugC(1, kDebugScript, "%s leads: %s, %d steps, at tick %u", kGangNames[_leader], _exit.directions[_leader].c_str(), _lengths[_leader], _tick);
		}
		const int left = (int)_room.getStepCount(leader->getSprite()) - (int)_room.getStep(leader->getSprite());
		if (left != 1) {
			for (uint k = 0; k < ARRAYSIZE(kFollowOrder); k++) {
				const Gang g = kFollowOrder[k];
				Character *c = findCharacter(kGangNames[g]);
				if (left - 1 <= _lengths[g] && c && c->getState() != Character::kStateWalk) {
					c->walk(_room, _sound, _exit.directions[g]);
					_walkLog += Common::String::format("%s %s %d@%u ", kGangNames[g], _exit.directions[g].c_str(), _lengths[g], _tick);
					debugC(1, kDebugScript, "%s follows: %s, %d steps, at tick %u", kGangNames[g], _exit.directions[g].c_str(), _lengths[g], _tick);
				}
			}
			return;
		}
	}

	debugC(1, kDebugScript, "exit %s: going to %s at tick %u", _exit.object.c_str(), _exit.destination.c_str(), _tick);
	_done = true;
}

bool RoomScript::getNextRoom(Common::String &room, int &movie) const {
	if (_ghostDone) {
		room = kGhostRoom;
		movie = -1;
		return true;
	}
	if (!_done)
		return false;
	room = _exit.destination;
	movie = _exit.movie;
	return true;
}

Common::String RoomScript::describeCharacters() const {
	Common::String s;
	for (uint i = 0; i < _characters.size(); i++) {
		if (i)
			s += ' ';
		s += _characters[i].getName();
	}
	return s;
}

Common::String RoomScript::describeScene(const Common::String &scene) const {
	Common::String s;
	if (scene.empty())
		return s;
	for (int line = 1; line <= kMaxLines; line++) {
		bool found = false;
		for (uint i = 0; i < _characters.size() && !found; i++) {
			if (_sound.findClip(Common::String::format("%s%d_%s", scene.c_str(), line, _characters[i].getName().c_str()))) {
				s += Common::String::format("%s%d:%s", s.empty() ? "" : " ", line, _characters[i].getName().c_str());
				found = true;
			}
		}
		if (!found)
			break;
	}
	return s;
}

} // End of namespace Scooby
