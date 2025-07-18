// https://github.com/vinniefalco/LuaBridge
// Copyright 2019, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// Copyright 2007, Nathan Reed
// SPDX-License-Identifier: MIT

#pragma once

#include "ClassInfo.h"

#include <iostream>
#include <string>

namespace luabridge {

	namespace debug {

		inline void putIndent(std::ostream& stream, unsigned level)
		{
			for (unsigned i = 0; i < level; ++i)
			{
				stream << "  ";
			}
		}

		inline void
			dumpTable(lua_State* L, int index, std::ostream& stream, unsigned level, unsigned maxDepth);

		inline void
			dumpValue(lua_State* L, int index, std::ostream& stream, unsigned maxDepth = 1, unsigned level = 0)
		{
			const int type = lua_type(L, index);
			switch (type)
			{
			case LUA_TNIL:
				stream << "nil";
				break;

			case LUA_TBOOLEAN:
				stream << (lua_toboolean(L, index) ? "true" : "false");
				break;

			case LUA_TNUMBER:
				stream << lua_tonumber(L, index);
				break;

			case LUA_TSTRING:
				stream << '"' << lua_tostring(L, index) << '"';
				break;

			case LUA_TFUNCTION:
				if (lua_iscfunction(L, index))
				{
					stream << "cfunction@" << lua_topointer(L, index);
				}
				else
				{
					stream << "function@" << lua_topointer(L, index);
				}
				break;

			case LUA_TTHREAD:
				stream << "thread@" << lua_tothread(L, index);
				break;

			case LUA_TLIGHTUSERDATA:
				stream << "lightuserdata@" << lua_touserdata(L, index);
				break;

			case LUA_TTABLE:
				dumpTable(L, index, stream, level, maxDepth);
				break;

			case LUA_TUSERDATA:
				stream << "userdata@" << lua_touserdata(L, index);
				break;

			default:
				stream << lua_typename(L, type);
				;
				break;
			}
		}

		inline void
			dumpTable(lua_State* L, int index, std::ostream& stream, unsigned level, unsigned maxDepth)
		{
			stream << "table@" << lua_topointer(L, index);

			if (level > maxDepth)
			{
				return;
			}

			index = lua_absindex(L, index);
			stream << " {";
			lua_pushnil(L); // Initial key
			while (lua_next(L, index))
			{
				stream << "\n";
				putIndent(stream, level + 1);
				dumpValue(L, -2, stream, maxDepth, level + 1); // Key
				stream << ": ";
				dumpValue(L, -1, stream, maxDepth, level + 1); // Value
				lua_pop(L, 1); // Value
			}
			stream << "\n";
			putIndent(stream, level);
			stream << "}";
		}

		inline void dumpState(lua_State* L, std::ostream& stream = std::cerr, unsigned maxDepth = 1)
		{
			int top = lua_gettop(L);
			for (int i = 1; i <= top; ++i)
			{
				stream << "stack #" << i << ": ";
				dumpValue(L, i, stream, maxDepth, 0);
				stream << "\n";
			}
		}

	} // namespace debug

} // namespace luabridge
