#pragma once
#include <cstdio>
#include <cstdlib>
inline void testAssertFail(const char* expression,int line){
 std::fprintf(stderr,"FAILED line %d: %s\n",line,expression);std::fflush(stderr);std::exit(3);
}
#undef assert
#define assert(expression) ((expression)?void(0):testAssertFail(#expression,__LINE__))
