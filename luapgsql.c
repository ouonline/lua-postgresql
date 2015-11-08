#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <libpq-fe.h>

static const char* PGSQLLIB = "PGSQLLIB";
static const char* PGSQLRESULT = "PGSQLRESULT";

/* ------------------------------------------------------------------------- */

#define pgsql_init_error(l, fmt, ...) \
    do { \
        lua_pop(l, lua_gettop(l) - 1); \
        lua_pushnil(l); /* empty result */ \
        lua_pushfstring(l, fmt, ##__VA_ARGS__); /* error message */ \
    } while (0)

/* return 0 if ok */
static inline int get_conninfo(lua_State* l, char* buf, int size)
{
    int argtype;
    unsigned int offset = 0;

    if (!lua_istable(l, 1)) {
        pgsql_init_error(l, "argument #1 is expected to be a table, but a %s is provided.",
                         lua_typename(l, lua_type(l, 1)));
        return 2;
    }

    lua_getfield(l, 1, "host");
    if (!lua_isstring(l, -1)) {
        argtype = lua_type(l, -1);
        pgsql_init_error(l, "argument#1::`host' is expected to be a string, but a %s is provided.",
                         lua_typename(l, argtype));
        return 2;
    }
    offset = snprintf(buf, size, "host='%s'", lua_tostring(l, -1));

    lua_getfield(l, 1, "port");
    if (!lua_isnumber(l, -1)) {
        argtype = lua_type(l, -1);
        pgsql_init_error(l, "argument#1::`port' is expected to be a number, but a %s is provided.",
                         lua_typename(l, argtype));
        return 2;
    }
    offset += snprintf(buf + offset, size - offset, " port='%s'",
                       lua_tostring(l, -1));

    lua_getfield(l, 1, "user");
    if (!lua_isnil(l, -1)) {
        if (!lua_isstring(l, -1)) {
            argtype = lua_type(l, -1);
            pgsql_init_error(l, "argument#1::`user' is expected to be a string, but a %s is provided.",
                             lua_typename(l, argtype));
            return 2;
        }
        offset += snprintf(buf + offset, size - offset, " user='%s'",
                           lua_tostring(l, -1));
    }

    lua_getfield(l, 1, "password");
    if (!lua_isnil(l, -1)) {
        if (!lua_isstring(l, -1)) {
            argtype = lua_type(l, -1);
            pgsql_init_error(l, "argument#1::`password' is expected to be a string, but a %s is provided.",
                             lua_typename(l, argtype));
            return 2;
        }
        offset += snprintf(buf + offset, size - offset, " password='%s'",
                           lua_tostring(l, -1));
    }

    lua_getfield(l, 1, "db");
    if (!lua_isnil(l, -1)) {
        if (!lua_isstring(l, -1)) {
            argtype = lua_type(l, -1);
            pgsql_init_error(l, "argument#1::`db' is expected to be a string, but a %s is provided.",
                             lua_typename(l, argtype));
            return 2;
        }
        offset += snprintf(buf + offset, size - offset, " dbname='%s'",
                           lua_tostring(l, -1));
    }

    lua_getfield(l, 1, "connect_timeout");
    if (!lua_isnil(l, -1)) {
        if (!lua_isstring(l, -1)) {
            argtype = lua_type(l, -1);
            pgsql_init_error(l, "argument#1::`connect_timeout' is expected to be a string, but a %s is provided.",
                             lua_typename(l, argtype));
            return 2;
        }
        offset += snprintf(buf + offset, size - offset, " connect_timeout='%s'",
                           lua_tostring(l, -1));
    }

    return 0;
}

#define BUFSIZE 1024
#define CONNINFOSIZE (BUFSIZE - sizeof(PGconn*))

typedef struct {
    PGconn* conn;
    char conninfo[CONNINFOSIZE];
} PGSQL;

/* return a connection and an error message */
static int l_new_pgsqlclient(lua_State* l)
{
    PGSQL* pg;

    if (lua_gettop(l) != 1) {
        lua_pushnil(l);
        lua_pushfstring(l, "1 argument required, but %d is provided.",
                        lua_gettop(l));
        return 2;
    }

    pg = lua_newuserdata(l, sizeof(PGSQL));
    luaL_setmetatable(l, PGSQLLIB);

    if (get_conninfo(l, pg->conninfo, CONNINFOSIZE) != 0)
        return 2;

    pg->conn = PQconnectdb(pg->conninfo);
    if (PQstatus(pg->conn) != CONNECTION_OK) {
        pgsql_init_error(l, PQerrorMessage(pg->conn));
        return 2;
    }

    lua_pop(l, lua_gettop(l) - 2);
    lua_pushnil(l); /* errmsg */
    return 2;
}

/* ------------------------------------------------------------------------- */

/* return an error message if fails, otherwise nil */
static int l_pgsqlclient_ping(lua_State* l)
{
    PGSQL* pg;
    PGPing status;

    pg = luaL_testudata(l, 1, PGSQLLIB);
    if (!pg) {
        lua_pushstring(l, "argument #1 is not a pgsql client.");
        return 1;
    }

    status = PQping(pg->conninfo);
    switch (status) {
        case PQPING_OK:
            lua_pushnil(l);
        break;
        case PQPING_REJECT:
            lua_pushstring(l, "The server is running but is in a state that disallows connections (startup, shutdown, or crash recovery).");
        break;
        case PQPING_NO_RESPONSE:
            lua_pushstring(l, "The server could not be contacted. This might indicate that the server is not running, or that there is something wrong with the given connection parameters (for example, wrong port number), or that there is a network connectivity problem (for example, a firewall blocking the connection request).");
        break;
        case PQPING_NO_ATTEMPT:
            lua_pushstring(l, "No attempt was made to contact the server, because the supplied parameters were obviously incorrect or there was some client-side problem (for example, out of memory).");
        break;
    }

    return 1;
}

/* return an error message if fails, otherwise nil */
static int l_pgsqlclient_setcharset(lua_State* l)
{
    PGSQL* pg;
    const char* charset;

    pg = luaL_testudata(l, 1, PGSQLLIB);
    if (!pg) {
        lua_pushstring(l, "argument #1 is not a pgsql client.");
        return 1;
    }

    if (!lua_isstring(l, 2)) {
        lua_pushfstring(l, "argument #2 is expected to be a string, but given a %s.",
                        lua_typename(l, lua_type(l, 2)));
        return 1;
    }
    charset = lua_tostring(l, 2);

    if (PQsetClientEncoding(pg->conn, charset) == 0)
        lua_pushnil(l);
    else
        lua_pushstring(l, PQerrorMessage(pg->conn));

    return 1;
}

/* return the result set and the error message */
static int l_pgsqlclient_query(lua_State* l)
{
    PGSQL* pg;
    PGresult** result;
    ExecStatusType status;
    const char* sqlstr;
    unsigned long sqllen = 0;

    pg = luaL_testudata(l, 1, PGSQLLIB);
    if (!pg) {
        lua_pushnil(l);
        lua_pushstring(l, "argument #1 is not a pgsql client.");
        return 2;
    }

    if (!lua_isstring(l, 2)) {
        int type = lua_type(l, 2);
        lua_pushnil(l);
        lua_pushfstring(l, "argument #2 expects a sql string, but given a %s.",
                        lua_typename(l, type));
        return 2;
    }
    sqlstr = lua_tolstring(l, 2, &sqllen);

    if (sqllen == 0) {
        lua_pushnil(l);
        lua_pushstring(l, "invalid SQL statement.");
        return 2;
    }

    result = lua_newuserdata(l, sizeof(PGresult*)); /* index 3 */
    luaL_setmetatable(l, PGSQLRESULT);

    *result = PQexec(pg->conn, sqlstr);
    status = PQresultStatus(*result);
    if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK)
        lua_pushnil(l);
    else {
        lua_pop(l, 1);
        lua_pushnil(l);
        lua_pushstring(l, PQresultErrorMessage(*result));
    }

    return 2;
}

static int l_pgsqlclient_gc(lua_State* l)
{
    PGSQL* pg = luaL_testudata(l, 1, PGSQLLIB);
    if (pg)
        PQfinish(pg->conn);

    return 0;
}

/* ------------------------------------------------------------------------- */

/* return the fieldnamelist, or nil if fails. */
static int l_pgsqlresult_fieldnamelist(lua_State* l)
{
    PGresult** result = luaL_testudata(l, 1, PGSQLRESULT);

    if (!result)
        lua_pushnil(l);
    else {
        int i;
        int nr_field = PQnfields(*result);

        lua_newtable(l);
        for (i = 0; i < nr_field; ++i) {
            lua_pushstring(l, PQfname(*result, i));
            lua_rawseti(l, -2, i + 1);
        }
    }

    return 1;
}

/* return the number of record(s), or nil if fails. */
static int l_pgsqlresult_size(lua_State* l)
{
    PGresult** result = luaL_testudata(l, 1, PGSQLRESULT);

    if (!result)
        lua_pushnil(l);
    else
        lua_pushinteger(l, PQntuples(*result));

    return 1;
}

static int l_pgsqlresult_record_iter(lua_State* l)
{
    PGresult** result = lua_touserdata(l, lua_upvalueindex(1));
    int cur = lua_tointeger(l, lua_upvalueindex(2));
    int nr_record = PQntuples(*result);
    int nr_field = PQnfields(*result);

    if (cur < nr_record) {
        int i;

        lua_newtable(l);
        for (i = 0; i < nr_field; ++i) {
            lua_pushlstring(l, PQgetvalue(*result, cur, i),
                            PQgetlength(*result, cur, i));
            lua_rawseti(l, -2, i + 1);
        }

        lua_pushinteger(l, cur + 1);
        lua_replace(l, lua_upvalueindex(2));

        return 1;
    }

    return 0;
}

/* return a record iterator, or nil if fails. */
static int l_pgsqlresult_recordlist(lua_State* l)
{
    PGresult** result = luaL_testudata(l, 1, PGSQLRESULT);

    if (!result)
        lua_pushnil(l);
    else {
        lua_pushvalue(l, -1); /* duplicate the result */
        lua_pushinteger(l, 0); /* current row */
        lua_pushcclosure(l, l_pgsqlresult_record_iter, 2);
    }

    return 1;
}

static int l_pgsqlresult_gc(lua_State* l)
{
    PGresult** result = luaL_testudata(l, 1, PGSQLRESULT);

    if (*result)
        PQclear(*result);

    return 0;
}

/* ------------------------------------------------------------------------- */

static const struct luaL_Reg pgsqlclient_f[] = {
    {"newclient", l_new_pgsqlclient},
    {NULL, NULL},
};

static const struct luaL_Reg pgsqlclient_m[] = {
    {"ping", l_pgsqlclient_ping},
    {"setcharset", l_pgsqlclient_setcharset},
    {"query", l_pgsqlclient_query},
    {"__gc", l_pgsqlclient_gc},
    {NULL, NULL},
};

static const struct luaL_Reg pgsqlresult_lib[] = {
    {"size", l_pgsqlresult_size},
    {"fieldnamelist", l_pgsqlresult_fieldnamelist},
    {"recordlist", l_pgsqlresult_recordlist},
    {"__gc", l_pgsqlresult_gc},
    {NULL, NULL},
};

int luaopen_luapgsql(lua_State* l)
{
    /* meta table for pgsql client */
    luaL_newmetatable(l, PGSQLLIB);
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");
    luaL_setfuncs(l, pgsqlclient_m, 0);

    /* meta table for pgsql result */
    luaL_newmetatable(l, PGSQLRESULT);
    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");
    luaL_setfuncs(l, pgsqlresult_lib, 0);

    luaL_newlib(l, pgsqlclient_f);
    return 1;
}
