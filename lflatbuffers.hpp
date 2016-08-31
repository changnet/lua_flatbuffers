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
#include <vector>
#include <string>
#include <unordered_map>

class lflatbuffers
{
public:
    int encode( lua_State *L,const char *schema,const char *object,int index );
    int decode( lua_State *L,const char *schema,const char *object,int index );

    const char *last_error();
    bool load_bfbs_file( const char *file );
    int load_bfbs_path( const char *path,const char *postfix = NULL );
private:
    struct
    {
        std::string what;
        std::string schema;
        std::stack< std::string > backtrace;
    } _error_collector;

    struct sequence
    {
        const reflection::Field* field;
        const reflection::Object *object;
        std::vector< struct sequence > nested;/* table,vector,struct,string */
        std::vector< const reflection::Field* > scalar;/* int ... */

        sequence() : field(NULL),object(NULL) {}
    };

    typedef std::unordered_map< std::string,struct sequence > sequence_map;
    typedef std::unordered_map< std::string,sequence_map >      schema_map;

    void make_build_sequence(
        const char *schema_name,const reflection::Schema *schema );
    void make_object_sequence( const reflection::Schema *schema,
            struct sequence &seq,const reflection::Object *obj );
    int encode_struct(
        uint8_t *buffer,lua_State *L,const struct sequence &seq,int index );
    int encode_table( flatbuffers::uoffset_t &offset,
                    lua_State *L,const struct sequence &seq,int index );
    int encode_object( flatbuffers::uoffset_t &offset,
                    lua_State *L,const struct sequence &seq,int index );
private:
    flatbuffers::FlatBufferBuilder _fbb;
    /* reflection::GetSchema cast string buffer to reflection::Schema
     * so you need to hold the string buffer
     */
    std::unordered_map< std::string,std::string > _bfbs_buffer;
    schema_map _schema;
};

#endif /* __LFLATBUFFERS_H__ */
