CC := gcc

ifeq ($(debug), y)
    CFLAGS := -g
else
    CFLAGS := -O2 -DNDEBUG
endif

CFLAGS := $(CFLAGS) -Wl,-E -Wall -Werror -fPIC

LUADIR := $(HOME)/workspace/lua

INCLUDE := -I$(LUADIR)/src -I/usr/include/postgresql
LIBS := -ldl -L$(LUADIR)/src -llua -lm -lpq

TARGET := luapgsql luapgsql.so

.PHONY: all clean deps

all: deps $(TARGET)

luapgsql: luapgsql.o host.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

luapgsql.so: luapgsql.o
	$(CC) $(CFLAGS) $^ -shared -o $@ $(LIBS)

deps:
	$(MAKE) posix MYCFLAGS="-fPIC -DLUA_USE_DLOPEN -Wl,-E" MYLIBS="-ldl" -C $(LUADIR)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean:
	$(MAKE) clean -C $(LUADIR)
	rm -f $(TARGET) *.o
