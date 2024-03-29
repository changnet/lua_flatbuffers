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

function test_table_eq(src,dst)
    if not src and not dst then error("not table") end

    -- one of them is nil,return not equal table
    if not src then return false,dst end
    if not dst then return false,src end

    local eq = true
    local not_eq = {}
    for k,v in pairs(src) do
        local dv = dst[k]
        if "table" == type(v) then
            local v_eq,v_not_eq = test_table_eq(v,dv)
            if not v_eq then eq = false;not_eq[k] = v_not_eq end
        else
            if v ~= dv then
                if "number" == type(v)
                    and "number" == type(dv) and math.abs(v - dv) < 0.001 then
                    -- double compare,do nothing
                else
                    eq = false
                    not_eq[k] = {src = v,dst = dst[k]}
                end
            end
        end
    end

    return eq,not_eq
end

local lua_flatbuffers = require "lua_flatbuffers"

local lfb = lua_flatbuffers()
-- lfb:load_bfbs_path( ".","bfbs" )
lfb:load_bfbs_file( "monster_test.bfbs" )

local mlocs = { {},{},{} }
mlocs[1].name = "Fred"
mlocs[2].name = "Barney"
mlocs[2].hp   = 1000
mlocs[3].name = "Wilma"

local monster     = {}
monster.pos       =
{
    x = 32562.1,y = 2456789, z = 3333333,
    test1 = 111119.88897, test2 = 1,test3 = {a = 10655,b = 20}
}
monster.mana      = 1500
monster.hp        = 800
monster.name      = "testMyMonster"

-- string, which may only hold UTF-8 or 7-bit ASCII. For other text encodings or general binary data use vectors ([byte] or [ubyte]) instead.
-- in lua,[byte] or [ubyte] can hold binary data and string,but not a number vector.
-- other language(c++) may hold number vector.if so you have to retrieve data with string.byte
-- monster.inventory = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }
monster.inventory = "0123456789"

-- no enum in lua,number insted
monster.color     = 3 -- Color_Blue

-- you must specific union field type
monster.test_type = 1 -- Any_Monster
monster.test      = mlocs[1]

monster.test4     = { {a = 10,b = 20},{a = 30,b = 40} }

monster.testarrayofstring  = { "bob", "fred", "bob", "fred" }
monster.testarrayoftables  = mlocs

-- optional field should be nil,not 0
-- enemy
-- testnestedflatbuffer
-- testempty

monster.testbool = false

monster.testhashs32_fnv1  = 0
monster.testhashu32_fnv1  = 0
monster.testhashs64_fnv1  = 0
monster.testhashu64_fnv1  = 0
monster.testhashs32_fnv1a = 0
monster.testhashu32_fnv1a = 0
monster.testhashs64_fnv1a = 0
monster.testhashu64_fnv1a = 0
-- monster.testarrayofbools

monster.testf = 3.14159
monster.testf2 = 3.0
monster.testf3 = 0.0

monster.testarrayofstring2 = { "jane","mary" }

-- math.maxinteger support
if _VERSION >= "Lua 5.3" then
    local min_val = {
        val = math.mininteger
    }
    local min_buff = lfb:encode("monster_test.bfbs","MyGame.Example.Stat", min_val)
    local min_val2 = lfb:decode("monster_test.bfbs","MyGame.Example.Stat", min_buff)
    assert(min_val2.val == min_val.val)

    local max_val = {
        val = math.maxinteger
    }
    local max_buff = lfb:encode("monster_test.bfbs","MyGame.Example.Stat", max_val)
    local max_val2 = lfb:decode("monster_test.bfbs","MyGame.Example.Stat", max_buff)
    print(string.format("expect %d, got %d", max_val.val, max_val2.val))
    assert(max_val2.val == max_val.val)
end

local buffer  = lfb:encode( "monster_test.bfbs","MyGame.Example.Monster",monster )
local mon_tbl = lfb:decode( "monster_test.bfbs","MyGame.Example.Monster",buffer  )
vd( mon_tbl )

local is_equal,not_eq = test_table_eq(monster,mon_tbl)
if not is_equal then
    vd( not_eq )
    error("test fail")
end

-- write to file,cross test with cpp later
local wf = io.open( "monsterdata_test.mon", "w")
wf:write(buffer)
io.close(wf)

-- include test
local include_test = { x = 1 }
local include_buff = lfb:encode( "monster_test.bfbs","simple_table",include_test )
local include_tbl = lfb:decode( "monster_test.bfbs","simple_table",include_buff )
vd(include_tbl)

-- dump binary to understand what is inside flatbuffer,see doc/README.md
local hex_tbl = {}
for index = 1,string.len(include_buff) do
    table.insert( hex_tbl,
        string.format("%02X",string.byte(include_buff, index)) )
end
print( table.concat(hex_tbl," ") )

local max = 100000
local encode_beg = os.clock()

local buffer
for index = 1,max do
    buffer = lfb:encode( "monster_test.bfbs","MyGame.Example.Monster",monster )
end
local encode_end = os.clock()

local decode_beg = os.clock()
local mon_tbl
for index = 1,max do
    mon_tbl = lfb:decode( "monster_test.bfbs","MyGame.Example.Monster",buffer )
end
local decode_end = os.clock()

local encode_tm = encode_end - encode_beg
local decode_tm = decode_end - decode_beg
local buff_size = string.len(buffer) * max / 1024 / 1024 -- byte to MB

print( string.format(
    "test %d times,each size:%d byte,encode: %.2f second %.2f MB/s;decode: %.2f second，rate: %.2f MB/s",
    max,string.len(buffer),encode_tm, buff_size / encode_tm,decode_tm, buff_size / decode_tm))
