##### Build defaults #####
CMAKE = cmake
MAKE = make
CC = g++ -std=c++11
TARGET_FBB =        flatbuffers
TARGET_SO =         lua_flatbuffers.so
TARGET_A  =         liblua_flatbuffers.a
PREFIX =            /usr/local
CFLAGS =            -g3 -Wall -pedantic -fno-inline
#CFLAGS =            -O2 -Wall -pedantic -DNDEBUG

LUA_INCLUDE_DIR =   $(PREFIX)/include
FBB_INCLUDE_DIR =   ./flatbuffers-1.4.0/include
FBB_LIBRARY_DIR =   ./flatbuffers-1.4.0

LUA_FLATBUFFERS_CFLAGS =      -fpic -I$(LUA_INCLUDE_DIR) -I$(FBB_INCLUDE_DIR)
LUA_FLATBUFFERS_LDFLAGS =     -shared -L$(FBB_LIBRARY_DIR)

AR= ar rcu
RANLIB= ranlib

OBJS =              lflatbuffers.o main.o

.PHONY: all clean test build

.cpp.o:
	$(CC) -c $(CFLAGS) $(LUA_FLATBUFFERS_CFLAGS) -o $@ $<

all: $(TARGET_SO)

tcpp: $(OBJS)
	$(CC) -o main $(OBJS) -L$(FBB_LIBRARY_DIR) -llua

build: $(TARGET_FBB)

$(TARGET_FBB):flatbuffers-1.4.0.tar.gz
	tar -zxvf flatbuffers-1.4.0.tar.gz
	$(CMAKE) -G flatbuffers-1.4.0
	$(MAKE) -C flatbuffers-1.4.0 all

$(TARGET_SO): $(OBJS)
	$(CC) $(LDFLAGS) $(LUA_FLATBUFFERS_LDFLAGS) -o $@ $(OBJS) -lflatbuffers

$(TARGET_A): $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

test:
	lua test.lua

clean:
	rm -f *.o $(TARGET_SO) $(TARGET_A)
