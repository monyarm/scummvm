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

#ifndef TERRAGLYPH_DETECTION_H
#define TERRAGLYPH_DETECTION_H

#include "engines/advancedDetector.h"

namespace Terraglyph {

// Debug channels. The names are fixed by the project's integration contract
// (docs/02-SCUMMVM-INTEGRATION.md, section 9) so that analysis notes and
// debugC() calls use the same vocabulary.
enum TerraglyphDebugChannels {
	kDebugResource = 1, // archive and file loading (TGIFILE.ART, MMFW)
	kDebugGraphics,     // cel decoding and rendering
	kDebugAudio,        // mixer routing and sound archives
	kDebugScript,       // object.ini / room logic interpretation
	kDebugInput,        // event handling
};

// Engine generations. The three generations share one code lineage but use
// different archive, config and video formats, so the engine dispatches on
// this value (docs/02-SCUMMVM-INTEGRATION.md, section 3).
enum TerraglyphGeneration {
	kGen1 = 1, // Showdown, Phantom: TGIFILE.ART, object.ini, .dat audio
	kGen2 = 2, // Jinx, Activity Challenge: MMFW archives (.MMF, .MMA, .MMP, .mms)
	kGen3 = 3  // Case File #1: MMFW archives with XML config
};

struct TerraglyphGameDescription {
	AD_GAME_DESCRIPTION_HELPERS(desc);

	ADGameDescription desc;
	TerraglyphGeneration generation;
};

extern const PlainGameDescriptor terraglyphGames[];

extern const TerraglyphGameDescription gameDescriptions[];

} // End of namespace Terraglyph

class TerraglyphMetaEngineDetection : public AdvancedMetaEngineDetection<Terraglyph::TerraglyphGameDescription> {
	static const DebugChannelDef debugFlagList[];

public:
	TerraglyphMetaEngineDetection();
	~TerraglyphMetaEngineDetection() override {}

	const char *getName() const override {
		return "terraglyph";
	}

	const char *getEngineName() const override {
		return "TerraGlyph";
	}

	const char *getOriginalCopyright() const override {
		return "Scooby-Doo! (C) Warner Bros. Entertainment Inc.";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

#endif // TERRAGLYPH_DETECTION_H
