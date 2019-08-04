#include <flatbuffers/flatbuffers.h>
#include "monster_test_generated.h"
#include <iostream> // C++ header file for printing
#include <fstream> // C++ header file for file access


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

    flatbuffers::Verifier vfer(reinterpret_cast<const uint8_t*>(data), length);
    assert( MyGame::Example::VerifyMonsterBuffer(vfer) );


    auto monster = MyGame::Example::GetMonster(data);

    delete []data;
    return 0;
}