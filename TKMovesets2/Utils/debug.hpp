#pragma once

#ifdef BUILD_TYPE_DEBUG
#include <stdio.h>
#define DEBUG_LOG(...) printf(__VA_ARGS__)
#define DEBUG_LOGN(...) (printf( __VA_ARGS__), printf("\n"))
#define DEBUG_ERR(...) (fprintf(stderr, "!! Error: "), fprintf(stderr, __VA_ARGS__), fprintf(stderr, " !!\n"))
#define DEBUG_LAST_ERR() { auto _err = GetLastError(); printf("GetLastError(): %d / 0x%x\n", _err, _err); }
#define DEBUG_LAST_GL_ERR() { auto _err = glGetError(); _err == GL_NO_ERROR ? true : printf("glGetError(): %d / 0x%x\n", _err, _err); }
#else
#define DEBUG_LOG(...) 
#define DEBUG_LOGN(...) 
#define DEBUG_ERR(...) 
#define DEBUG_LAST_ERR()
#endif