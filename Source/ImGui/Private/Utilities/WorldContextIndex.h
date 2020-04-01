// Distributed under the MIT License (MIT) (see accompanying LICENSE file)

#pragma once

#include "Utilities/WorldContext.h"


// Utilities mapping worlds to indices that we use to identify ImGui contexts.
// Editor and standalone games have context index 0 while PIE worlds have indices starting from 1 for server and 2+ for
// clients.

namespace Utilities
{
	// Invalid context index for parameters that cannot be resolved to a valid world.
	static constexpr int32 INVALID_CONTEXT_INDEX = -1;

	// Standalone context index.
	static constexpr int32 STANDALONE_GAME_CONTEXT_INDEX = 0;

	template<typename T>
	constexpr int32 GetWorldContextIndex(const T&)
	{
		// The only option is standalone game with one context.
		return STANDALONE_GAME_CONTEXT_INDEX;
	}

}
