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

#include "terraglyph/metaengine.h"
#include "terraglyph/detection.h"
#include "terraglyph/terraglyph.h"

const char *TerraglyphMetaEngine::getName() const {
	return "terraglyph";
}

Common::Error TerraglyphMetaEngine::createInstance(OSystem *syst, Engine **engine, const Terraglyph::TerraglyphGameDescription *desc) const {
	*engine = new Terraglyph::TerraglyphEngine(syst, desc);
	return Common::kNoError;
}

bool TerraglyphMetaEngine::hasFeature(MetaEngineFeature f) const {
	// Saves hold the player profile (engines/terraglyph/profile.cpp), in the
	// extended format with thumbnails, named <target>.NNN as the engine
	// writes them (Engine::getSaveStateName); without kSimpleSavesNames the
	// list would look for <target>.sNN and find none.
	return f == kSupportsListSaves || f == kSupportsDeleteSave || f == kSupportsLoadingDuringStartup ||
	       f == kSavesSupportMetaInfo || f == kSavesSupportThumbnail || f == kSavesSupportCreationDate ||
	       f == kSavesSupportPlayTime || f == kSavesUseExtendedFormat || f == kSimpleSavesNames;
}

#if PLUGIN_ENABLED_DYNAMIC(TERRAGLYPH)
REGISTER_PLUGIN_DYNAMIC(TERRAGLYPH, PLUGIN_TYPE_ENGINE, TerraglyphMetaEngine);
#else
REGISTER_PLUGIN_STATIC(TERRAGLYPH, PLUGIN_TYPE_ENGINE, TerraglyphMetaEngine);
#endif
