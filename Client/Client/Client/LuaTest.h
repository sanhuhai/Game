#pragma once
#include <iostream>

#ifdef __cplusplus
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#else
#extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#endif // __cplusplus
#include "LuaBridge/LuaBridge.h"

class LuaTest
{
public:
	LuaTest(const std::string&);
	LuaTest();
	virtual	~LuaTest();

	// public methods
public:

	// private methods
private:
	void Init();
	// private data
private:
	std::string m_name;
	lua_State* m_luaState = nullptr;
};

class LuaTestChild : public LuaTest
{
public:
	LuaTestChild();
	virtual~LuaTestChild();

public:
	int GetAge() const;
	void SetAge(int age);

private:
	void Init();

private:
	int m_age;
};

