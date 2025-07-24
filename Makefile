CC := gcc

ifeq ($(debug), y)
    CFLAGS := -g
else
    CFLAGS := -O2 -DNDEBUG
endif

CFLAGS := $(CFLAGS) -Wl,-E -Wall -Werror -fPIC

ifndef LUADIR
    $(error environment variable 'LUADIR' is not set)
endif

INCLUDE := -I$(LUADIR)/src -I/usr/include/postgresql
LIBS := -L$(LUADIR)/src -llua -lm -lpq -ldl

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
	rm -f $(TARGET) *.o
