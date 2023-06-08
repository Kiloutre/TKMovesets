#pragma once

#include <map>

#include "MovesetConverters.hpp"

#include "GameTypes.h"

namespace Aliases
{
	// Applies a move extraproperty alias
	void ApplyPropertyAlias(unsigned int& id, unsigned int& value, const std::map<unsigned int, s_propAlias>& g_propertyAliases);

	// Fills the requirement & extraproperties dictionary
	void BuildAliasDictionary(std::map<unsigned int, s_propAlias>& g_propertyAliases);
};