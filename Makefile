CC := gcc

ifeq ($(debug), y)
    CFLAGS := -g
else
    CFLAGS := -O2 -DNDEBUG
endif

CFLAGS := $(CFLAGS) -Wl,-E -Wall -Werror -fPIC

INCLUDE := -I/usr/include/postgresql
LIBS := -llua -lm -lpq -ldl

ifdef LUADIR
	INCLUDE := -I$(LUADIR)/src $(INCLUDE)
	LIBS := -L$(LUADIR)/src $(LIBS)
endif

TARGET := luapgsql luapgsql.so

.PHONY: all clean

all: $(TARGET)

luapgsql: luapgsql.o host.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

luapgsql.so: luapgsql.o
	$(CC) $(CFLAGS) $^ -shared -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f $(TARGET) *.o
