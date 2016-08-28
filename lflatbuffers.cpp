#include "lflatbuffers.hpp"

#include <flatbuffers/util.h>

#include <cerrno>
#include <iostream>

/* linux open dir */
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>


#define LIB_NAME "lua_flatbuffers"

static int is_postfix_file( const char *path,const char *postfix )
{
    /* simply check,not consider file like ./subdir/.bfbs */
    size_t sz = strlen( postfix );
    size_t ps = strlen( path );

    /* file like .bfbs will be ignore */
    if ( ps <= sz + 2 ) return 0;

    if ( '.' == path[ps-sz-1] && 0 == strcmp( path + ps - sz,postfix ) )
    {
        return true;
    }

    return false;
}

int lflatbuffers::load_bfbs_path( const char *path,const char *postfix )
{
    DIR *dir = opendir( path );
    if ( !dir )
    {
        _error_collector.what = "can not open directory:";
        _error_collector.what.append( path );
        _error_collector.what.append( "," );
        _error_collector.what.append( strerror(errno) );

        return -1;
    }

    int count = 0;
    struct dirent *dt = NULL;
    while ( (dt = readdir( dir )) )
    {
        struct stat path_stat;
        stat( dt->d_name, &path_stat );

        /* dt->d_type == DT_REG not supported by all file system types */
        if ( S_ISREG( path_stat.st_mode ) //This is a regular file
            && is_postfix_file( dt->d_name,postfix) )
        {
            if ( !load_bfbs_file( dt->d_name ) )
            {
                _bfbs_schema.clear();
                return -1;
            }
            ++ count;
        }
    }
    return count;
}

bool lflatbuffers::load_bfbs_file( const char *file )
{
    /* if the file already loaded,consider it need to update,not dumplicate */
    std::string &bfbs = _bfbs_schema[file];
    if ( !flatbuffers::LoadFile( file,true,&bfbs ) )
    {
        _bfbs_schema.erase( file );

        _error_collector.what = "can not load file:";
        _error_collector.what.append( file );
        _error_collector.what.append( "," );
        _error_collector.what.append( strerror(errno) );

        return false;
    }

    flatbuffers::Verifier encode_verifier(
        reinterpret_cast<const uint8_t *>(bfbs.c_str()), bfbs.length());
    if ( !reflection::VerifySchemaBuffer(encode_verifier) )
    {
        _bfbs_schema.erase( file );

        _error_collector.what = "invalid flatbuffers binary schema file:";
        _error_collector.what.append( file );

        return false;
    }

    return true;
}

const char *lflatbuffers::last_error()
{
    return _error_collector.what.c_str();
}

int lflatbuffers::encode( lua_State *L,
    const char *schema,const char *object,int index )
{
    return 0;
}

int lflatbuffers::decode( lua_State *L,
    const char *schema,const char *object,int index )
{
    return 0;
}

/* ========================== static function for lua ======================= */

static int load_bfbs_path( lua_State *L )
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, LIB_NAME );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error(L, "load_bfbs_path argument #1 expect %s", LIB_NAME);
    }

    const char *path = luaL_checkstring( L,2 );
    const char *postfix = luaL_optstring( L,3,"bfbs" );

    int count = (*lfb)->load_bfbs_path( path,postfix );
    if ( count < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    lua_pushinteger( L,count );
    return 1;
}

static int load_bfbs_file( lua_State *L )
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, LIB_NAME );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error(L, "load_bfbs_file argument #1 expect %s", LIB_NAME);
    }

    const char *path = luaL_checkstring( L,2 );
    if ( !(*lfb)->load_bfbs_file( path ) )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    lua_pushinteger( L,1 );
    return 1;
}

static int encode( lua_State *L )
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, LIB_NAME );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "encode argument #1 expect %s", LIB_NAME );
    }

    const char *schema = luaL_checkstring( L,2 );
    const char *object = luaL_checkstring( L,3 );
    if ( !lua_istable( L,4) )
    {
        return luaL_error( L,"argument #1 expect table,got %s",
            lua_typename(L, lua_type(L, 4)) );
    }

    if ( (*lfb)->encode( L,schema,object,4 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }
    return 1;
}

static int decode( lua_State *L )
{
    return 1;
}

/* create a C++ object and push to lua stack */
static int __call( lua_State* L )
{
    /* lua调用__call,第一个参数是该元表所属的table.取构造函数参数要注意 */
    class lflatbuffers* obj = new class lflatbuffers();

    lua_settop( L,1 ); /* 清除所有构造函数参数,只保留元表 */

    class lflatbuffers** ptr =
        (class lflatbuffers**)lua_newuserdata(L, sizeof(class lflatbuffers*));
    *ptr = obj;

    /* 把新创建的userdata和元表交换堆栈位置 */
    lua_insert(L,1);

    /* 弹出元表,并把元表设置为userdata的元表 */
    lua_setmetatable(L, -2);

    return 1;
}

/* 元方法,__tostring */
static int __tostring( lua_State* L )
{
    class lflatbuffers** ptr = (class lflatbuffers**)luaL_checkudata(L, 1,LIB_NAME);
    if(ptr != NULL)
    {
        lua_pushfstring(L, "%s: %p", LIB_NAME, *ptr);
        return 1;
    }
    return 0;
}

/*  元方法,__gc */
static int __gc( lua_State* L )
{
    class lflatbuffers** ptr = (class lflatbuffers**)luaL_checkudata(L, 1,LIB_NAME);
    if ( *ptr != NULL ) delete *ptr;
    *ptr = NULL;

    return 0;
}

/* ====================LIBRARY INITIALISATION FUNCTION======================= */

int luaopen_lua_flatbuffers(lua_State *L)
{
    //luaL_newlib(L, lua_parson_lib);
    if ( 0 == luaL_newmetatable( L,LIB_NAME ) )
    {
        assert( false );
        return 0;
    }

    lua_pushcfunction(L, __gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, __tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pushcfunction(L, encode);
    lua_setfield(L, -2, "encode");

    lua_pushcfunction(L, decode);
    lua_setfield(L, -2, "decode");

    lua_pushcfunction(L, load_bfbs_path);
    lua_setfield(L, -2, "load_bfbs_path");

    lua_pushcfunction(L, load_bfbs_file);
    lua_setfield(L, -2, "load_bfbs_file");

    /* metatable as value and pop metatable */
    lua_pushvalue( L,-1 );
    lua_setfield(L, -2, "__index");

    lua_newtable( L );
    lua_pushcfunction(L, __call);
    lua_setfield(L, -2, "__call");
    lua_setmetatable( L,-2 );

    return 1;  /* return metatable */
}
