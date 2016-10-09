-- lua_flatbuffers test


function var_dump(data, max_level, prefix)
    if type(prefix) ~= "string" then
        prefix = ""
    end

    if type(data) ~= "table" then
        print(prefix .. tostring(data))
    else
        print(data)
        if max_level ~= 0 then
            local prefix_next = prefix .. "    "
            print(prefix .. "{")
            for k,v in pairs(data) do
                io.stdout:write(prefix_next .. k .. " = ")
                if type(v) ~= "table" or (type(max_level) == "number" and max_level <= 1) then
                    print(v)
                else
                    if max_level == nil then
                        var_dump(v, nil, prefix_next)
                    else
                        var_dump(v, max_level - 1, prefix_next)
                    end
                end
            end
            print(prefix .. "}")
        end
    end
end

--[[
    eg: local b = {aaa="aaa",bbb="bbb",ccc="ccc"}
]]
function vd(data, max_level)
    var_dump(data, max_level or 20)
end

local lua_flatbuffers = require "lua_flatbuffers"

local lfb = lua_flatbuffers()

print( lfb:load_bfbs_file( "test.bfbs" ) )

local tbl =
{
    bool_min = false,
    bool_max = true,
    int8_min = -128,
    int8_max = 128,
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

    float_min = 998998.12334567,
    float_max = 998998.987654321,

    double_min = 998.893837464,
    double_max = 998.893837464,
}

print( "start test scalar_struct ..." )
local buffer = lfb:encode( "test.bfbs","scalar_struct",tbl )
print( "decode done",string.len(buffer) )

local after_tbl = lfb:decode( "test.bfbs","scalar_struct",buffer )
vd( after_tbl )
print( "end test scalar_struct ..." )

print( "start test scalar_table ..." )
local buffer = lfb:encode( "test.bfbs","scalar_table",tbl )
print( "decode done",string.len(buffer) )

local after_tbl = lfb:decode( "test.bfbs","scalar_table",buffer )
vd( after_tbl )
print( "end test scalar_table ..." )

local test_struct_tbl = {}
test_struct_tbl.x = 9874310
test_struct_tbl.y = 32101234
test_struct_tbl.sub_struct = tbl
print( "start test test_struct ..." )
local buffer = lfb:encode( "test.bfbs","test_struct",test_struct_tbl )
print( "decode done",string.len(buffer) )

local after_tbl = lfb:decode( "test.bfbs","test_struct",buffer )
vd( after_tbl )
print( "end test test_struct ..." )
