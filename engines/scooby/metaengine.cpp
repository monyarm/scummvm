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

#include "scooby/metaengine.h"
#include "scooby/detection.h"
#include "scooby/scooby.h"

const char *ScoobyMetaEngine::getName() const {
	return "scooby";
}

Common::Error ScoobyMetaEngine::createInstance(OSystem *syst, Engine **engine, const Scooby::ScoobyGameDescription *desc) const {
	*engine = new Scooby::ScoobyEngine(syst, desc);
	return Common::kNoError;
}

bool ScoobyMetaEngine::hasFeature(MetaEngineFeature f) const {
	// No saves or launcher features yet.
	return false;
}

#if PLUGIN_ENABLED_DYNAMIC(SCOOBY)
REGISTER_PLUGIN_DYNAMIC(SCOOBY, PLUGIN_TYPE_ENGINE, ScoobyMetaEngine);
#else
REGISTER_PLUGIN_STATIC(SCOOBY, PLUGIN_TYPE_ENGINE, ScoobyMetaEngine);
#endif
