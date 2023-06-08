#pragma once

#include <map>

#include "MovesetConverters.hpp"

#include "GameTypes.h"

namespace Aliases
{
	void ApplyPropertyAlias(unsigned int& id, unsigned int& value, const std::map<unsigned int, s_propAlias>& g_propertyAliases)
	{
		auto item = g_propertyAliases.find(id);
		if (item == g_propertyAliases.end()) return;

		id = item->second.target_id;

		auto value_item = item->second.param_alias.find(value);
		if (value_item != item->second.param_alias.end()) {
			value = value_item->second;
		}

	}

	void BuildAliasDictionary(std::map<unsigned int, s_propAlias>& g_propertyAliases)
	{
#ifdef BUILD_TYPE_DEBUG
		unsigned int filled = 0;
#endif

		if (g_propertyAliases.size() == 0) {
			DEBUG_ERR("Conversion: no alias to apply");
			return;
		}

		std::map<unsigned int, s_propAlias> aliasesCopy(g_propertyAliases);

		auto iter = aliasesCopy.begin();
		auto nextiter = std::next(iter, 1);
		auto iter_count = aliasesCopy.size() - 1;

		for (unsigned int i = 0; i < iter_count; ++i, std::advance(iter, 1), std::advance(nextiter, 1))
		{
			auto key = iter->first;
			auto next_key = nextiter->first;

			g_propertyAliases[key] = iter->second;
			if (iter->second.nofill || nextiter->second.nofill) {
				continue;
			}

			unsigned int key_diff = next_key - (key + 1);
			auto alias_offset = iter->second.target_id - key;
			auto alias_offset2 = nextiter->second.target_id - next_key;

			if (alias_offset == alias_offset2 and key_diff > 0) {
				for (unsigned int j = 1; j < key_diff + 1; ++j) {
#ifdef BUILD_TYPE_DEBUG
					++filled;
#endif
					g_propertyAliases[key + j] = {
						.target_id = iter->second.target_id + j
					};
				}
			}
		}
		
		g_propertyAliases[iter->first] = iter->second;
		DEBUG_LOG("Built alias dictionary: %u extra items (%llu total)\n", filled, g_propertyAliases.size());
	}

};