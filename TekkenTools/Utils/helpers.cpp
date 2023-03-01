#include "helpers.hpp"

#include <time.h>

namespace Helpers
{
	std::string currentDateTime() {
		time_t     now = time(0);
		struct tm  tstruct;
		char       buf[20];

		localtime_s(&tstruct, &now);
		strftime(buf, sizeof(buf), "%H:%M %d/%m/%Y", &tstruct);
		return std::string(buf);
	}
}
