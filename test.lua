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
-- lfb:load_bfbs_path( ".","bfbs" )
lfb:load_bfbs_file( "monster_test.bfbs" )

local mlocs = { {},{},{} }
mlocs[1].name = "Fred"
mlocs[2].name = "Barney"
mlocs[2].hp   = 1000
mlocs[3].name = "Wilma"

local monster     = {}
monster.pos       = {x = 1, y = 2, z = 3, test1 = 0, test2 = 1,test3 = {a = 10,b = 20}}
monster.mana      = 150
monster.hp        = 80
monster.name      = "MyMonster"

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

local buffer  = lfb:encode( "monster_test.bfbs","Monster",monster )
local mon_tbl = lfb:decode( "monster_test.bfbs","Monster",buffer  )
vd( mon_tbl )

local max = 100000
local sx = os.clock()

local buffer
for index = 1,max do
    buffer = lfb:encode( "monster_test.bfbs","Monster",monster )
end
local sy = os.clock()

local ex = os.clock()
local mon_tbl
for index = 1,max do
    mon_tbl = lfb:decode( "monster_test.bfbs","Monster",buffer )
end
local ey = os.clock()

print( string.format("simple benchmark test %d times,encode elapsed time: %.2f second,decode elapsed time: %.2f second",
    max,sy - sx,ey - ex))
