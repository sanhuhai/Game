#include "LuaTest.h"

LuaTest::LuaTest()
{
}

LuaTest::LuaTest(const std::string &name)
{
	m_luaState = luaL_newstate();
	luaL_openlibs(m_luaState);
	m_name = name;
	Init();
}

LuaTest::~LuaTest()
{
	//lua_close(m_luaState);
	m_luaState = nullptr;
}

void LuaTest::Init()
{
	m_name = "LuaTest";
}

LuaTestChild::LuaTestChild()
{
}

LuaTestChild::~LuaTestChild()
{
}

int LuaTestChild::GetAge() const
{
	return m_age;
}

void LuaTestChild::SetAge(int age)
{
	m_age = age;
}

void LuaTestChild::Init()
{
	m_age = 0;
}
