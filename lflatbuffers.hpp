#ifndef __LFLATBUFFERS_H__
#define __LFLATBUFFERS_H__

#include <lua.hpp>
extern "C"
{
extern int luaopen_lua_flatbuffers( lua_State *L );
}

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/idl.h>

#include <vector>
#include <string>
#include <unordered_map>

#define MAX_NESTED 128
#define UNION_KEY_LEN 64
#define MAX_LUA_STACK 256
#define LIB_NAME "lua_flatbuffers"

#define ERROR_WHAT(x)   _error_collector.what = x
#define ERROR_APPEND(x) _error_collector.what.append(x)
#define ERROR_TRACE(x)  _error_collector.backtrace.push_back(x)

class lflatbuffers
{
public:
    int encode( lua_State *L,const char *schema,const char *object,int index );
    int decode( lua_State *L,const char *schema,const char *object,const char *buffer,size_t sz );

    const char *last_error();
    const char *get_buffer( size_t &sz );
    bool load_bfbs_file( const char *file );
    int load_bfbs_path( const char *path,const char *postfix = "bfbs" );
private:
    struct
    {
        std::string what;
        std::string buffer;
        std::string schema;
        std::string object;
        std::vector< std::string > backtrace;
    } _error_collector;

    int encode_vector( lua_State *L,flatbuffers::uoffset_t &offset,
        const reflection::Schema *schema,const reflection::Field *field,int index );
    int encode_struct( lua_State *L,flatbuffers::uoffset_t &offset,
        const reflection::Schema *schema,const reflection::Object *object,int index );
    int do_encode_struct( lua_State *L,
        const reflection::Schema *schema,const reflection::Object *object,int index );
    int encode_table( lua_State *L,flatbuffers::uoffset_t &offset,
        const reflection::Schema *schema,const reflection::Object *object,int index );

    int decode_object( lua_State *L,const reflection::Schema *schema,
        const reflection::Object *object,flatbuffers::Verifier &vfer,const void *root );
    int decode_struct( lua_State *L,const reflection::Schema *schema,
        const reflection::Object *object,flatbuffers::Verifier &vfer,const void *root );
    int decode_table( lua_State *L,const reflection::Schema *schema,
        const reflection::Object *object,flatbuffers::Verifier &vfer,const void *root );
    int decode_vector( lua_State *L,const reflection::Schema *schema,
        const reflection::Type *type,flatbuffers::Verifier &vfer,const flatbuffers::VectorOfAny *vec );
private:
    flatbuffers::FlatBufferBuilder _fbb;
    /* reflection::GetSchema cast string buffer to reflection::Schema
     * so you need to hold the string buffer
     */
    std::unordered_map< std::string,std::string > _bfbs_buffer;
};

#endif /* __LFLATBUFFERS_H__ */
