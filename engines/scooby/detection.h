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

#ifndef SCOOBY_DETECTION_H
#define SCOOBY_DETECTION_H

#include "engines/advancedDetector.h"

namespace Scooby {

// Debug channels. The names are fixed by the project's integration contract
// (docs/02-SCUMMVM-INTEGRATION.md, section 9) so that analysis notes and
// debugC() calls use the same vocabulary.
enum ScoobyDebugChannels {
	kDebugResource = 1, // archive and file loading (TGIFILE.ART, MMFW)
	kDebugGraphics,     // cel decoding and rendering
	kDebugAudio,        // mixer routing and sound archives
	kDebugScript,       // object.ini / room logic interpretation
	kDebugInput,        // event handling
};

extern const PlainGameDescriptor scoobyGames[];

extern const ADGameDescription gameDescriptions[];

} // End of namespace Scooby

class ScoobyMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
	static const DebugChannelDef debugFlagList[];

public:
	ScoobyMetaEngineDetection();
	~ScoobyMetaEngineDetection() override {}

	const char *getName() const override {
		return "scooby";
	}

	const char *getEngineName() const override {
		return "Scooby-Doo (TerraGlyph)";
	}

	const char *getOriginalCopyright() const override {
		return "Scooby-Doo! (C) Warner Bros. Entertainment Inc.";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

#endif // SCOOBY_DETECTION_H
