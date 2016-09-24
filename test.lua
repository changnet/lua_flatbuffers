-- lua_flatbuffers test

local lua_flatbuffers = require "lua_flatbuffers"

local lfb = lua_flatbuffers()

print( lfb:load_bfbs_file( "test.bfbs" ) )

local tbl = { x = 9,y = 10,z = 99 }

local buffer = lfb:encode( "test.bfbs","stc",tbl )
