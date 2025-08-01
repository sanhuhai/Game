/*
** $Id: lauxlib.c $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/

#define lauxlib_c
#define LUA_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Lua.
** Any function declared here could be written as an application function.
*/

#include "lua.h"

#include "lauxlib.h"


#if !defined(MAX_SIZET)
/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))
#endif


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield(lua_State* L, int objidx, int level) {
	if (level == 0 || !lua_istable(L, -1))
		return 0;  /* not found */
	lua_pushnil(L);  /* start 'next' loop */
	while (lua_next(L, -2)) {  /* for each pair in table */
		if (lua_type(L, -2) == LUA_TSTRING) {  /* ignore non-string keys */
			if (lua_rawequal(L, objidx, -1)) {  /* found object? */
				lua_pop(L, 1);  /* remove value (but keep name) */
				return 1;
			}
			else if (findfield(L, objidx, level - 1)) {  /* try recursively */
				/* stack: lib_name, lib_table, field_name (top) */
				lua_pushliteral(L, ".");  /* place '.' between the two names */
				lua_replace(L, -3);  /* (in the slot occupied by table) */
				lua_concat(L, 3);  /* lib_name.field_name */
				return 1;
			}
		}
		lua_pop(L, 1);  /* remove value */
	}
	return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname(lua_State* L, lua_Debug* ar) {
	int top = lua_gettop(L);
	lua_getinfo(L, "f", ar);  /* push function */
	lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
	luaL_checkstack(L, 6, "not enough stack");  /* slots for 'findfield' */
	if (findfield(L, top + 1, 2)) {
		const char* name = lua_tostring(L, -1);
		if (strncmp(name, LUA_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
			lua_pushstring(L, name + 3);  /* push name without prefix */
			lua_remove(L, -2);  /* remove original name */
		}
		lua_copy(L, -1, top + 1);  /* copy name to proper place */
		lua_settop(L, top + 1);  /* remove table "loaded" and name copy */
		return 1;
	}
	else {
		lua_settop(L, top);  /* remove function and global table */
		return 0;
	}
}


static void pushfuncname(lua_State* L, lua_Debug* ar) {
	if (pushglobalfuncname(L, ar)) {  /* try first a global name */
		lua_pushfstring(L, "function '%s'", lua_tostring(L, -1));
		lua_remove(L, -2);  /* remove name */
	}
	else if (*ar->namewhat != '\0')  /* is there a name from code? */
		lua_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
	else if (*ar->what == 'm')  /* main? */
		lua_pushliteral(L, "main chunk");
	else if (*ar->what != 'C')  /* for Lua functions, use <file:line> */
		lua_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
	else  /* nothing left... */
		lua_pushliteral(L, "?");
}


static int lastlevel(lua_State* L) {
	lua_Debug ar;
	int li = 1, le = 1;
	/* find an upper bound */
	while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
	/* do a binary search */
	while (li < le) {
		int m = (li + le) / 2;
		if (lua_getstack(L, m, &ar)) li = m + 1;
		else le = m;
	}
	return le - 1;
}


LUALIB_API void luaL_traceback(lua_State* L, lua_State* L1,
	const char* msg, int level) {
	luaL_Buffer b;
	lua_Debug ar;
	int last = lastlevel(L1);
	int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
	luaL_buffinit(L, &b);
	if (msg) {
		luaL_addstring(&b, msg);
		luaL_addchar(&b, '\n');
	}
	luaL_addstring(&b, "stack traceback:");
	while (lua_getstack(L1, level++, &ar)) {
		if (limit2show-- == 0) {  /* too many levels? */
			int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
			lua_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
			luaL_addvalue(&b);  /* add warning about skip */
			level += n;  /* and skip to last levels */
		}
		else {
			lua_getinfo(L1, "Slnt", &ar);
			if (ar.currentline <= 0)
				lua_pushfstring(L, "\n\t%s: in ", ar.short_src);
			else
				lua_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
			luaL_addvalue(&b);
			pushfuncname(L, &ar);
			luaL_addvalue(&b);
			if (ar.istailcall)
				luaL_addstring(&b, "\n\t(...tail calls...)");
		}
	}
	luaL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

LUALIB_API int luaL_argerror(lua_State* L, int arg, const char* extramsg) {
	lua_Debug ar;
	if (!lua_getstack(L, 0, &ar))  /* no stack frame? */
		return luaL_error(L, "bad argument #%d (%s)", arg, extramsg);
	lua_getinfo(L, "n", &ar);
	if (strcmp(ar.namewhat, "method") == 0) {
		arg--;  /* do not count 'self' */
		if (arg == 0)  /* error is in the self argument itself? */
			return luaL_error(L, "calling '%s' on bad self (%s)",
				ar.name, extramsg);
	}
	if (ar.name == NULL)
		ar.name = (pushglobalfuncname(L, &ar)) ? lua_tostring(L, -1) : "?";
	return luaL_error(L, "bad argument #%d to '%s' (%s)",
		arg, ar.name, extramsg);
}


LUALIB_API int luaL_typeerror(lua_State* L, int arg, const char* tname) {
	const char* msg;
	const char* typearg;  /* name for the type of the actual argument */
	if (luaL_getmetafield(L, arg, "__name") == LUA_TSTRING)
		typearg = lua_tostring(L, -1);  /* use the given type name */
	else if (lua_type(L, arg) == LUA_TLIGHTUSERDATA)
		typearg = "light userdata";  /* special name for messages */
	else
		typearg = luaL_typename(L, arg);  /* standard name */
	msg = lua_pushfstring(L, "%s expected, got %s", tname, typearg);
	return luaL_argerror(L, arg, msg);
}


static void tag_error(lua_State* L, int arg, int tag) {
	luaL_typeerror(L, arg, lua_typename(L, tag));
}


/*
** The use of 'lua_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
LUALIB_API void luaL_where(lua_State* L, int level) {
	lua_Debug ar;
	if (lua_getstack(L, level, &ar)) {  /* check function at level */
		lua_getinfo(L, "Sl", &ar);  /* get info about it */
		if (ar.currentline > 0) {  /* is there info? */
			lua_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
			return;
		}
	}
	lua_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'lua_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
LUALIB_API int luaL_error(lua_State* L, const char* fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	luaL_where(L, 1);
	lua_pushvfstring(L, fmt, argp);
	va_end(argp);
	lua_concat(L, 2);
	return lua_error(L);
}


LUALIB_API int luaL_fileresult(lua_State* L, int stat, const char* fname) {
	int en = errno;  /* calls to Lua API may change this value */
	if (stat) {
		lua_pushboolean(L, 1);
		return 1;
	}
	else {
		const char* msg;
		luaL_pushfail(L);
		msg = (en != 0) ? strerror(en) : "(no extra info)";
		if (fname)
			lua_pushfstring(L, "%s: %s", fname, msg);
		else
			lua_pushstring(L, msg);
		lua_pushinteger(L, en);
		return 3;
	}
}


#if !defined(l_inspectstat)	/* { */

#if defined(LUA_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


LUALIB_API int luaL_execresult(lua_State* L, int stat) {
	if (stat != 0 && errno != 0)  /* error with an 'errno'? */
		return luaL_fileresult(L, 0, NULL);
	else {
		const char* what = "exit";  /* type of termination */
		l_inspectstat(stat, what);  /* interpret result */
		if (*what == 'e' && stat == 0)  /* successful termination? */
			lua_pushboolean(L, 1);
		else
			luaL_pushfail(L);
		lua_pushstring(L, what);
		lua_pushinteger(L, stat);
		return 3;  /* return true/fail,what,code */
	}
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

LUALIB_API int luaL_newmetatable(lua_State* L, const char* tname) {
	if (luaL_getmetatable(L, tname) != LUA_TNIL)  /* name already in use? */
		return 0;  /* leave previous value on top, but return 0 */
	lua_pop(L, 1);
	lua_createtable(L, 0, 2);  /* create metatable */
	lua_pushstring(L, tname);
	lua_setfield(L, -2, "__name");  /* metatable.__name = tname */
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, tname);  /* registry.name = metatable */
	return 1;
}


LUALIB_API void luaL_setmetatable(lua_State* L, const char* tname) {
	luaL_getmetatable(L, tname);
	lua_setmetatable(L, -2);
}


LUALIB_API void* luaL_testudata(lua_State* L, int ud, const char* tname) {
	void* p = lua_touserdata(L, ud);
	if (p != NULL) {  /* value is a userdata? */
		if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
			luaL_getmetatable(L, tname);  /* get correct metatable */
			if (!lua_rawequal(L, -1, -2))  /* not the same? */
				p = NULL;  /* value is a userdata with wrong metatable */
			lua_pop(L, 2);  /* remove both metatables */
			return p;
		}
	}
	return NULL;  /* value is not a userdata with a metatable */
}


LUALIB_API void* luaL_checkudata(lua_State* L, int ud, const char* tname) {
	void* p = luaL_testudata(L, ud, tname);
	luaL_argexpected(L, p != NULL, ud, tname);
	return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

LUALIB_API int luaL_checkoption(lua_State* L, int arg, const char* def,
	const char* const lst[]) {
	const char* name = (def) ? luaL_optstring(L, arg, def) :
		luaL_checkstring(L, arg);
	int i;
	for (i = 0; lst[i]; i++)
		if (strcmp(lst[i], name) == 0)
			return i;
	return luaL_argerror(L, arg,
		lua_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Lua will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
LUALIB_API void luaL_checkstack(lua_State* L, int space, const char* msg) {
	if (l_unlikely(!lua_checkstack(L, space))) {
		if (msg)
			luaL_error(L, "stack overflow (%s)", msg);
		else
			luaL_error(L, "stack overflow");
	}
}


LUALIB_API void luaL_checktype(lua_State* L, int arg, int t) {
	if (l_unlikely(lua_type(L, arg) != t))
		tag_error(L, arg, t);
}


LUALIB_API void luaL_checkany(lua_State* L, int arg) {
	if (l_unlikely(lua_type(L, arg) == LUA_TNONE))
		luaL_argerror(L, arg, "value expected");
}


LUALIB_API const char* luaL_checklstring(lua_State* L, int arg, size_t* len) {
	const char* s = lua_tolstring(L, arg, len);
	if (l_unlikely(!s)) tag_error(L, arg, LUA_TSTRING);
	return s;
}


LUALIB_API const char* luaL_optlstring(lua_State* L, int arg,
	const char* def, size_t* len) {
	if (lua_isnoneornil(L, arg)) {
		if (len)
			*len = (def ? strlen(def) : 0);
		return def;
	}
	else return luaL_checklstring(L, arg, len);
}


LUALIB_API lua_Number luaL_checknumber(lua_State* L, int arg) {
	int isnum;
	lua_Number d = lua_tonumberx(L, arg, &isnum);
	if (l_unlikely(!isnum))
		tag_error(L, arg, LUA_TNUMBER);
	return d;
}


LUALIB_API lua_Number luaL_optnumber(lua_State* L, int arg, lua_Number def) {
	return luaL_opt(L, luaL_checknumber, arg, def);
}


static void interror(lua_State* L, int arg) {
	if (lua_isnumber(L, arg))
		luaL_argerror(L, arg, "number has no integer representation");
	else
		tag_error(L, arg, LUA_TNUMBER);
}


LUALIB_API lua_Integer luaL_checkinteger(lua_State* L, int arg) {
	int isnum;
	lua_Integer d = lua_tointegerx(L, arg, &isnum);
	if (l_unlikely(!isnum)) {
		interror(L, arg);
	}
	return d;
}


LUALIB_API lua_Integer luaL_optinteger(lua_State* L, int arg,
	lua_Integer def) {
	return luaL_opt(L, luaL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
	void* box;
	size_t bsize;
} UBox;


static void* resizebox(lua_State* L, int idx, size_t newsize) {
	void* ud;
	lua_Alloc allocf = lua_getallocf(L, &ud);
	UBox* box = (UBox*)lua_touserdata(L, idx);
	void* temp = allocf(ud, box->box, box->bsize, newsize);
	if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
		lua_pushliteral(L, "not enough memory");
		lua_error(L);  /* raise a memory error */
	}
	box->box = temp;
	box->bsize = newsize;
	return temp;
}


static int boxgc(lua_State* L) {
	resizebox(L, 1, 0);
	return 0;
}


static const luaL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox(lua_State* L) {
	UBox* box = (UBox*)lua_newuserdatauv(L, sizeof(UBox), 0);
	box->box = NULL;
	box->bsize = 0;
	if (luaL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
		luaL_setfuncs(L, boxmt, 0);  /* set its metamethods */
	lua_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Whenever buffer is accessed, slot 'idx' must either be a box (which
** cannot be NULL) or it is a placeholder for the buffer.
*/
#define checkbufferlevel(B,idx)  \
  lua_assert(buffonstack(B) ? lua_touserdata(B->L, idx) != NULL  \
                            : lua_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes. (The test for "not big enough" also gets the case when the
** computation of 'newsize' overflows.)
*/
static size_t newbuffsize(luaL_Buffer* B, size_t sz) {
	size_t newsize = (B->size / 2) * 3;  /* buffer size * 1.5 */
	if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
		return luaL_error(B->L, "buffer too large");
	if (newsize < B->n + sz)  /* not big enough? */
		newsize = B->n + sz;
	return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char* prepbuffsize(luaL_Buffer* B, size_t sz, int boxidx) {
	checkbufferlevel(B, boxidx);
	if (B->size - B->n >= sz)  /* enough space? */
		return B->b + B->n;
	else {
		lua_State* L = B->L;
		char* newbuff;
		size_t newsize = newbuffsize(B, sz);
		/* create larger buffer */
		if (buffonstack(B))  /* buffer already has a box? */
			newbuff = (char*)resizebox(L, boxidx, newsize);  /* resize it */
		else {  /* no box yet */
			lua_remove(L, boxidx);  /* remove placeholder */
			newbox(L);  /* create a new box */
			lua_insert(L, boxidx);  /* move box to its intended position */
			lua_toclose(L, boxidx);
			newbuff = (char*)resizebox(L, boxidx, newsize);
			memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
		}
		B->b = newbuff;
		B->size = newsize;
		return newbuff + B->n;
	}
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
LUALIB_API char* luaL_prepbuffsize(luaL_Buffer* B, size_t sz) {
	return prepbuffsize(B, sz, -1);
}


LUALIB_API void luaL_addlstring(luaL_Buffer* B, const char* s, size_t l) {
	if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
		char* b = prepbuffsize(B, l, -1);
		memcpy(b, s, l * sizeof(char));
		luaL_addsize(B, l);
	}
}


LUALIB_API void luaL_addstring(luaL_Buffer* B, const char* s) {
	luaL_addlstring(B, s, strlen(s));
}


LUALIB_API void luaL_pushresult(luaL_Buffer* B) {
	lua_State* L = B->L;
	checkbufferlevel(B, -1);
	lua_pushlstring(L, B->b, B->n);
	if (buffonstack(B))
		lua_closeslot(L, -2);  /* close the box */
	lua_remove(L, -2);  /* remove box or placeholder from the stack */
}


LUALIB_API void luaL_pushresultsize(luaL_Buffer* B, size_t sz) {
	luaL_addsize(B, sz);
	luaL_pushresult(B);
}


/*
** 'luaL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'luaL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) below the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
LUALIB_API void luaL_addvalue(luaL_Buffer* B) {
	lua_State* L = B->L;
	size_t len;
	const char* s = lua_tolstring(L, -1, &len);
	char* b = prepbuffsize(B, len, -2);
	memcpy(b, s, len * sizeof(char));
	luaL_addsize(B, len);
	lua_pop(L, 1);  /* pop string */
}


LUALIB_API void luaL_buffinit(lua_State* L, luaL_Buffer* B) {
	B->L = L;
	B->b = B->init.b;
	B->n = 0;
	B->size = LUAL_BUFFERSIZE;
	lua_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


LUALIB_API char* luaL_buffinitsize(lua_State* L, luaL_Buffer* B, size_t sz) {
	luaL_buffinit(L, B);
	return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(LUA_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
LUALIB_API int luaL_ref(lua_State* L, int t) {
	int ref;
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);  /* remove from stack */
		return LUA_REFNIL;  /* 'nil' has a unique fixed reference */
	}
	t = lua_absindex(L, t);
	if (lua_rawgeti(L, t, freelist) == LUA_TNIL) {  /* first access? */
		ref = 0;  /* list is empty */
		lua_pushinteger(L, 0);  /* initialize as an empty list */
		lua_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
	}
	else {  /* already initialized */
		lua_assert(lua_isinteger(L, -1));
		ref = (int)lua_tointeger(L, -1);  /* ref = t[freelist] */
	}
	lua_pop(L, 1);  /* remove element from stack */
	if (ref != 0) {  /* any free element? */
		lua_rawgeti(L, t, ref);  /* remove it from list */
		lua_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
	}
	else  /* no free elements */
		ref = (int)lua_rawlen(L, t) + 1;  /* get a new reference */
	lua_rawseti(L, t, ref);
	return ref;
}


LUALIB_API void luaL_unref(lua_State* L, int t, int ref) {
	if (ref >= 0) {
		t = lua_absindex(L, t);
		lua_rawgeti(L, t, freelist);
		lua_assert(lua_isinteger(L, -1));
		lua_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
		lua_pushinteger(L, ref);
		lua_rawseti(L, t, freelist);  /* t[freelist] = ref */
	}
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
	int n;  /* number of pre-read characters */
	FILE* f;  /* file being read */
	char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char* getF(lua_State* L, void* ud, size_t* size) {
	LoadF* lf = (LoadF*)ud;
	(void)L;  /* not used */
	if (lf->n > 0) {  /* are there pre-read characters to be read? */
		*size = lf->n;  /* return them (chars already in buffer) */
		lf->n = 0;  /* no more pre-read characters */
	}
	else {  /* read a block from file */
		/* 'fread' can return > 0 *and* set the EOF flag. If next call to
		   'getF' called 'fread', it might still wait for user input.
		   The next check avoids this problem. */
		if (feof(lf->f)) return NULL;
		*size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
	}
	return lf->buff;
}


static int errfile(lua_State* L, const char* what, int fnameindex) {
	int err = errno;
	const char* filename = lua_tostring(L, fnameindex) + 1;
	if (err != 0)
		lua_pushfstring(L, "cannot %s %s: %s", what, filename, strerror(err));
	else
		lua_pushfstring(L, "cannot %s %s", what, filename);
	lua_remove(L, fnameindex);
	return LUA_ERRFILE;
}


/*
** Skip an optional BOM at the start of a stream. If there is an
** incomplete BOM (the first character is correct but the rest is
** not), returns the first character anyway to force an error
** (as no chunk can start with 0xEF).
*/
static int skipBOM(FILE* f) {
	int c = getc(f);  /* read first character */
	if (c == 0xEF && getc(f) == 0xBB && getc(f) == 0xBF)  /* correct BOM? */
		return getc(f);  /* ignore BOM and return next char */
	else  /* no (valid) BOM */
		return c;  /* return first character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment(FILE* f, int* cp) {
	int c = *cp = skipBOM(f);
	if (c == '#') {  /* first line is a comment (Unix exec. file)? */
		do {  /* skip first line */
			c = getc(f);
		} while (c != EOF && c != '\n');
		*cp = getc(f);  /* next character after comment, if present */
		return 1;  /* there was a comment */
	}
	else return 0;  /* no comment */
}


LUALIB_API int luaL_loadfilex(lua_State* L, const char* filename,
	const char* mode) {
	LoadF lf;
	int status, readstatus;
	int c;
	int fnameindex = lua_gettop(L) + 1;  /* index of filename on the stack */
	if (filename == NULL) {
		lua_pushliteral(L, "=stdin");
		lf.f = stdin;
	}
	else {
		lua_pushfstring(L, "@%s", filename);
		errno = 0;
		lf.f = fopen(filename, "r");
		if (lf.f == NULL) return errfile(L, "open", fnameindex);
	}
	lf.n = 0;
	if (skipcomment(lf.f, &c))  /* read initial portion */
		lf.buff[lf.n++] = '\n';  /* add newline to correct line numbers */
	if (c == LUA_SIGNATURE[0]) {  /* binary file? */
		lf.n = 0;  /* remove possible newline */
		if (filename) {  /* "real" file? */
			errno = 0;
			lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
			if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
			skipcomment(lf.f, &c);  /* re-read initial portion */
		}
	}
	if (c != EOF)
		lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
	errno = 0;
	status = lua_load(L, getF, &lf, lua_tostring(L, -1), mode);
	readstatus = ferror(lf.f);
	if (filename) fclose(lf.f);  /* close file (even in case of errors) */
	if (readstatus) {
		lua_settop(L, fnameindex);  /* ignore results from 'lua_load' */
		return errfile(L, "read", fnameindex);
	}
	lua_remove(L, fnameindex);
	return status;
}


typedef struct LoadS {
	const char* s;
	size_t size;
} LoadS;


static const char* getS(lua_State* L, void* ud, size_t* size) {
	LoadS* ls = (LoadS*)ud;
	(void)L;  /* not used */
	if (ls->size == 0) return NULL;
	*size = ls->size;
	ls->size = 0;
	return ls->s;
}


LUALIB_API int luaL_loadbufferx(lua_State* L, const char* buff, size_t size,
	const char* name, const char* mode) {
	LoadS ls;
	ls.s = buff;
	ls.size = size;
	return lua_load(L, getS, &ls, name, mode);
}


LUALIB_API int luaL_loadstring(lua_State* L, const char* s) {
	return luaL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



LUALIB_API int luaL_getmetafield(lua_State* L, int obj, const char* event) {
	if (!lua_getmetatable(L, obj))  /* no metatable? */
		return LUA_TNIL;
	else {
		int tt;
		lua_pushstring(L, event);
		tt = lua_rawget(L, -2);
		if (tt == LUA_TNIL)  /* is metafield nil? */
			lua_pop(L, 2);  /* remove metatable and metafield */
		else
			lua_remove(L, -2);  /* remove only metatable */
		return tt;  /* return metafield type */
	}
}


LUALIB_API int luaL_callmeta(lua_State* L, int obj, const char* event) {
	obj = lua_absindex(L, obj);
	if (luaL_getmetafield(L, obj, event) == LUA_TNIL)  /* no metafield? */
		return 0;
	lua_pushvalue(L, obj);
	lua_call(L, 1, 1);
	return 1;
}


LUALIB_API lua_Integer luaL_len(lua_State* L, int idx) {
	lua_Integer l;
	int isnum;
	lua_len(L, idx);
	l = lua_tointegerx(L, -1, &isnum);
	if (l_unlikely(!isnum))
		luaL_error(L, "object length is not an integer");
	lua_pop(L, 1);  /* remove object */
	return l;
}


LUALIB_API const char* luaL_tolstring(lua_State* L, int idx, size_t* len) {
	idx = lua_absindex(L, idx);
	if (luaL_callmeta(L, idx, "__tostring")) {  /* metafield? */
		if (!lua_isstring(L, -1))
			luaL_error(L, "'__tostring' must return a string");
	}
	else {
		switch (lua_type(L, idx)) {
		case LUA_TNUMBER: {
			if (lua_isinteger(L, idx))
				lua_pushfstring(L, "%I", (LUAI_UACINT)lua_tointeger(L, idx));
			else
				lua_pushfstring(L, "%f", (LUAI_UACNUMBER)lua_tonumber(L, idx));
			break;
		}
		case LUA_TSTRING:
			lua_pushvalue(L, idx);
			break;
		case LUA_TBOOLEAN:
			lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
			break;
		case LUA_TNIL:
			lua_pushliteral(L, "nil");
			break;
		default: {
			int tt = luaL_getmetafield(L, idx, "__name");  /* try name */
			const char* kind = (tt == LUA_TSTRING) ? lua_tostring(L, -1) :
				luaL_typename(L, idx);
			lua_pushfstring(L, "%s: %p", kind, lua_topointer(L, idx));
			if (tt != LUA_TNIL)
				lua_remove(L, -2);  /* remove '__name' */
			break;
		}
		}
	}
	return lua_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
LUALIB_API void luaL_setfuncs(lua_State* L, const luaL_Reg* l, int nup) {
	luaL_checkstack(L, nup, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		if (l->func == NULL)  /* placeholder? */
			lua_pushboolean(L, 0);
		else {
			int i;
			for (i = 0; i < nup; i++)  /* copy upvalues to the top */
				lua_pushvalue(L, -nup);
			lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		}
		lua_setfield(L, -(nup + 2), l->name);
	}
	lua_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
LUALIB_API int luaL_getsubtable(lua_State* L, int idx, const char* fname) {
	if (lua_getfield(L, idx, fname) == LUA_TTABLE)
		return 1;  /* table already there */
	else {
		lua_pop(L, 1);  /* remove previous result */
		idx = lua_absindex(L, idx);
		lua_newtable(L);
		lua_pushvalue(L, -1);  /* copy to be left at top */
		lua_setfield(L, idx, fname);  /* assign new table to field */
		return 0;  /* false, because did not find table there */
	}
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
LUALIB_API void luaL_requiref(lua_State* L, const char* modname,
	lua_CFunction openf, int glb) {
	luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
	lua_getfield(L, -1, modname);  /* LOADED[modname] */
	if (!lua_toboolean(L, -1)) {  /* package not already loaded? */
		lua_pop(L, 1);  /* remove field */
		lua_pushcfunction(L, openf);
		lua_pushstring(L, modname);  /* argument to open function */
		lua_call(L, 1, 1);  /* call 'openf' to open module */
		lua_pushvalue(L, -1);  /* make copy of module (call result) */
		lua_setfield(L, -3, modname);  /* LOADED[modname] = module */
	}
	lua_remove(L, -2);  /* remove LOADED table */
	if (glb) {
		lua_pushvalue(L, -1);  /* copy of module */
		lua_setglobal(L, modname);  /* _G[modname] = module */
	}
}


LUALIB_API void luaL_addgsub(luaL_Buffer* b, const char* s,
	const char* p, const char* r) {
	const char* wild;
	size_t l = strlen(p);
	while ((wild = strstr(s, p)) != NULL) {
		luaL_addlstring(b, s, wild - s);  /* push prefix */
		luaL_addstring(b, r);  /* push replacement in place of pattern */
		s = wild + l;  /* continue after 'p' */
	}
	luaL_addstring(b, s);  /* push last suffix */
}


LUALIB_API const char* luaL_gsub(lua_State* L, const char* s,
	const char* p, const char* r) {
	luaL_Buffer b;
	luaL_buffinit(L, &b);
	luaL_addgsub(&b, s, p, r);
	luaL_pushresult(&b);
	return lua_tostring(L, -1);
}


static void* l_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
	(void)ud; (void)osize;  /* not used */
	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	else
		return realloc(ptr, nsize);
}


/*
** Standard panic funcion just prints an error message. The test
** with 'lua_type' avoids possible memory errors in 'lua_tostring'.
*/
static int panic(lua_State* L) {
	const char* msg = (lua_type(L, -1) == LUA_TSTRING)
		? lua_tostring(L, -1)
		: "error object is not a string";
	lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
		msg);
	return 0;  /* return to Lua to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff(void* ud, const char* message, int tocont);
static void warnfon(void* ud, const char* message, int tocont);
static void warnfcont(void* ud, const char* message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol(lua_State* L, const char* message, int tocont) {
	if (tocont || *(message++) != '@')  /* not a control message? */
		return 0;
	else {
		if (strcmp(message, "off") == 0)
			lua_setwarnf(L, warnfoff, L);  /* turn warnings off */
		else if (strcmp(message, "on") == 0)
			lua_setwarnf(L, warnfon, L);   /* turn warnings on */
		return 1;  /* it was a control message */
	}
}


static void warnfoff(void* ud, const char* message, int tocont) {
	checkcontrol((lua_State*)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont(void* ud, const char* message, int tocont) {
	lua_State* L = (lua_State*)ud;
	lua_writestringerror("%s", message);  /* write message */
	if (tocont)  /* not the last part? */
		lua_setwarnf(L, warnfcont, L);  /* to be continued */
	else {  /* last part */
		lua_writestringerror("%s", "\n");  /* finish message with end-of-line */
		lua_setwarnf(L, warnfon, L);  /* next call is a new message */
	}
}


static void warnfon(void* ud, const char* message, int tocont) {
	if (checkcontrol((lua_State*)ud, message, tocont))  /* control message? */
		return;  /* nothing else to be done */
	lua_writestringerror("%s", "Lua warning: ");  /* start a new warning */
	warnfcont(ud, message, tocont);  /* finish processing */
}


LUALIB_API lua_State* luaL_newstate(void) {
	lua_State* L = lua_newstate(l_alloc, NULL);
	if (l_likely(L)) {
		lua_atpanic(L, &panic);
		lua_setwarnf(L, warnfoff, L);  /* default is warnings off */
	}
	return L;
}


LUALIB_API void luaL_checkversion_(lua_State* L, lua_Number ver, size_t sz) {
	lua_Number v = lua_version(L);
	if (sz != LUAL_NUMSIZES)  /* check numeric types */
		luaL_error(L, "core and library have incompatible numeric types");
	else if (v != ver)
		luaL_error(L, "version mismatch: app. needs %f, Lua core provides %f",
			(LUAI_UACNUMBER)ver, (LUAI_UACNUMBER)v);
}

