##### Build defaults #####
CMAKE = cmake
MAKE = make
CXX = g++ -std=c++11
TARGET_FBB =        flatbuffers
TARGET_SO =         lua_flatbuffers.so
TARGET_A  =         liblua_flatbuffers.a
PREFIX =            /usr/local
#CFLAGS =            -g3 -Wall -pedantic -fno-inline
CFLAGS =            -O2 -Wall -pedantic

LUA_INCLUDE_DIR =   $(PREFIX)/include

#force to link with static flatbuffers library
LUA_FLATBUFFERS_DEPS = -Wl,-dn -lflatbuffers -Wl,-dy
LUA_FLATBUFFERS_CFLAGS = -fPIC
LUA_FLATBUFFERS_LDFLAGS = -shared

AR= ar rcu
RANLIB= ranlib

OBJS =              lflatbuffers.o lflatbuffers_encode.o lflatbuffers_decode.o

DEPS := $(OBJS:.o=.d)

.PHONY: all clean test buildfbb

.cpp.o:
	$(CXX) -c $(CFLAGS) $(LUA_FLATBUFFERS_CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -o $@ $<

all: $(TARGET_SO) $(TARGET_A)

#The dash at the start of '-include' tells Make to continue when the .d file doesn't exist (e.g. on first compilation)
-include $(DEPS)

buildfbb: $(TARGET_FBB)

$(TARGET_FBB):flatbuffers-1.4.0.tar.gz
	tar -zxvf flatbuffers-1.4.0.tar.gz
	$(CMAKE) -DCMAKE_CXX_FLAGS=-fPIC flatbuffers-1.4.0 -Bflatbuffers-1.4.0
	$(MAKE) -C flatbuffers-1.4.0 all
	$(MAKE) -C flatbuffers-1.4.0 install
	ldconfig -v

# -Wl,--whole-archive /usr/local/lib/libflatbuffers.a -Wl,--no-whole-archive
$(TARGET_SO): $(OBJS)
	$(CXX) $(LDFLAGS) $(LUA_FLATBUFFERS_LDFLAGS) -o $@ $(OBJS) $(LUA_FLATBUFFERS_DEPS)

$(TARGET_A): $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

test:
	flatc -b --schema monster_test.fbs
	lua test.lua

clean:
	rm -f *.o *.d $(TARGET_SO) $(TARGET_A)
