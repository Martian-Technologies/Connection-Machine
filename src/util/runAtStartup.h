#define CONCAT_(prefix, suffix) prefix ## suffix
#define CONCAT(prefix, suffix) CONCAT_(prefix, suffix)
#define runAtStartup(code) void CONCAT(__garbageFunc_, __LINE__) (){code}\
static int CONCAT(__garbageVariable_,  __LINE__) = (CONCAT(__garbageFunc_, __LINE__)(), 0);