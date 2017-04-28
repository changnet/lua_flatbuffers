##### Build defaults #####
CMAKE = cmake
MAKE = make
CXX = g++ -std=c++11

TARGET_SO =         lua_flatbuffers.so
TARGET_A  =         liblua_flatbuffers.a
#CFLAGS =            -g3 -Wall -pedantic -fno-inline
CFLAGS =            -O2 -Wall -pedantic #-DNDEBUG

LUA_FLATBUFFERS_DEPS = -lflatbuffers

SHAREDDIR = .sharedlib
STATICDIR = .staticlib

AR= ar rcu
RANLIB= ranlib

OBJS = lflatbuffers.o lflatbuffers_encode.o lflatbuffers_decode.o

SHAREDOBJS = $(addprefix $(SHAREDDIR)/,$(OBJS))
STATICOBJS = $(addprefix $(STATICDIR)/,$(OBJS))

DEPS := $(SHAREDOBJS + STATICOBJS:.o=.d)

#The dash at the start of '-include' tells Make to continue when the .d file doesn't exist (e.g. on first compilation)
-include $(DEPS)

.PHONY: all clean test buildfbb staticlib sharedlib

$(SHAREDDIR)/%.o: %.cpp
	@[ ! -d $(SHAREDDIR) ] & mkdir -p $(SHAREDDIR)
	$(CXX) -c $(CFLAGS) -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -o $@ $<

$(STATICDIR)/%.o: %.cpp
	@[ ! -d $(STATICDIR) ] & mkdir -p $(STATICDIR)
	$(CXX) -c $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -o $@ $<

all: $(TARGET_SO) $(TARGET_A)

staticlib: $(TARGET_A)
sharedlib: $(TARGET_SO)

FBB_VER=1.6.0
buildfbb:
	wget https://github.com/google/flatbuffers/archive/v$(FBB_VER).tar.gz -Oflatbuffers-$(FBB_VER).tar.gz
	tar -zxvf flatbuffers-$(FBB_VER).tar.gz
	$(CMAKE) -DFLATBUFFERS_BUILD_SHAREDLIB=ON flatbuffers-$(FBB_VER) -Bflatbuffers-$(FBB_VER)
	$(MAKE) -C flatbuffers-$(FBB_VER) all
	$(MAKE) -C flatbuffers-$(FBB_VER) install
	ldconfig -v

# -Wl,--whole-archive /usr/local/lib/libflatbuffers.a -Wl,--no-whole-archive
$(TARGET_SO): $(SHAREDOBJS)
	$(CXX) $(LDFLAGS) -shared -o $@ $(SHAREDOBJS) $(LUA_FLATBUFFERS_DEPS)

$(TARGET_A): $(STATICOBJS)
	$(AR) $@ $(STATICOBJS)
	$(RANLIB) $@

test:
	flatc -b --schema monster_test.fbs
	lua test.lua

clean:
	rm -f -R $(SHAREDDIR) $(STATICDIR) $(TARGET_SO) $(TARGET_A)
