#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <stdlib.h>
#include <libpq-fe.h>

/* ------------------------------------------------------------------------- */

#define pgsql_init_error(l, fmt, ...)                               \
    do {                                                            \
        lua_pop(l, lua_gettop(l) - 1);                              \
        lua_pushnil(l); /* empty result */                          \
        lua_pushfstring(l, fmt, ##__VA_ARGS__); /* error message */ \
    } while (0)

/* return 0 if ok */
static inline int get_conninfo(lua_State* l, char* buf, int size) {
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
static int l_new_pgsqlclient(lua_State* l) {
    PGSQL* pg;

    if (lua_gettop(l) != 1) {
        lua_pushnil(l);
        lua_pushfstring(l, "1 argument required, but %d is provided.",
                        lua_gettop(l));
        return 2;
    }

    pg = lua_newuserdata(l, sizeof(PGSQL));
    lua_pushvalue(l, lua_upvalueindex(1));
    lua_setmetatable(l, -2);

    if (get_conninfo(l, pg->conninfo, CONNINFOSIZE) != 0) {
        return 2;
    }

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
static int l_pgsqlclient_ping(lua_State* l) {
    PGSQL* pg = lua_touserdata(l, 1);
    PGPing status = PQping(pg->conninfo);
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
static int l_pgsqlclient_setcharset(lua_State* l) {
    PGSQL* pg;
    const char* charset;
    if (!lua_isstring(l, 2)) {
        lua_pushfstring(l, "argument #2 is expected to be a string, but given a %s.",
                        lua_typename(l, lua_type(l, 2)));
        return 1;
    }

    pg = lua_touserdata(l, 1);
    charset = lua_tostring(l, 2);

    if (PQsetClientEncoding(pg->conn, charset) == 0) {
        lua_pushnil(l);
    } else {
        lua_pushstring(l, PQerrorMessage(pg->conn));
    }

    return 1;
}

/* return the result set and the error message */
static int l_pgsqlclient_execute(lua_State* l) {
    PGSQL* pg;
    PGresult** result;
    ExecStatusType status;
    const char* sqlstr;
    unsigned long sqllen = 0;

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

    result = lua_newuserdata(l, sizeof(PGresult*));
    lua_pushvalue(l, lua_upvalueindex(1));
    lua_setmetatable(l, -2);

    pg = lua_touserdata(l, 1);
    *result = PQexec(pg->conn, sqlstr);
    status = PQresultStatus(*result);
    if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK) {
        lua_pushnil(l);
    } else {
        lua_pop(l, 1);
        lua_pushnil(l);
        lua_pushstring(l, PQresultErrorMessage(*result));
    }

    return 2;
}

static int l_pgsqlclient_gc(lua_State* l) {
    PGSQL* pg = lua_touserdata(l, 1);
    PQfinish(pg->conn);
    return 0;
}

/* ------------------------------------------------------------------------- */

/* return the fieldnamelist, or nil if fails. */
static int l_pgsqlresult_fieldnamelist(lua_State* l) {
    int i;
    PGresult** result = lua_touserdata(l, 1);
    int nr_field = PQnfields(*result);

    lua_createtable(l, nr_field, 0);
    for (i = 0; i < nr_field; ++i) {
        lua_pushstring(l, PQfname(*result, i));
        lua_rawseti(l, -2, i + 1);
    }

    return 1;
}

/* return the number of record(s), or nil if fails. */
static int l_pgsqlresult_size(lua_State* l) {
    PGresult** result = lua_touserdata(l, 1);
    lua_pushinteger(l, PQntuples(*result));
    return 1;
}

static int l_pgsqlresult_record_iter(lua_State* l) {
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
static int l_pgsqlresult_recordlist(lua_State* l) {
    lua_pushvalue(l, 1);
    lua_pushinteger(l, 0);
    lua_pushcclosure(l, l_pgsqlresult_record_iter, 2);
    return 1;
}

static int l_pgsqlresult_gc(lua_State* l) {
    PGresult** result = lua_touserdata(l, 1);
    PQclear(*result);
    return 0;
}

/* ------------------------------------------------------------------------- */

static void create_pgsqlresult_funcs(lua_State* l) {
    lua_newtable(l);

    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    lua_pushcfunction(l, l_pgsqlresult_size);
    lua_setfield(l, -2, "size");

    lua_pushcfunction(l, l_pgsqlresult_fieldnamelist);
    lua_setfield(l, -2, "fieldnamelist");

    lua_pushcfunction(l, l_pgsqlresult_recordlist);
    lua_setfield(l, -2, "recordlist");

    lua_pushcfunction(l, l_pgsqlresult_gc);
    lua_setfield(l, -2, "__gc");
}

static void create_pgsqlclient_funcs(lua_State* l) {
    lua_newtable(l);

    lua_pushvalue(l, -1);
    lua_setfield(l, -2, "__index");

    lua_pushcfunction(l, l_pgsqlclient_ping);
    lua_setfield(l, -2, "ping");

    lua_pushcfunction(l, l_pgsqlclient_setcharset);
    lua_setfield(l, -2, "setcharset");

    create_pgsqlresult_funcs(l);
    lua_pushcclosure(l, l_pgsqlclient_execute, 1);
    lua_setfield(l, -2, "execute");

    lua_pushcfunction(l, l_pgsqlclient_gc);
    lua_setfield(l, -2, "__gc");
}

int luaopen_luapgsql(lua_State* l) {
    lua_createtable(l, 0, 1);
    create_pgsqlclient_funcs(l);
    lua_pushcclosure(l, l_new_pgsqlclient, 1);
    lua_setfield(l, -2, "newclient");
    return 1;
}
