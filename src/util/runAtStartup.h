#include "preprocessors.h"

#define runAtStartup(code) \
namespace { \
	void CONCAT(__garbageFunc_, __LINE__) () { code } \
	int CONCAT(__garbageVariable_, __LINE__) = (CONCAT(__garbageFunc_, __LINE__)(), 0); \
}
