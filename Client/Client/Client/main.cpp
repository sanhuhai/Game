#include <iostream>
#include <fstream>
#include <array>

#include "LuaTest.h"

void ReadFile(const std::string& filename)
{
	std::ifstream ifs;
	ifs.open(filename, std::ios::in | std::ios::binary);
	std::string ss;
	while (getline(ifs, ss))
	{
		std::cout << ss << std::endl;
	}
	ifs.close();
}

int average(lua_State* L)
{
	int n = lua_gettop(L);  // number of arguments
	int sum = 0;
	for (int index = 0; index <= n; ++index)
	{
		sum += lua_tonumber(L, index);
	}
	lua_pushnumber(L, sum / n);  // calculate the average
	lua_pushnumber(L, sum);
	return 2;  // return the average
}

void testLua()
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	//luaL_dostring(l, R"(print("Hello world"))");
	lua_register(L, "average", average);
	luaL_dofile(L, R"(../../luacode/test.lua)");

	lua_close(L);
}

void testCore() {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	std::string str = R"(
		-- require("socket.core");
		-- require("socket.core")
		print("lua begin");
		local sum,aver = average(10,20,30,40,50)
		print("sum",sum)
		print("average",aver)
	)";
	luaL_dostring(L, str.c_str());
	lua_close(L);
}

void testLuaBridge()
{
	std::cout << "Testing LuaBridge..." << std::endl;
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	luabridge::getGlobalNamespace(L)
		.beginNamespace("client")
			.beginClass<LuaTest>("TestLua")
		.addConstructor<void(*)(void)>()
		.addConstructor<void(*)(const std::string&)>()
		.endClass()

		.deriveClass<LuaTestChild, LuaTest>("TestLuaChild")
			.addConstructor<void(*)()>()
			.addFunction("GetAge", &LuaTestChild::GetAge)
			.addFunction("SetAge", &LuaTestChild::SetAge)
		.endClass()
		.endNamespace();
	LuaTestChild* test = new LuaTestChild();
	test->SetAge(25);
	lua_close(L);
}

int main(int argc, char* argv[])
{
	//std::cout << "This is a placeholder for the main function." << std::endl;
	//std::cout << "You can implement your logic here." << std::endl;
	testLuaBridge();
	return 0;
}