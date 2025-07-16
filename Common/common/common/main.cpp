#include <iostream>
#include <string.h>

#ifdef __cplusplus
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#else
extern "C"
{
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}
#endif

const char* lua_script = R"(
print(_VERSION)
)";

void GetCurrentLuaVersion()
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	/*lua_getglobal(L, "version");
	const char* version = lua_tostring(L, -1);
	std::cout << version << std::endl;
	lua_pop(L, 1);*/
	luaL_dostring(L, lua_script);
}

int main(int argc, char** argv)
{
	GetCurrentLuaVersion();
	return 0;
}