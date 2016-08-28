#ifndef __LFLATBUFFERS_H__
#define __LFLATBUFFERS_H__

#include <lua.hpp>
extern "C"
{
extern int luaopen_lua_flatbuffers( lua_State *L );
}

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/idl.h>

#include <stack>
#include <string>
#include <unordered_map>

class lflatbuffers
{
public:
    const char *last_error();
    bool load_bfbs_file( const char *file );
    int load_bfbs_path( const char *path,const char *postfix = NULL );
private:
    struct
    {
        std::string what;
        std::stack< std::string > backtrace;
    } _error_collector;
private:
    flatbuffers::FlatBufferBuilder _fbb;
    /* reflection::GetSchema cast string buffer to reflection::Schema
     * so you need to hold the string buffer
     */
    std::unordered_map< std::string,std::string > _bfbs_schema;
};

#endif /* __LFLATBUFFERS_H__ */
