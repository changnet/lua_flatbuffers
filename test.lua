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
--[[
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
]]

local test_table_tbl = {}
test_table_tbl.str = "abcdefghijklmnopqrstuvwxyz0123456789/*-+测试中文字符串UTF8"
test_table_tbl.subxx_struct = tbl
test_table_tbl.sub_table  = tbl

test_table_tbl.sub_uion      = { y = false }
test_table_tbl.sub_uion_type = 2   -- you have to specific a uion type
test_table_tbl.vec_str       = { "abc","def","dfghijk" }

test_table_tbl.vec_table     =
{
    {
        bool_min = false,
        bool_max = true,
        int8_min = -128,
        int8_max = 128,
        uint8_min = 223,
        uint8_max = 223
    },

    {
        int16_min = 224,
        int16_max = 224,
        uint16_min = 225,
        uint16_max = 225,

        int32_min = 2234556,
        int32_max = 2234556,
        uint32_min = 12345567,
        uint32_max = 12345567
    },

    {
        int64_min = 99887766,
        int64_max = 99887766,
        uint64_min = 68932345,
        uint64_max = 68932345,

        float_min = 998998.12334567,
        float_max = 998998.987654321,

        double_min = 998.893837464,
        double_max = 998.893837464
    }
}

-- test_table_tbl.vec_struct = { tbl,tbl,tbl }
--
-- print( "start test test_table ..." )
-- local buffer = lfb:encode( "test.bfbs","test_table",test_table_tbl )
-- print( "decode done",string.len(buffer) )
--
-- local after_tbl = lfb:decode( "test.bfbs","test_table",buffer )
-- vd( after_tbl )
-- print( "end test test_table ..." )


--[[
inline flatbuffers::Offset<Monster> CreateMonster(flatbuffers::FlatBufferBuilder &_fbb, const MonsterT *_o) {
  return CreateMonster(_fbb,
    _o->pos ? _o->pos.get() : 0,
    _o->mana,
    _o->hp,
    _fbb.CreateString(_o->name),
    _o->inventory.size() ? _fbb.CreateVector(_o->inventory) : 0,
    _o->color,
    _o->test.type,
    _o->test.Pack(_fbb),
    _o->test4.size() ? _fbb.CreateVectorOfStructs(_o->test4) : 0,
    _o->testarrayofstring.size() ? _fbb.CreateVectorOfStrings(_o->testarrayofstring) : 0,
    _o->testarrayoftables.size() ? _fbb.CreateVector<flatbuffers::Offset<Monster>>(_o->testarrayoftables.size(), [&](size_t i) { return CreateMonster(_fbb, _o->testarrayoftables[i].get()); }) : 0,
    _o->enemy ? CreateMonster(_fbb, _o->enemy.get()) : 0,
    _o->testnestedflatbuffer.size() ? _fbb.CreateVector(_o->testnestedflatbuffer) : 0,
    _o->testempty ? CreateStat(_fbb, _o->testempty.get()) : 0,
    _o->testbool,
    _o->testhashs32_fnv1,
    _o->testhashu32_fnv1,
    _o->testhashs64_fnv1,
    _o->testhashu64_fnv1,
    _o->testhashs32_fnv1a,
    _o->testhashu32_fnv1a,
    _o->testhashs64_fnv1a,
    _o->testhashu64_fnv1a,
    _o->testarrayofbools.size() ? _fbb.CreateVector(_o->testarrayofbools) : 0,
    _o->testf,
    _o->testf2,
    _o->testf3,
    _o->testarrayofstring2.size() ? _fbb.CreateVectorOfStrings(_o->testarrayofstring2) : 0);
}


inline flatbuffers::Offset<Monster> CreateMonster(flatbuffers::FlatBufferBuilder &_fbb,
    const Vec3 *pos = 0,
    int16_t mana = 150,
    int16_t hp = 100,
    flatbuffers::Offset<flatbuffers::String> name = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> inventory = 0,
    Color color = Color_Blue,
    Any test_type = Any_NONE,
    flatbuffers::Offset<void> test = 0,
    flatbuffers::Offset<flatbuffers::Vector<const Test *>> test4 = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> testarrayofstring = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<Monster>>> testarrayoftables = 0,
    flatbuffers::Offset<Monster> enemy = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> testnestedflatbuffer = 0,
    flatbuffers::Offset<Stat> testempty = 0,
    bool testbool = false,
    int32_t testhashs32_fnv1 = 0,
    uint32_t testhashu32_fnv1 = 0,
    int64_t testhashs64_fnv1 = 0,
    uint64_t testhashu64_fnv1 = 0,
    int32_t testhashs32_fnv1a = 0,
    uint32_t testhashu32_fnv1a = 0,
    int64_t testhashs64_fnv1a = 0,
    uint64_t testhashu64_fnv1a = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> testarrayofbools = 0,
    float testf = 3.14159f,
    float testf2 = 3.0f,
    float testf3 = 0.0f,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> testarrayofstring2 = 0) {
  MonsterBuilder builder_(_fbb);
  builder_.add_testhashu64_fnv1a(testhashu64_fnv1a);
  builder_.add_testhashs64_fnv1a(testhashs64_fnv1a);
  builder_.add_testhashu64_fnv1(testhashu64_fnv1);
  builder_.add_testhashs64_fnv1(testhashs64_fnv1);
  builder_.add_testarrayofstring2(testarrayofstring2);
  builder_.add_testf3(testf3);
  builder_.add_testf2(testf2);
  builder_.add_testf(testf);
  builder_.add_testarrayofbools(testarrayofbools);
  builder_.add_testhashu32_fnv1a(testhashu32_fnv1a);
  builder_.add_testhashs32_fnv1a(testhashs32_fnv1a);
  builder_.add_testhashu32_fnv1(testhashu32_fnv1);
  builder_.add_testhashs32_fnv1(testhashs32_fnv1);
  builder_.add_testempty(testempty);
  builder_.add_testnestedflatbuffer(testnestedflatbuffer);
  builder_.add_enemy(enemy);
  builder_.add_testarrayoftables(testarrayoftables);
  builder_.add_testarrayofstring(testarrayofstring);
  builder_.add_test4(test4);
  builder_.add_test(test);
  builder_.add_inventory(inventory);
  builder_.add_name(name);
  builder_.add_pos(pos);
  builder_.add_hp(hp);
  builder_.add_mana(mana);
  builder_.add_testbool(testbool);
  builder_.add_test_type(test_type);
  builder_.add_color(color);
  return builder_.Finish();
}

  auto mloc = CreateMonster(builder, &vec, 150, 80, name, inventory, Color_Blue,
                            Any_Monster, mlocs[1].Union(), // Store a union.
                            testv, vecofstrings, vecoftables, 0, 0, 0, false,
                            0, 0, 0, 0, 0, 0, 0, 0, 0, 3.14159f, 3.0f, 0.0f,
                            vecofstrings2);
]]

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
monster.inventory = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }

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

local buffer  = lfb:encode( "test.bfbs","Monster",monster )
local mon_tbl = lfb:decode( "test.bfbs","Monster",buffer  )
vd( mon_tbl )
print( "start simple benchmark test" )
local max = 100000

local x = os.clock()

el_tbl =
{
    int8_min = -128,
    int8_max = 128,
    uint8_min = 256,
    uint8_max = 256,
    int16_min = 256,
    int16_max = 256,
    uint16_min = 256,
    uint16_max = 256,
    int32_min = 256,
    int32_max = 256,
    uint32_min = 256,
    uint32_max = 256,
    int64_min = 256,
    int64_max = 256,
    uint64_min = 256,
    uint64_max = 256,
    double_min = 256,
    double_max = 256,
    -- str = "abcdefghijklmnopqrstuvwxyz0123456789",
}
local ptbl = {}
ptbl.el    = { el_tbl,el_tbl,el_tbl,el_tbl,el_tbl }

local buffer
for index = 1,max do
    buffer = lfb:encode( "test.bfbs","test",ptbl )
end
local y = os.clock()
print( "end simple encode benchmark test",
    string.format("%d times elapsed time: %.2f\n",max,y - x))

local x = os.clock()
local pafter_tbl
for index = 1,max do
    pafter_tbl = lfb:decode( "test.bfbs","test",buffer )
end
local y = os.clock()
print( "end simple decode benchmark test",
    string.format("%d times elapsed time: %.2f\n",max,y - x))

--vd( lfb:decode( "test.bfbs","test",buffer ) )
