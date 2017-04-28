lua_flatbuffers
================
[![Build Status](https://travis-ci.org/changnet/lua_flatbuffers.svg?branch=master)](https://travis-ci.org/changnet/lua_flatbuffers)

a lua google flatbuffers encode/decode module base on google flatbuffers reflection  
See more about google flatbuffers at http://google.github.io/flatbuffers/  

Dependency
-------------
* lua >=5.3.0(http://www.lua.org/)
* FlatBuffers >=1.4.0(https://github.com/google/flatbuffers/releases)
* linux(g++ support c++11)

make sure flatbuffers shared library is built if you need to build the lua_flatbuffers.so.

Installation
------------

Make sure lua and FlatBuffers develop environment already installed

 * git clone https://github.com/changnet/lua_flatbuffers.git
 * cd lua_flatbuffers
 * (make buildfbb will install FlatBuffers)
 * make
 * make test
 * Copy lua_flatbuffers.so to your lua project's c module directory
or embed to your project

Api
-----

```lua
-- encode lua table into flatbuffer and return the buffer
encode( bfbs,object,tbl )

-- decode flatbuffer into a lua table,return the lua table
decode( bfbs,object,buffer )

-- load all the flatbuffers binary schema file in path with suffix
load_bfbs_path( path,suffix )

-- load one binary schema file
load_bfbs_file( path )
```
Example & Benchmark
-------

See 'test.lua'   

simple benchmark test 100000 times,encode elapsed time: 1.14 second,decode elapsed time: 2.36 second

Note
-----
1. namespace not supported! you have to put objects with same name in different
schema file,though they are in different namespace.
2. deprecated field still be encode or decode.
3. [byte] or [ubyte] will be encode as lua string
