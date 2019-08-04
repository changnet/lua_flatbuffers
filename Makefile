##### Build defaults #####
CMAKE = cmake
MAKE = make
CXX = g++

TARGET_SO =         lua_flatbuffers.so
TARGET_A  =         liblua_flatbuffers.a
#CFLAGS =            -std=c++11 -g3 -Wall -pedantic -fno-inline
CFLAGS =            -std=c++11 -O2 -Wall -pedantic #-DNDEBUG

LUA_FLATBUFFERS_DEPS = -lflatbuffers

SHAREDDIR = .sharedlib
STATICDIR = .staticlib

# AR= ar rcu # ar: `u' modifier ignored since `D' is the default (see `U')

AR= ar rc
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

FBB_VER=1.11.0
buildfbb:
	wget https://github.com/google/flatbuffers/archive/v$(FBB_VER).tar.gz -Oflatbuffers-$(FBB_VER).tar.gz
	tar -zxvf flatbuffers-$(FBB_VER).tar.gz
	$(CMAKE) -DFLATBUFFERS_BUILD_SHAREDLIB=ON flatbuffers-$(FBB_VER) -Bflatbuffers-$(FBB_VER) -DCMAKE_POSITION_INDEPENDENT_CODE=ON
	$(MAKE) -C flatbuffers-$(FBB_VER) all
	$(MAKE) -C flatbuffers-$(FBB_VER) install
	ldconfig -v

# -Wl,-E
# When creating a dynamically linked executable, using the -E option or the 
# --export-dynamic option causes the linker to add all symbols to the dynamic 
# symbol table. The dynamic symbol table is the set of symbols which are visible
# from dynamic objects at run time
# -Wl,--whole-archive /usr/local/lib/libflatbuffers.a -Wl,--no-whole-archive
$(TARGET_SO): $(SHAREDOBJS)
	$(CXX) $(LDFLAGS) -shared -o $@ $(SHAREDOBJS) $(LUA_FLATBUFFERS_DEPS)

$(TARGET_A): $(STATICOBJS)
	$(AR) $@ $(STATICOBJS)
	$(RANLIB) $@

test:
	#flatc -b --schema monster_test.fbs
	#lua test.lua

	flatc -c include_test1.fbs
	flatc -c monster_test.fbs
	g++ -Wall -o cpp_test cpp_test.cpp
	./cpp_test

clean:
	rm -f -R $(SHAREDDIR) $(STATICDIR) $(TARGET_SO) $(TARGET_A)
