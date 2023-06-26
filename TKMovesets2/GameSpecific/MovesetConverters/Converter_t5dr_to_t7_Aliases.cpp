#pragma once

#include <map>

#include "MovesetConverters.hpp"

#include "GameTypes.h"

// Aliases for extraprops andrequirements
// Will be built from cg_propertyAliases, attempts to guess unknown values in between known values
 std::map<unsigned int, s_propAlias> MovesetConverter::T5DRToT7::InitAliasDictionary()
{
	return std::map<unsigned int, s_propAlias> {

	};
}
