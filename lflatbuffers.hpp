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

class lflatbuffers
{
public:
    ~lflatbuffers() {}
    explicit lflatbuffers( lua_State *L ) : L(L) {}
    int encode( lua_State *L,const char *schema,const char *object,int index );
    int decode( lua_State *L,const char *schema,const char *object,int index );

    const char *last_error();
    const char *get_buffer( size_t &sz );
    bool load_bfbs_file( const char *file );
    int load_bfbs_path( const char *path,const char *postfix = NULL );
private:
    struct
    {
        std::string what;
        std::string buffer;
        std::string schema;
        std::string object;
        std::vector< std::string > backtrace;
    } _error_collector;

    int encode_vector( flatbuffers::uoffset_t &offset,
        const reflection::Schema *schema,const reflection::Field *field,int index );
    int encode_struct(uint8_t *buffer,
        const reflection::Schema *schema,const reflection::Object *object,int index );
    int encode_table( flatbuffers::uoffset_t &offset,
        const reflection::Schema *schema,const reflection::Object *object,int index );
    int encode_object( flatbuffers::uoffset_t &offset,
        const reflection::Schema *schema,const reflection::Object *object,int index );
private:
    lua_State *L;
    flatbuffers::FlatBufferBuilder _fbb;
    /* reflection::GetSchema cast string buffer to reflection::Schema
     * so you need to hold the string buffer
     */
    std::unordered_map< std::string,std::string > _bfbs_buffer;
};

#endif /* __LFLATBUFFERS_H__ */
