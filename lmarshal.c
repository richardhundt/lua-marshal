/*
* lmarshal.c
* A Lua library for serializing and deserializing tables
* Richard Hundt <richardhundt@gmail.com>
*
* Provides:
* s = table.marshal(t)      - serializes a table to a byte stream
* t = table.unmarshal(s)    - deserializes a byte stream to a table
*
* Limitations:
* Coroutines are not serialized and nor are userdata, however support
* for userdata the __persist metatable hook can be used.
* 
* License: MIT
*
* Copyright (c) 2010 Richard Hundt
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following
* conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"


#define MAR_TREF 1
#define MAR_TVAL 2
#define MAR_TUSR 3

#define MAR_CHR 1
#define MAR_I32 4
#define MAR_I64 8

#define MAR_MAGIC 0x8e

typedef struct mar_Buffer {
    size_t size;
    size_t seek;
    size_t head;
    char*  data;
} mar_Buffer;

static int mar_pack(lua_State *L, mar_Buffer *buf, int idx);
static int mar_unpack(lua_State *L, const char* buf, size_t len, int idx);

static void buf_init(lua_State *L, mar_Buffer *buf)
{
    buf->size = 128;
    buf->seek = 0;
    buf->head = 0;
    if (!(buf->data = malloc(buf->size))) luaL_error(L, "Out of memory!");
}

static void buf_done(lua_State* L, mar_Buffer *buf)
{
    lua_pushlstring(L, buf->data, buf->head);
    free(buf->data);
}

static int buf_write(lua_State* L, const char* str, size_t len, mar_Buffer *buf)
{
    if (len > UINT32_MAX) luaL_error(L, "buffer too long");
    if (buf->size - buf->head < len) {
        size_t new_size = buf->size << 1;
        size_t cur_head = buf->head;
        while (new_size - cur_head <= len) {
            new_size = new_size << 1;
        }
        if (!(buf->data = realloc(buf->data, new_size))) {
            luaL_error(L, "Out of memory!");
        }
        buf->size = new_size;
    }
    memcpy(&buf->data[buf->head], str, len);
    buf->head += len;
    return 0;
}

static const char* buf_read(lua_State *L, mar_Buffer *buf, size_t *len)
{
    if (buf->seek < buf->head) {
        buf->seek = buf->head;
        *len = buf->seek;
        return buf->data;
    }
    *len = 0;
    return NULL;
}

static void pack_value(lua_State *L, mar_Buffer *buf, int val, int *idx)
{
    size_t l;
    int val_type = lua_type(L, val);
    buf_write(L, (void*)&val_type, MAR_CHR, buf);
    switch (val_type) {
    case LUA_TBOOLEAN: {
        int int_val = lua_toboolean(L, val);
        buf_write(L, (void*)&int_val, MAR_CHR, buf);
        break;
    }
    case LUA_TSTRING: {
        const char *str_val = lua_tolstring(L, val, &l);
        buf_write(L, (void*)&l, MAR_I32, buf);
        buf_write(L, str_val, l, buf);
        break;
    }
    case LUA_TNUMBER: {
        lua_Number num_val = lua_tonumber(L, val);
        buf_write(L, (void*)&num_val, MAR_I64, buf);
        break;
    }
    case LUA_TTABLE: {
        int tag, ref;
        lua_pushvalue(L, val);
        lua_rawget(L, 2);
        if (!lua_isnil(L, -1)) {
            ref = lua_tointeger(L, -1);
            tag = MAR_TREF;
            buf_write(L, (void*)&tag, MAR_CHR, buf);
            buf_write(L, (void*)&ref, MAR_I32, buf);
            lua_pop(L, 1);
        }
        else {
            mar_Buffer rec_buf;
            const char* rec_packed;
            size_t rec_l;
            lua_pop(L, 1);
            if (luaL_getmetafield(L, val, "__persist")) {
                tag = MAR_TUSR;

                lua_pushvalue(L, val-1);
                lua_pushinteger(L, (*idx)++);
                lua_rawset(L, 2);

                lua_pushvalue(L, val-1);
                lua_call(L, 1, 1);
                if (!lua_isfunction(L, -1)) {
                    luaL_error(L, "__persist must return a function");
                }
                lua_newtable(L);
                lua_pushvalue(L, -2);
                lua_rawseti(L, -2, 1);
                lua_remove(L, -2);

                buf_init(L, &rec_buf);
                mar_pack(L, &rec_buf, *idx);
                buf_done(L, &rec_buf);

                rec_packed = lua_tolstring(L, -1, &rec_l);
                buf_write(L, (void*)&tag, MAR_CHR, buf);
                buf_write(L, (void*)&rec_l, MAR_I32, buf);
                buf_write(L, rec_packed, rec_l, buf);
                lua_pop(L, 2);
            }
            else {
                tag = MAR_TVAL;

                lua_pushvalue(L, val);
                lua_pushinteger(L, (*idx)++);
                lua_rawset(L, 2);

                lua_pushvalue(L, val);
                buf_init(L, &rec_buf);
                mar_pack(L, &rec_buf, *idx);
                buf_done(L, &rec_buf);

                rec_packed = lua_tolstring(L, -1, &rec_l);
                buf_write(L, (void*)&tag, MAR_CHR, buf);
                buf_write(L, (void*)&rec_l, MAR_I32, buf);
                buf_write(L, rec_packed, rec_l, buf);
                lua_pop(L, 2);
            }
        }
        break;
    }
    case LUA_TFUNCTION: {
        int tag, ref;
        lua_pushvalue(L, val);
        lua_rawget(L, 2);
        if (!lua_isnil(L, -1)) {
            ref = lua_tointeger(L, -1);
            tag = MAR_TREF;
            buf_write(L, (void*)&tag, MAR_CHR, buf);
            buf_write(L, (void*)&ref, MAR_I32, buf);
            lua_pop(L, 1);
        }
        else {
            mar_Buffer rec_buf;
            const char* rec_packed;
            size_t rec_l;
            int i;
            lua_Debug ar;
            lua_pop(L, 1);

            tag = MAR_TVAL;
            lua_pushvalue(L, val);
            lua_pushinteger(L, (*idx)++);
            lua_rawset(L, 2);

            lua_pushvalue(L, val);
            buf_init(L, &rec_buf);
            lua_dump(L, (lua_Writer)buf_write, &rec_buf);
            buf_done(L, &rec_buf);
            rec_packed = lua_tolstring(L, -1, &rec_l);

            buf_write(L, (void*)&tag, MAR_CHR, buf);
            buf_write(L, (void*)&rec_l, MAR_I32, buf);
            buf_write(L, rec_packed, rec_l, buf);
            lua_pop(L, 2);

            lua_pushvalue(L, val);
            lua_getinfo(L, ">uS", &ar);
            if (ar.what[0] != 'L') {
                luaL_error(L, "attempt to persist a C function");
            }

            lua_newtable(L);
            for (i=1; i <= ar.nups; i++) {
                lua_getupvalue(L, -2, i);
                lua_rawseti(L, -2, i);
            }

            buf_init(L, &rec_buf);
            mar_pack(L, &rec_buf, *idx);
            buf_done(L, &rec_buf);
            rec_packed = lua_tolstring(L, -1, &rec_l);

            buf_write(L, (void*)&rec_l, MAR_I32, buf);
            buf_write(L, rec_packed, rec_l, buf);
            lua_pop(L, 2);
        }

        break;
    }
    case LUA_TUSERDATA: {
        int tag, ref;
        lua_pushvalue(L, val);
        lua_rawget(L, 2);
        if (!lua_isnil(L, -1)) {
            ref = lua_tointeger(L, -1);
            tag = MAR_TREF;
            buf_write(L, (void*)&tag, MAR_CHR, buf);
            buf_write(L, (void*)&ref, MAR_I32, buf);
            lua_pop(L, 1);
        }
        else {
            mar_Buffer rec_buf;
            const char* rec_packed;
            size_t rec_l;
            lua_pop(L, 1);
            if (luaL_getmetafield(L, val, "__persist")) {
                tag = MAR_TUSR;

                lua_pushvalue(L, val-1);
                lua_pushinteger(L, (*idx)++);
                lua_rawset(L, 2);

                lua_pushvalue(L, val-1);
                lua_call(L, 1, 1);
                if (!lua_isfunction(L, -1)) {
                    luaL_error(L, "__persist must return a function");
                }
                lua_newtable(L);
                lua_pushvalue(L, -2);
                lua_rawseti(L, -2, 1);
                lua_remove(L, -2);

                buf_init(L, &rec_buf);
                mar_pack(L, &rec_buf, *idx);
                buf_done(L, &rec_buf);

                rec_packed = lua_tolstring(L, -1, &rec_l);
                buf_write(L, (void*)&tag, MAR_CHR, buf);
                buf_write(L, (void*)&rec_l, MAR_I32, buf);
                buf_write(L, rec_packed, rec_l, buf);
                lua_pop(L, 2);
            }
            else {
                tag = MAR_TVAL;
                buf_write(L, (void*)&tag, MAR_CHR, buf);
            }
        }
        break;
    }
    case LUA_TTHREAD: break; /* just give them a nil during unpack */
    default:
        luaL_error(L, "invalid value type");
    }
}

static int mar_pack(lua_State *L, mar_Buffer *buf, int idx)
{
    /* size_t l; */
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        pack_value(L, buf, -2, &idx);
        pack_value(L, buf, -1, &idx);
        lua_pop(L, 1);
    }
    return 1;
}

#define mar_incr_ptr(l) \
    if (((*p)-buf)+(l) > len) luaL_error(L, "bad code"); (*p) += (l);

#define mar_next_len(l,T) \
    if (((*p)-buf)+sizeof(T) > len) luaL_error(L, "bad code"); \
    l = *(T*)*p; (*p) += sizeof(T);

static void unpack_value
    (lua_State *L, const char *buf, size_t len, const char **p, int *idx)
{
    size_t l;
    char val_type = **p;
    mar_incr_ptr(MAR_CHR);
    switch (val_type) {
    case LUA_TBOOLEAN:
        lua_pushboolean(L, *(char*)*p);
        mar_incr_ptr(MAR_CHR);
        break;
    case LUA_TNUMBER:
        lua_pushnumber(L, *(lua_Number*)*p);
        mar_incr_ptr(MAR_I64);
        break;
    case LUA_TSTRING:
        mar_next_len(l, uint32_t);
        lua_pushlstring(L, *p, l);
        mar_incr_ptr(l);
        break;
    case LUA_TTABLE: {
        char tag = *(char*)*p;
        mar_incr_ptr(MAR_CHR);
        if (tag == MAR_TREF) {
            int ref;
            mar_next_len(ref, int);
            lua_rawgeti(L, 2, ref);
        }
        else if (tag == MAR_TVAL) {
            mar_next_len(l, uint32_t);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_rawseti(L, 2, (*idx)++);
            mar_unpack(L, *p, l, *idx);
            mar_incr_ptr(l);
        }
        else if (tag == MAR_TUSR) {
            mar_next_len(l, uint32_t);
            lua_newtable(L);
            mar_unpack(L, *p, l, *idx);
            lua_rawgeti(L, -1, 1);
            lua_call(L, 0, 1);
            lua_remove(L, -2);
            lua_pushvalue(L, -1);
            lua_rawseti(L, 2, (*idx)++);
            mar_incr_ptr(l);
        }
        else {
            luaL_error(L, "bad encoded data");
        }
        break;
    }
    case LUA_TFUNCTION: {
        size_t nups;
        int i;
        mar_Buffer dec_buf;
        char tag = *(char*)*p;
        mar_incr_ptr(1);
        if (tag == MAR_TREF) {
            int ref;
            mar_next_len(ref, int);
            lua_rawgeti(L, 2, ref);
        }
        else {
            mar_next_len(l, uint32_t);
            dec_buf.data = (char*)*p;
            dec_buf.size = l;
            dec_buf.head = l;
            dec_buf.seek = 0;
            lua_load(L, (lua_Reader)buf_read, &dec_buf, "=marshal");
            mar_incr_ptr(l);

            lua_pushvalue(L, -1);
            lua_rawseti(L, 2, (*idx)++);

            mar_next_len(l, uint32_t);
            lua_newtable(L);
            mar_unpack(L, *p, l, *idx);
            nups = lua_objlen(L, -1);
            for (i=1; i <= nups; i++) {
                lua_rawgeti(L, -1, i);
                lua_setupvalue(L, -3, i);
            }
            lua_pop(L, 1);
            mar_incr_ptr(l);
        }
        break;
    }
    case LUA_TUSERDATA: {
        char tag = *(char*)*p;
        mar_incr_ptr(MAR_CHR);
        if (tag == MAR_TREF) {
            int ref;
            mar_next_len(ref, int);
            lua_rawgeti(L, 2, ref);
        }
        else if (tag == MAR_TUSR) {
            mar_next_len(l, uint32_t);
            lua_newtable(L);
            mar_unpack(L, *p, l, *idx);
            lua_rawgeti(L, -1, 1);
            lua_call(L, 0, 1);
            lua_remove(L, -2);
            lua_pushvalue(L, -1);
            lua_rawseti(L, 2, (*idx)++);
            mar_incr_ptr(l);
        }
        else { /* tag == MAR_TVAL */
            lua_pushnil(L);
        }
        break;
    }
    case LUA_TTHREAD:
        lua_pushnil(L);
        break;
    default:
        luaL_error(L, "bad code");
    }
}

static int mar_unpack(lua_State *L, const char* buf, size_t len, int idx)
{
    const char* p;

    p = buf;
    while (p - buf < len) {
        unpack_value(L, buf, len, &p, &idx);
        unpack_value(L, buf, len, &p, &idx);
        lua_settable(L, -3);
    }
    return 1;
}

static int tbl_marshal(lua_State* L)
{
    int x = 1;
    int e = *(char*)&x;
    const unsigned char m = MAR_MAGIC;
    mar_Buffer buf;
    lua_newtable(L);
    lua_pushvalue(L, 1);

    buf_init(L, &buf);
    buf_write(L, (void*)&m, 1, &buf);
    buf_write(L, (void*)&e, 1, &buf);
    mar_pack(L, &buf, 1);

    buf_done(L, &buf);
    return 1;
}

static int tbl_unmarshal(lua_State* L)
{
    int x = 1;
    size_t l;
    const char *s = luaL_checklstring(L, -1, &l);

    if (l < 2) luaL_error(L, "bad header");
    if (*(unsigned char *)s++ != MAR_MAGIC) luaL_error(L, "bad magic");
    l -= 2;

    lua_newtable(L);
    lua_newtable(L);

    if (*(char*)&x != *s++) {
        /* endianness mismatch so reverse the bytes */
        char *p;
        int n = l;
        int i,j;
        mar_Buffer buf;
        buf_init(L, &buf);
        buf_write(L, s, l, &buf);
        p = buf.data;
        /* stolen from lhf's lpack.c */
        for (i=0, j=n-1, n=n/2; n--; i++, j--) {
            char t=p[i]; p[i]=p[j]; p[j]=t;
        }
        mar_unpack(L, p, l, 1);
        free(buf.data);
    }
    else {
        mar_unpack(L, s, l, 1);
    }

    return 1;
}

static int tbl_clone(lua_State* L)
{
    tbl_marshal(L);
    tbl_unmarshal(L);
    return 1;
}

static const luaL_reg R[] =
{
    {"marshal",     tbl_marshal},
    {"unmarshal",   tbl_unmarshal},
    {"clone",       tbl_clone},
    {NULL,	    NULL}
};

int luaopen_marshal(lua_State *L)
{
    luaL_openlib(L, LUA_TABLIBNAME, R, 0);
    return 1;
}

