-- lua_flatbuffers test

local lua_flatbuffers = require "lua_flatbuffers"

local lfb = lua_flatbuffers()

print( lfb:load_bfbs_file( "test.bfbs" ) )

local tbl =
{
    int8_min = 222,
    int8_max = 222,
    uint8_min = 223,
    uint8_max = 223,

    int16_min = 224,
    int16_max = 224,
    uint16_min = 225,
    uint16_max = 225,

    int32_min = 2234556,
    int32_max = 2234556,
    uint32_min = 12345567,
    uint32_max = 12345567,

    int64_min = 99887766,
    int64_max = 99887766,
    uint64_min = 68932345,
    uint64_max = 68932345,

    double_min = 998.893837464,
    double_max = 998.893837464,
}

local buffer = lfb:encode( "test.bfbs","scalar_types",tbl )

print( "decode done",string.len(buffer),buffer )
