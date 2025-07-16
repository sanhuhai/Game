#pragma once
#include <string>
#include <iostream>


#ifdef __cplusplus
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#else
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#endif // __cplusplus
#include "LuaBridge/LuaBridge.h"

class TestLua
{
	// contructors and destructors
public:
	TestLua();
	virtual ~TestLua();
	// public methods
public:

	// private methods
private:

	// private data
private:
};

