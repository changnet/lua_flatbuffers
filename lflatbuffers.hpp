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
        std::vector< std::string > name;
        /* we don't have to reference object pointer here
         * but if we do,we do't need to call LookupByKey everytime
         */
        const reflection::Object* object;
    };

    typedef std::unordered_map< std::string,std::string > schema_map;
    typedef std::unordered_map< std::string,std::vector< struct sequence > > sequence_map;

    void make_build_sequence(
        const char *schema_name,const reflection::Schema *schema );
    int encode_object( flatbuffers::uoffset_t &offset,lua_State *L,
        const reflection::Schema *schema,const reflection::Object* object,int index );
private:
    flatbuffers::FlatBufferBuilder _fbb;
    /* reflection::GetSchema cast string buffer to reflection::Schema
     * so you need to hold the string buffer
     */
    schema_map _bfbs_schema;
    sequence_map _build_sequence;
};

#endif /* __LFLATBUFFERS_H__ */
