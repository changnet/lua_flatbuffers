#include <flatbuffers/flatbuffers.h>
#include "monster_test_generated.h"
#include <iostream> // C++ header file for printing
#include <fstream> // C++ header file for file access

#define TEST_EQ(src,dst) \
    do{\
        if ((src) != (dst)){\
            std::cerr << "test fail:" << __FUNCTION__ \
            << ":" << __LINE__ << std::endl;\
            return -1;\
        }\
    } while (0)

#define TEST_STR_EQ(src,dst) \
    do{\
        if (0 != strcmp((src) , (dst))){\
            std::cerr << "test fail:" << __FUNCTION__ \
            << ":" << __LINE__ << std::endl;\
            return -1;\
        }\
    } while (0)

int main()
{
    std::ifstream infile;
    infile.open("monsterdata_test.mon", std::ios::binary | std::ios::in);
    infile.seekg(0,std::ios::end);
    int length = infile.tellg();
    infile.seekg(0,std::ios::beg);
    char *data = new char[length];
    infile.read(data, length);
    infile.close();

    std::cout << "read " << length << " from file,start verify ..." << std::endl;

    flatbuffers::Verifier vfer(reinterpret_cast<const uint8_t*>(data), length);

    TEST_EQ( MyGame::Example::VerifyMonsterBuffer(vfer),true );


    auto monster = MyGame::Example::GetMonster(data);

    TEST_EQ( monster->mana() , 1500 );
    TEST_EQ( monster->hp() , 800 );
    TEST_STR_EQ( monster->name()->c_str(),"testMyMonster" );

    delete []data;
    return 0;
}