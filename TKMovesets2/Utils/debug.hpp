#pragma once

#ifdef BUILD_TYPE_DEBUG
#define DEBUG_LOG(...) printf(__VA_ARGS__)
#define DEBUG_LAST_ERR() { auto _err = GetLastError(); printf("GetLastError(): %d / 0x%x\n", _err, _err); }
#else
#define DEBUG_LOG(...) 
#define DEBUG_LAST_ERR()
#endif