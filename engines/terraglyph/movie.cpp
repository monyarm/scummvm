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

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/endian.h"
#include "common/events.h"
#include "common/file.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "graphics/surface.h"
#include "video/bink_decoder.h"

#include "terraglyph/terraglyph.h"
#include "terraglyph/sound.h"

namespace Terraglyph {

// How long past its length a movie may run before it counts as stalled.
static const uint32 kMovieStallMillis = 2000;

// 0x4362E0 builds "%sBK.%.3d" from the movie id.
static Common::Path getMovieName(int id) {
	return Common::Path(Common::String::format("BK.%03d", id));
}

bool TerraglyphEngine::playMovie(int id) {
	// 0x42DE10: the first viewing can't be skipped unless Scooby.ini's
	// AbortMovies is on; later ones can. The title movies (0, 25-28) aren't
	// in the history: they can be skipped once the title room has set the
	// machine's GBHTitleMovies (Phantom: FBFTitleMovies) to Yes, kept here as
	// the "title_movies_seen" config key.
	_lastMovie = id;
	bool skippable;
	if (isTitleMovie(id)) {
		skippable = ConfMan.hasKey("title_movies_seen") && ConfMan.getBool("title_movies_seen");
	} else {
		skippable = _seenMovies.contains(id) || (ConfMan.hasKey("abort_movies") && ConfMan.getBool("abort_movies"));
		_seenMovies[id] = true;
	}

	// The Bink header holds the file's size less 8 at offset 4; it matches
	// on every movie of both discs. A truncated file can hang the decoder
	// mid-frame, so check it first.
	Common::File file;
	if (file.open(getMovieName(id))) {
		const uint32 tag = file.readUint32BE();
		const uint32 size = file.readUint32LE();
		if ((tag >> 8) != MKTAG(0, 'B', 'I', 'K') || (uint64)size + 8 != (uint64)file.size()) {
			warning("Movie %d: %s is damaged (header says %u bytes, file has %u)", id,
			        getMovieName(id).toString().c_str(), size + 8, (uint)file.size());
			return false;
		}
		file.close();
	}

	Video::BinkDecoder decoder;
	decoder.setSoundType(Audio::Mixer::kSFXSoundType);
	if (!decoder.loadFile(getMovieName(id))) {
		warning("Movie %d: cannot open %s", id, getMovieName(id).toString().c_str());
		return false;
	}
	if (!decoder.setOutputPixelFormat(_system->getScreenFormat())) {
		warning("Movie %d: cannot decode to the screen format", id);
		return false;
	}

	// 0x4362E0 stops the game's sounds and clears the screen first.
	_sound->stop();
	_system->fillScreen(0);
	_system->updateScreen();
	debugC(1, kDebugScript, "Playing movie (%d): %u frames, %u ms%s", id, decoder.getFrameCount(),
	       decoder.getDuration().msecs(), skippable ? ", skippable" : "");

	// Centre the frames; every Gen 1 movie is 640x480.
	const int x = MAX<int>(0, (640 - decoder.getWidth()) / 2);
	const int y = MAX<int>(0, (480 - decoder.getHeight()) / 2);
	const uint32 start = _system->getMillis();
	uint frames = 0;
	bool skipped = false;
	decoder.start();
	while (!shouldQuit() && !decoder.endOfVideo() && !skipped) {
		if (decoder.needsUpdate()) {
			const Graphics::Surface *frame = decoder.decodeNextFrame();
			if (frame) {
				_system->copyRectToScreen(frame->getPixels(), frame->pitch, x, y,
				                          MIN<int>(frame->w, 640 - x), MIN<int>(frame->h, 480 - y));
				if (frames++ == 0)
					debugC(1, kDebugScript, "movie (%d) sound on the SFX channel: %s", id,
					       _mixer->hasActiveChannelOfType(Audio::Mixer::kSFXSoundType) ? "playing" : "none");
			}
			_system->updateScreen();
		}

		// The original stops on input slots 4 and 5 or a mouse button; which
		// keys those slots hold was not traced, so Escape and Space stand in.
		Common::Event e;
		while (_system->getEventManager()->pollEvent(e)) {
			const bool key = e.type == Common::EVENT_KEYDOWN &&
			                 (e.kbd.keycode == Common::KEYCODE_ESCAPE || e.kbd.keycode == Common::KEYCODE_SPACE);
			const bool click = e.type == Common::EVENT_LBUTTONDOWN || e.type == Common::EVENT_RBUTTONDOWN;
			if (e.type == Common::EVENT_MOUSEMOVE)
				_mousePos = e.mouse;
			if (skippable && (key || click))
				skipped = true;
		}

		// A damaged file can stop decoding without ever ending; give up two
		// seconds past the movie's length.
		if (_system->getMillis() - start > decoder.getDuration().msecs() + kMovieStallMillis) {
			warning("Movie %d: stalled after %u of %u frames; going on", id, frames, decoder.getFrameCount());
			break;
		}
		_system->delayMillis(5);
	}
	decoder.close();
	debugC(1, kDebugScript, "movie (%d) %s after %u frames in %u ms", id, skipped ? "skipped" : "ended", frames,
	       _system->getMillis() - start);
	return true;
}

static void meanColour(const Graphics::Surface &s, uint32 &r, uint32 &g, uint32 &b) {
	// Tenths of a level, per channel, over the whole frame.
	uint64 sr = 0, sg = 0, sb = 0;
	for (int y = 0; y < s.h; y++) {
		for (int x = 0; x < s.w; x++) {
			uint8 a, cr, cg, cb;
			s.format.colorToARGB(s.getPixel(x, y), a, cr, cg, cb);
			sr += cr;
			sg += cg;
			sb += cb;
		}
	}
	const uint64 n = (uint64)s.w * s.h;
	r = (uint32)((sr * 10 + n / 2) / n);
	g = (uint32)((sg * 10 + n / 2) / n);
	b = (uint32)((sb * 10 + n / 2) / n);
}

void TerraglyphEngine::checkAllMovies() {
	// Every BK.NNN present: decode it to its last frame, at 32 bits so the
	// colours aren't rounded to the screen's 565, and log its size, frame
	// count, length and the mean colour of its first, middle and last
	// frames.
	const Graphics::PixelFormat argb(4, 8, 8, 8, 8, 16, 8, 0, 24);
	uint movies = 0, bad = 0;
	for (int id = 0; id < 100; id++) {
		const Common::Path name = getMovieName(id);
		if (!SearchMan.hasFile(name))
			continue;
		movies++;
		Video::BinkDecoder decoder;
		if (!decoder.loadFile(name) || !decoder.setOutputPixelFormat(argb)) {
			debugC(3, kDebugScript, "movie check %s BAD", name.toString().c_str());
			bad++;
			continue;
		}
		const int count = decoder.getFrameCount();
		const int marks[] = { 0, count / 2, count - 1 };
		Common::String colours;
		int decoded = 0;
		while (!decoder.endOfVideo()) {
			const Graphics::Surface *frame = decoder.decodeNextFrame();
			if (!frame)
				break;
			for (uint k = 0; k < ARRAYSIZE(marks); k++) {
				if (decoded == marks[k]) {
					uint32 r, g, b;
					meanColour(*frame, r, g, b);
					colours += Common::String::format(" %u,%u,%u", r, g, b);
				}
			}
			decoded++;
		}
		if (decoded != count)
			bad++;
		debugC(3, kDebugScript, "movie check %s %ux%u %d frames %u ms decoded %d colours%s", name.toString().c_str(),
		       decoder.getWidth(), decoder.getHeight(), count, decoder.getDuration().msecs(), decoded, colours.c_str());
		decoder.close();
	}
	debugC(3, kDebugScript, "movie check: %u movies, %u bad", movies, bad);
}

} // End of namespace Terraglyph
