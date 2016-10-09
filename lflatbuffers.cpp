#include "lflatbuffers.hpp"

#include <flatbuffers/util.h>

#include <cerrno>
#include <iostream>

/* linux open dir */
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_NESTED 128
#define UNION_KEY_LEN 64
#define MAX_LUA_STACK 256
#define LIB_NAME "lua_flatbuffers"

#define ERROR_WHAT(x)   _error_collector.what = x
#define ERROR_APPEND(x) _error_collector.what.append(x)
#define ERROR_TRACE(x)  _error_collector.backtrace.push_back(x)

/* check if suffix match */
static int is_suffix_file( const char *path,const char *suffix )
{
    /* simply check,not consider file like ./subdir/.bfbs */
    size_t sz = strlen( suffix );
    size_t ps = strlen( path );

    /* file like .bfbs will be ignore */
    if ( ps <= sz + 2 ) return 0;

    if ( '.' == path[ps-sz-1] && 0 == strcmp( path + ps - sz,suffix ) )
    {
        return true;
    }

    return false;
}

/* load all the schema files in this directory */
int lflatbuffers::load_bfbs_path( const char *path,const char *suffix )
{
    DIR *dir = opendir( path );
    if ( !dir )
    {
        ERROR_WHAT( "can not open directory:" );
        ERROR_APPEND( path );
        ERROR_APPEND( "," );
        ERROR_APPEND( strerror(errno) );

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
            && is_suffix_file( dt->d_name,suffix) )
        {
            if ( !load_bfbs_file( dt->d_name ) )
            {
                _bfbs_buffer.clear();
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
    std::string &bfbs = _bfbs_buffer[file];
    if ( !flatbuffers::LoadFile( file,true,&bfbs ) )
    {
        _bfbs_buffer.erase( file );

        ERROR_WHAT( "can not load file:" );
        ERROR_APPEND( file );
        ERROR_APPEND( "," );
        ERROR_APPEND( strerror(errno) );

        return false;
    }

    flatbuffers::Verifier encode_verifier(
        reinterpret_cast<const uint8_t *>(bfbs.c_str()), bfbs.length());
    if ( !reflection::VerifySchemaBuffer(encode_verifier) )
    {
        _bfbs_buffer.erase( file );

        ERROR_WHAT( "invalid flatbuffers binary schema file:" );
        ERROR_APPEND( file );

        return false;
    }

    return true;
}

const char *lflatbuffers::last_error()
{
    std::string &buffer = _error_collector.buffer;
    buffer = _error_collector.what;

    buffer.append( " at (" );
    buffer.append( _error_collector.schema );

    buffer.append( ")" );
    buffer.append( _error_collector.object );
    for ( auto itr = _error_collector.backtrace.rbegin();
            itr != _error_collector.backtrace.rend();itr ++ )
    {
        buffer.append( "."  );
        buffer.append( *itr );
    }

    return buffer.c_str();
}

int lflatbuffers::encode_struct(uint8_t *buffer,
    const reflection::Schema *schema,const reflection::Object *object,int index )
{
#define SET_INTEGER(T)   \
    do{\
        if ( !lua_isnumber( L,index + 1 ) )\
        {\
            ERROR_WHAT( "expect number,got " );\
            ERROR_APPEND( lua_typename( L,lua_type(L,index+1) ) );\
            ERROR_TRACE( field->name()->c_str() );\
            lua_pop( L,1 );\
            return -1;\
        }\
        int64_t val = lua_tointeger( L,index + 1 );\
        flatbuffers::WriteScalar(data, static_cast<T>(val));\
    }while(0)

#define SET_NUMBER(T)   \
    do{\
        if ( !lua_isnumber( L,index + 1 ) )\
        {\
            ERROR_WHAT( "expect number,got " );\
            ERROR_APPEND( lua_typename( L,lua_type(L,index+1) ) );\
            ERROR_TRACE( field->name()->c_str() );\
            lua_pop( L,1 );\
            return -1;\
        }\
        double val = lua_tonumber( L,index + 1 );\
        flatbuffers::WriteScalar(data, static_cast<T>(val));\
    }while(0)

    assert( object->is_struct() );

    assert( lua_gettop( L ) == index );

    const auto fields = object->fields();
    for ( auto itr = fields->begin();itr != fields->end();itr ++ )
    {
        const auto field = *itr;

        lua_getfield( L,index,field->name()->c_str() );
        if ( lua_isnil( L,index + 1 ) )
        {
            ERROR_WHAT( "missing required field" );
            ERROR_TRACE( field->name()->c_str() );

            lua_pop( L,1 );
            return -1;
        }

        const auto type = field->type();
        uint8_t *data = buffer + field->offset();
        switch ( type->base_type() )
        {
            case reflection::None: /* auto fall through */
            case reflection::String:
            case reflection::Vector:/* struct never contain those types */
            case reflection::Union: assert( false );break;
            case reflection::Obj:
            {
                /* struct contain struct*/
                const auto *sub_object = schema->objects()->Get( type->index() );
                assert( sub_object && sub_object->is_struct() );

                if ( encode_struct( data,schema,sub_object,index + 1 ) < 0 )
                {
                    ERROR_TRACE( field->name()->c_str() );
                    lua_pop( L,1 );
                    return -1;
                }
            }break;
            case reflection::Bool:
            {
                bool val = lua_toboolean( L,index + 1 );
                flatbuffers::WriteScalar( data, static_cast<uint8_t>(val) );
            }break;
            case reflection::UType: SET_INTEGER(uint8_t );break;
            case reflection::Byte:  SET_INTEGER(int8_t  );break;
            case reflection::UByte: SET_INTEGER(uint8_t );break;
            case reflection::Short: SET_INTEGER(int16_t );break;
            case reflection::UShort:SET_INTEGER(uint16_t);break;
            case reflection::Int:   SET_INTEGER(int32_t );break;
            case reflection::UInt:  SET_INTEGER(uint32_t);break;
            case reflection::Long:  SET_INTEGER(int64_t );break;
            case reflection::ULong: SET_INTEGER(uint64_t);break;
            case reflection::Float: SET_NUMBER (float   );break;
            case reflection::Double:SET_NUMBER (double  );break;
        }

        lua_pop( L,1 );
    }

    return 0;

#undef SET_INTEGER
#undef SET_NUMBER

}

int lflatbuffers::encode_vector( flatbuffers::uoffset_t &offset,
    const reflection::Schema *schema,const reflection::Field *field,int index )
{
#define UNCHECK(TYPE)
#define TYPE_CHECK(TYPE)    \
    do{\
        if ( !lua_is##TYPE( L,-1 ) )\
        {\
            ERROR_WHAT( "vector element expect "#TYPE",got " );\
            ERROR_APPEND( lua_typename(L, lua_type(L, -1)) );\
            lua_pop(L,1); return -1;\
        }\
    }while(0)

#define CREATE_VECTOR(T,TYPE,TC,CF)    \
    do{\
        std::vector<T> vt;\
        lua_pushnil( L );\
        while( lua_next( L,index ) )\
        {\
            vt.push_back( static_cast<T>(lua_to##TYPE(L,-1)) );\
            lua_pop( L,1 );\
        }\
        if ( !vt.empty() ) offset = _fbb.CF( vt ).o;\
    }while(0)

#define CREATE_BOOLEAN_VECTOR(T)    \
    CREATE_VECTOR(T,boolean,UNCHECK,CreateVector)
#define CREATE_NUMBER_VECTOR(T)    \
    CREATE_VECTOR(T,integer,TYPE_CHECK,CreateVector)
#define CREATE_STRING_VECTOR(T) \
    CREATE_VECTOR(T,string,TYPE_CHECK,CreateVectorOfStrings)

    /* element type could be scalar、table、struct、string */
    const auto type = field->type();
    switch( type->element() )
    {
        case reflection::None:
        case reflection::Vector:
        case reflection::Union : assert( false );break;

        case reflection::Bool:  CREATE_BOOLEAN_VECTOR(bool);break;
        case reflection::String:CREATE_STRING_VECTOR(std::string);break;
        case reflection::UType: CREATE_NUMBER_VECTOR(uint8_t );break;
        case reflection::Byte:  CREATE_NUMBER_VECTOR(int8_t  );break;
        case reflection::UByte: CREATE_NUMBER_VECTOR(uint8_t );break;
        case reflection::Short: CREATE_NUMBER_VECTOR(int16_t );break;
        case reflection::UShort:CREATE_NUMBER_VECTOR(uint16_t);break;
        case reflection::Int:   CREATE_NUMBER_VECTOR(int32_t );break;
        case reflection::UInt:  CREATE_NUMBER_VECTOR(uint32_t);break;
        case reflection::Long:  CREATE_NUMBER_VECTOR(int64_t );break;
        case reflection::ULong: CREATE_NUMBER_VECTOR(uint64_t);break;
        case reflection::Float: CREATE_NUMBER_VECTOR(float   );break;
        case reflection::Double:CREATE_NUMBER_VECTOR(double  );break;
        case reflection::Obj:
        {
            if ( !lua_istable( L,index ) )
            {
                ERROR_WHAT( "vector element expect table,got " );
                ERROR_APPEND( lua_typename(L, lua_type(L, -1)) );
                lua_pop(L,1); return -1;
            }

            auto *sub_object = schema->objects()->Get( type->index() );
            if ( sub_object->is_struct() )
            {
                assert( sub_object->bytesize() == field->offset() );

                /* when create vector,flatbuffers need to know element size and
                 * vector length to align
                 */
                lua_len( L,index );
                int length = lua_tointeger( L,index + 1 );
                lua_pop( L,1 );

                if ( length <= 0 ) { lua_pop( L,1 );return 0; }

                uint8_t *buffer = NULL;
                int32_t bytesize = sub_object->bytesize();
                offset = _fbb.CreateUninitializedVector( length,bytesize,&buffer );

                /* we use the length operator(lua_len,same as #) to get the array
                 * length.but we use lua_next insted using key 1...n to get element.
                 * so if someone want to use talbe as array,just implemente __len.
                 * please note:even your table's keys are 1..n,lua_next may not get
                 * it as 1...n.it's out of order,maybe 2,5,4,3...
                 * however,if you create a array with table.insert,if will be ordered.
                 */
                int sub_index = 0;
                lua_pushnil( L );
                while ( lua_next( L,index ) )
                {
                    uint8_t *sub_buffer = buffer + bytesize*sub_index;
                    if ( encode_struct( sub_buffer,schema,sub_object,index + 2 ) < 0 )
                    {
                        lua_pop( L,2 );
                        return      -1;
                    }

                    /* in case table key is not number */
                    sub_index ++;
                    if ( sub_index >= length ) { lua_pop( L,2 );break; }

                    lua_pop( L,1 );
                }

                return 0;
            }

            /* handle table vector here */
            std::vector< flatbuffers::Offset<void> > offsets;
            lua_pushnil( L );
            while ( lua_next( L,index ) )
            {
                flatbuffers::uoffset_t sub_offset = 0;
                if ( encode_table( sub_offset,schema,sub_object,index + 2 ) < 0 )
                {
                    lua_pop( L,2 );
                    return      -1;
                }

                lua_pop( L,1 );
            }
            offset = _fbb.CreateVector( offsets ).o;
        }break;
    }
    return 0;

#undef UNCHECK
#undef TYPE_CHECK
#undef CREATE_VECTOR
#undef CREATE_BOOLEAN_VECTOR
#undef CREATE_NUMBER_VECTOR
#undef CREATE_STRING_VECTOR
}

int lflatbuffers::encode_table( flatbuffers::uoffset_t &offset,
    const reflection::Schema *schema,const reflection::Object *object,int index )
{
#define CHECK_FIELD()   \
    do{\
        lua_getfield( L,index,field->name()->c_str() );\
        if ( lua_isnil( L,index + 1 ) )\
        {\
            lua_pop( L,1 );\
            continue; /* all field in table is optional */\
        }\
    }while(0)

#define TYPE_CHECK(TYPE)    \
    do{\
        if ( !lua_is##TYPE( L,index + 1 ) )\
        {\
            ERROR_WHAT( "expect "#TYPE",got " );\
            ERROR_APPEND( lua_typename(L, lua_type(L, index + 1)) );\
            lua_pop(L,1); return -1;\
        }\
    }while(0)

#define ADD_NUMBER(T)   \
    do{\
        if ( !lua_isnumber( L,index + 1 ) )\
        {\
            ERROR_WHAT( "expect number,got " );\
            ERROR_APPEND( lua_typename( L,lua_type(L,index + 1) ) );\
            ERROR_TRACE( field->name()->c_str() );\
            lua_pop( L,1 );\
            return      -1;\
        }\
        _fbb.AddElement<T>(off, static_cast<T>(lua_tonumber( L,index + 1 ) ),0 );\
    }while(0)

    assert( !object->is_struct() ); /* call encode struct insted */

    /* flatbuffers has to build in post-order.this make code a little mess up.
     * we have to iterate fields to built nested field first,to avoid memory
     * allocate,we do't use std::vector< std::pair<uint16_t,flatbuffers::uoffset_t> >
     * one object may contain MAX_NESTED(128) nested fields max.
     */
    typedef struct { uint16_t offset;flatbuffers::uoffset_t uoffset; } offset_pair;

    int nested_count = 0;
    offset_pair nested_offset[MAX_NESTED];

    const auto fields = object->fields();
    for ( auto itr = fields->begin();itr != fields->end();itr ++ )
    {
        const auto field = *itr; //reflection::Field
        assert( field );

        const auto type = field->type();
        flatbuffers::uoffset_t one_nested_offset = 0;
        switch ( field->type()->base_type() )
        {
            case reflection::None: assert( false );break;
            /* we handle scalar type later */
            case reflection::Bool:
            case reflection::UType:
            case reflection::Byte:
            case reflection::UByte:
            case reflection::Short:
            case reflection::UShort:
            case reflection::Int:
            case reflection::UInt:
            case reflection::Long:
            case reflection::ULong:
            case reflection::Float:
            case reflection::Double:continue;break;

            case reflection::String:
            {
                CHECK_FIELD();
                TYPE_CHECK(string);

                size_t len = 0;
                const char *str = lua_tolstring( L,index + 1,&len );
                one_nested_offset = _fbb.CreateString( str,len ).o;
            }break;
            case reflection::Vector:
            {
                CHECK_FIELD();
                TYPE_CHECK(table);

                if ( encode_vector( one_nested_offset,schema,field,index + 1 ) < 0 )
                {
                    lua_pop( L,1 );
                    return      -1;
                }
            }break;
            case reflection::Union:
            {
                CHECK_FIELD();
                TYPE_CHECK(table);

                /* For union fields, flatbuffers add a second auto-generated
                 * field to hold the type,with a special suffix.so when you build
                 * a table contain a union field,you have to specified the value
                 * of this field.
                 */
                char union_key[UNION_KEY_LEN];
                snprintf( union_key,UNION_KEY_LEN,"%s%s",field->name()->c_str(),
                    flatbuffers::UnionTypeFieldSuffix() );
                lua_pushstring( L,union_key );
                lua_gettable  ( L,index ); /* not index + 1 */
                if ( !lua_isinteger( L,index + 2 ) )
                {
                    ERROR_WHAT( "union type not specified,expect integer,got " );
                    ERROR_APPEND( lua_typename(L, lua_type(L, index + 2)) );
                    lua_pop(L,2); return -1;
                }

                int union_type = lua_tointeger( L,index + 2 );
                lua_pop( L,1 ); /* pop union type */

                /* here we encode union element,the union type should be encode
                 * later with other scalar type
                 */
                const auto *enums = schema->enums()->Get( type->index() );
                const auto *enumval = enums->values()->LookupByKey( union_type );
                if ( !enumval )
                {
                    ERROR_WHAT( "no such union type" );
                    lua_pop(L,2); return -1;
                }

                if ( encode_table( one_nested_offset,schema,enumval->object(),index + 1 ) < 0 )
                {
                    ERROR_TRACE( field->name()->c_str() );

                    lua_pop( L,1 );
                    return      -1;
                }
            }break;
            case reflection::Obj:
            {
                CHECK_FIELD();
                auto *sub_object = schema->objects()->Get( type->index() );
                if ( encode_object( one_nested_offset,schema,sub_object,index + 1 ) < 0 )
                {
                    ERROR_TRACE( field->name()->c_str() );

                    lua_pop( L,1 );
                    return      -1;
                }
            }break;
        }

        auto &nf   = nested_offset[nested_count];
        nf.offset  = field->offset();
        nf.uoffset = one_nested_offset;
        nested_count ++;

        lua_pop( L,1 ); /* pop the value which push at CHECK_FIELD */
    }

    flatbuffers::uoffset_t start = _fbb.StartTable();
    for ( auto itr = fields->begin();itr != fields->end();itr ++ )
    {
        const auto field = *itr;

        uint16_t off = field->offset();
        switch ( field->type()->base_type() )
        {
            case reflection::None: assert( false );break;

            /* object shouble be handled at nested */
            case reflection::String:
            case reflection::Vector:
            case reflection::Union :
            case reflection::Obj   : continue;break;
            case reflection::Bool:
            {
                CHECK_FIELD();
                bool bool_val = lua_toboolean( L,index + 1 );
                _fbb.AddElement<uint8_t>( off, bool_val,0 );
            }break;
            case reflection::UType: CHECK_FIELD();ADD_NUMBER(uint8_t );break;
            case reflection::Byte:  CHECK_FIELD();ADD_NUMBER(int8_t  );break;
            case reflection::UByte: CHECK_FIELD();ADD_NUMBER(uint8_t );break;
            case reflection::Short: CHECK_FIELD();ADD_NUMBER(int16_t );break;
            case reflection::UShort:CHECK_FIELD();ADD_NUMBER(uint16_t);break;
            case reflection::Int:   CHECK_FIELD();ADD_NUMBER(int32_t );break;
            case reflection::UInt:  CHECK_FIELD();ADD_NUMBER(uint32_t);break;
            case reflection::Long:  CHECK_FIELD();ADD_NUMBER(int64_t );break;
            case reflection::ULong: CHECK_FIELD();ADD_NUMBER(uint64_t);break;
            case reflection::Float: CHECK_FIELD();ADD_NUMBER(float   );break;
            case reflection::Double:CHECK_FIELD();ADD_NUMBER(double  );break;
        }

        lua_pop( L,1 ); /* pop the value which push at CHECK_FIELD */
    }

    for ( int index = 0;index < nested_count;index ++ )
    {
        _fbb.AddOffset( nested_offset[index].offset,
            flatbuffers::Offset<void>(nested_offset[index].uoffset) );
    }

    offset = _fbb.EndTable( start,fields->size() );

    return 0;

#undef ADD_INTEGER
#undef ADD_NUMBER
#undef CHECK_FIELD
#undef TYPE_CHECK

}

/* encode into a flatbuffers object( struct or table ) */
int lflatbuffers::encode_object( flatbuffers::uoffset_t &offset,
    const reflection::Schema *schema,const reflection::Object *object,int index )
{

    if ( object->is_struct() )
    {
        /* "structs" are flat structures that do not have an offset table
         * always have all members present.so we reserve a flat buffer at
         * FlatBufferBuilder,then fill every member.
         */
        _fbb.StartStruct( object->minalign() );
        uint8_t* buffer = _fbb.ReserveElements( object->bytesize(), 1 );
        if ( encode_struct( buffer,schema,object,index ) < 0 )
        {
            return -1;
        }
        offset =  _fbb.EndStruct();

        return 0;
    }

    return encode_table( offset,schema,object,index );
}

int lflatbuffers::encode( lua_State *L,
    const char *schema,const char *object,int index )
{
    auto sch_itr = _bfbs_buffer.find( schema );
    if ( sch_itr == _bfbs_buffer.end() )
    {
        _error_collector.what = "no such schema";
        return -1;
    }

    /* casting into a schema pointer */
    const auto *_schema = reflection::GetSchema( sch_itr->second.c_str() );

    /* do a bsearch */
    const auto *_object = _schema->objects()->LookupByKey( object );
    if ( !_object )
    {
        ERROR_WHAT( "no such object(" );
        ERROR_APPEND( object );
        ERROR_APPEND( ") at schema(" );
        ERROR_APPEND( schema );
        ERROR_APPEND( ")." );

        return -1;
    }

    /* Reset all the state in this FlatBufferBuilder so it can be reused
     * to construct another buffer
     */
    _error_collector.what.clear();
    _error_collector.backtrace.clear();
    _fbb.Clear();

    flatbuffers::uoffset_t offset;
    if ( encode_object( offset,_schema,_object,index ) <  0 )
    {
        _error_collector.schema = schema;
        _error_collector.object = object;
        return -1;
    }
    _fbb.Finish( flatbuffers::Offset<void>(offset) );

    return 0;
}

int lflatbuffers::decode( lua_State *L,
    const char *schema,const char *object,const char *buffer,size_t sz)
{
    auto sch_itr = _bfbs_buffer.find( schema );
    if ( sch_itr == _bfbs_buffer.end() )
    {
        _error_collector.what = "no such schema";
        return -1;
    }

    /* casting into a schema pointer */
    const auto *_schema = reflection::GetSchema( sch_itr->second.c_str() );

    /* do a bsearch */
    const auto *_object = _schema->objects()->LookupByKey( object );
    if ( !_object )
    {
        ERROR_WHAT( "no such object(" );
        ERROR_APPEND( object );
        ERROR_APPEND( ") at schema(" );
        ERROR_APPEND( schema );
        ERROR_APPEND( ")." );

        return -1;
    }

    /* seems no way to reuse a verifier like flatbufferbuilder,we create a new one */
    flatbuffers::Verifier vfer( reinterpret_cast<const uint8_t *>( buffer ), sz );

    /* verify first 4bytes(root object offset or root table offset in doc/README.md) */
    if ( !vfer.Verify<flatbuffers::uoffset_t>( buffer ) )
    {
        ERROR_WHAT( "invalid buffer,no root object offset" );
        _error_collector.schema = schema;
        _error_collector.object = object;
        return -1;
    }

    /* verify root object offset */
    flatbuffers::uoffset_t root_offset = flatbuffers::EndianScalar(
            *reinterpret_cast<const flatbuffers::uoffset_t *>( buffer ) );
    if ( !vfer.Verify( buffer,root_offset ) )
    {
        ERROR_WHAT( "invalid buffer,no root object" );
        _error_collector.schema = schema;
        _error_collector.object = object;
        return -1;
    }
    /* now we can safely get root object
     * other field,like vtable offset will be verify later
     */
    const void* root = flatbuffers::GetRoot<void>(buffer);

    int top = lua_gettop( L );
    if ( decode_object( L,_schema,_object,vfer,root ) < 0 )
    {
        _error_collector.schema = schema;
        _error_collector.object = object;
        return -1;
    }

    /* make sure stack clean after decode */
    assert( top + 1 == lua_gettop(L) );
    return 0;
}

int lflatbuffers::decode_object( lua_State *L,const reflection::Schema *schema,
    const reflection::Object *object,flatbuffers::Verifier &vfer,const void *root )
{
    if ( object->is_struct() ) return decode_struct( L,schema,object,vfer,root );

    return decode_table( L,schema,object,vfer,root );
}

int lflatbuffers::decode_struct( lua_State *L,const reflection::Schema *schema,
    const reflection::Object *object,flatbuffers::Verifier &vfer,const void *root )
{
    if ( !vfer.Verify( root, object->bytesize() ) )
    {
        ERROR_WHAT( "struct verify fail,not a invalid flatbuffer" );
        return -1;
    }

    if ( lua_gettop( L ) > MAX_LUA_STACK )
    {
        ERROR_WHAT( "lua stack overflow" );
        return -1;
    }

    lua_checkstack( L,3 ); /* table,val and key */
    lua_newtable( L );

    const flatbuffers::Struct& st  =
        *reinterpret_cast<const flatbuffers::Struct*>( root );

    const auto fields = object->fields();
    for ( auto itr = fields->begin();itr != fields->end();itr ++ )
    {
        const auto field = *itr; //reflection::Field
        assert( field );

        lua_pushstring ( L,field->name()->c_str() );

        /* field->deprecated() ?? */
        const auto type = field->type();
        switch( type->base_type() )
        {
            case reflection::None  :
            case reflection::String:
            case reflection::Vector:
            case reflection::Union : assert( false );break;
            case reflection::Bool  :
            {
                int64_t val = flatbuffers::GetAnyFieldI( st, *field );

                lua_pushboolean( L,val );
            }break;
            case reflection::UType :
            case reflection::Byte  :
            case reflection::UByte :
            case reflection::Short :
            case reflection::UShort:
            case reflection::Int   :
            case reflection::UInt  :
            case reflection::Long  :
            case reflection::ULong :
            {
                int64_t val = flatbuffers::GetAnyFieldI( st, *field );

                lua_pushinteger( L,val );
            }break;
            case reflection::Float :
            case reflection::Double:
            {
                double val = flatbuffers::GetAnyFieldF( st, *field );

                lua_pushnumber( L,val );
            }break;
            case reflection::Obj   :
            {
                auto *sub_object = schema->objects()->Get( type->index() );

                assert( sub_object->is_struct() );

                const uint8_t* sub_root = st.GetAddressOf( field->offset() );
                if ( decode_struct( L,schema,sub_object,vfer,sub_root ) < 0 )
                {
                    ERROR_TRACE( sub_object->name()->c_str() );
                    lua_pop( L,2 );
                    return -1;
                }
            }break;
        }

        lua_rawset( L,-3 );
    }

    return 0;
}

/* decode table(in binary buffer) to a lua table
 * all fields in table are optional,we are able to tell wether a field exist in
 * table with GetOptionalFieldOffset.but GetField always return value(a default
 * one if the field not exist),so all field will be push to lua table.
 */
int lflatbuffers::decode_table( lua_State *L,const reflection::Schema *schema,
    const reflection::Object *object,flatbuffers::Verifier &vfer,const void *root )
{
#define VERIFY_FIELD(T,tbl,field)    \
    do{\
        bool verify = field->required() ?\
            tbl.VerifyFieldRequired<T>(vfer,field->offset()) :\
            tbl.VerifyField<T>(vfer,field->offset());\
        if ( !verify )\
        {\
            ERROR_WHAT( "table field verify fail,not a invalid flatbuffer" );\
            ERROR_TRACE( field->name()->c_str() );\
            lua_pop( L,2 ); return -1;\
        }\
    }while(0)

#define DECODE_INTEGER_FIELD(T,tbl,field)    \
    do{\
        VERIFY_FIELD(T,tbl,field);\
        T val = flatbuffers::GetFieldI<T>( tbl,*field );\
        lua_pushinteger( L,val );\
    }while(0)


#define DECODE_NUMBER_FIELD(T,tbl,field)    \
    do{\
        VERIFY_FIELD(T,tbl,field);\
        T val = flatbuffers::GetFieldF<T>( tbl,*field );\
        lua_pushnumber( L,val );\
    }while(0)

    const flatbuffers::Table &tbl =
        *reinterpret_cast<const flatbuffers::Table*>( root );
    /* root maybe NULL or illegal data,but it's safe to call VerifyTableStart */
    if ( !tbl.VerifyTableStart( vfer ) )
    {
        ERROR_WHAT( "table verify fail,not a invalid flatbuffer" );
        return -1;
    }


    if ( lua_gettop( L ) > MAX_LUA_STACK )
    {
        ERROR_WHAT( "lua stack overflow" );
        return -1;
    }

    lua_checkstack( L,3 ); /* table,val and key */
    lua_newtable( L );

    const auto fields = object->fields();
    for ( auto itr = fields->begin();itr != fields->end();itr ++ )
    {
        const auto field = *itr; //reflection::Field
        assert( field );

        lua_pushstring ( L,field->name()->c_str() );

        /* field->deprecated() ?? */
        const auto type = field->type();
        switch( type->base_type() )
        {
            case reflection::None  : assert( false );break;
            case reflection::String:
            {
                VERIFY_FIELD( flatbuffers::uoffset_t,tbl,field );

                const flatbuffers::String* str = flatbuffers::GetFieldS( tbl,*field );
                if ( !str ) // return nullptr if string field not exist
                {
                    lua_pop( L,1 ); /* pop the key */
                    continue;
                }
                lua_pushstring( L,str->c_str() );
            }break;
            case reflection::Obj   :
            {
                VERIFY_FIELD( flatbuffers::uoffset_t,tbl,field );

                auto *sub_object = schema->objects()->Get( type->index() );
                /* GetStruct get data pointer,a field don't have vtable
                 * GetPointer get data pointer,a field have vtable,like table
                 */
                const void *sub_root = sub_object->is_struct() ?
                    tbl.GetStruct<const void*>( field->offset() ) :
                    tbl.GetPointer<const void*>( field->offset() );
                if ( !sub_root ) // optional field
                {
                    lua_pop( L,1 );
                    continue;
                }
                if ( decode_object( L,schema,sub_object,vfer,sub_root ) < 0 )
                {
                    lua_pop( L,2 );
                    return -1;
                }
            }break;
            case reflection::Vector:
            {
                VERIFY_FIELD( flatbuffers::uoffset_t,tbl,field );
                const auto* vec = tbl.GetPointer<const flatbuffers::VectorOfAny*>( field->offset() );
                if ( !vec ) // optional field
                {
                    lua_pop( L,1 );
                    continue      ;
                }

                if ( decode_vector( L,schema,type,vfer,vec ) < 0 )
                {
                    ERROR_TRACE( field->name()->c_str() );
                    lua_pop( L,2 );
                    return      -1;
                }
            }break;
            case reflection::Union :
            {
                VERIFY_FIELD( flatbuffers::uoffset_t,tbl,field );

                const void *sub_root = tbl.GetPointer<const void*>( field->offset() );
                if ( !sub_root ) // optional field
                {
                    lua_pop( L,1 );
                    continue      ;
                }

                /* GetUnionType(in reflection.h) do most of the jobs here,but we
                 * need to do more
                 */
                char union_key[UNION_KEY_LEN];
                snprintf( union_key,UNION_KEY_LEN,"%s%s",field->name()->c_str(),
                    flatbuffers::UnionTypeFieldSuffix() );

                auto type_field = fields->LookupByKey( union_key );
                assert( type_field );
                auto union_type = flatbuffers::GetFieldI<uint8_t>( tbl, *type_field );

                auto enumdef = schema->enums()->Get( type->index() );
                auto enumval = enumdef->values()->LookupByKey( union_type );
                assert( enumval );

                /* push union type(xx_type ) to lua,nesscery ?? */
                lua_pushstring ( L,union_key );
                lua_pushinteger( L,union_type );
                lua_rawset( L,-4 );

                 const auto *sub_object = enumval->object();
                 assert( sub_object && !sub_object->is_struct() );

                 if ( decode_table( L,schema,sub_object,vfer,sub_root) < 0 )
                 {
                     lua_pop( L,2 );
                     return      -1;
                 }
            }break;
            case reflection::Bool  :
            {
                VERIFY_FIELD( uint8_t,tbl,field );
                uint8_t val = flatbuffers::GetFieldI<uint8_t>( tbl,*field );
                lua_pushboolean( L,val );
            }break;
            case reflection::UType :DECODE_INTEGER_FIELD(  uint8_t,tbl,field );break;
            case reflection::Byte  :DECODE_INTEGER_FIELD(   int8_t,tbl,field );break;
            case reflection::UByte :DECODE_INTEGER_FIELD(  uint8_t,tbl,field );break;
            case reflection::Short :DECODE_INTEGER_FIELD(  int16_t,tbl,field );break;
            case reflection::UShort:DECODE_INTEGER_FIELD( uint16_t,tbl,field );break;
            case reflection::Int   :DECODE_INTEGER_FIELD(  int32_t,tbl,field );break;
            case reflection::UInt  :DECODE_INTEGER_FIELD( uint32_t,tbl,field );break;
            case reflection::Long  :DECODE_INTEGER_FIELD(  int64_t,tbl,field );break;
            case reflection::ULong :DECODE_INTEGER_FIELD( uint64_t,tbl,field );break;
            case reflection::Float :DECODE_NUMBER_FIELD (    float,tbl,field );break;
            case reflection::Double:DECODE_NUMBER_FIELD (   double,tbl,field );break;
        }

        lua_rawset( L,-3 );
    }

    // Called at the end of a table to pop the depth count,always true
    vfer.EndTable();
    return 0;

#undef VERIFY_FIELD
#undef DECODE_INTEGER_FIELD
#undef DECODE_NUMBER_FIELD
}

int lflatbuffers::decode_vector( lua_State *L,const reflection::Schema *schema,
    const reflection::Type *type,flatbuffers::Verifier &vfer,const flatbuffers::VectorOfAny *vec )
{
/* GetAnyVectorElemI do the same job,just a little slow */
#define VECTOR_GET(T) flatbuffers::ReadScalar<T>(vec->Data() + sz * index)

#define INTEGER_VECTOR(T)    \
    do{\
        for ( unsigned int index = 0;index < vec->size();index ++ )\
        {\
            T val = VECTOR_GET( T );\
            lua_pushinteger( L,val );\
            lua_rawseti( L,stack,index + 1 );\
        }\
    }while(0)

#define NUMBER_VECTOR(T)    \
    do{\
        for ( unsigned int index = 0;index < vec->size();index ++ )\
        {\
            T val = VECTOR_GET( T );\
            lua_pushnumber( L,val );\
            lua_rawseti( L,stack,index + 1 );\
        }\
    }while(0)

    reflection::BaseType et = type->element();
    size_t sz = flatbuffers::GetTypeSizeInline( et,type->index(),*schema );

    const uint8_t* end;
    if ( !vfer.VerifyVector(reinterpret_cast<const uint8_t*>(vec), sz, &end) )
    {
        ERROR_WHAT( "verify vector fail,not a valid flatbuffer" );
        return -1;
    }

    int stack = lua_gettop( L );
    if ( stack > MAX_LUA_STACK )
    {
        ERROR_WHAT( "lua stack overflow" );
        return -1;
    }

    lua_checkstack( L,2 ); /* table,val */
    lua_newtable( L );
    stack ++;

    switch( et )
    {
        case reflection::None  :
        case reflection::Vector:
        case reflection::Union : assert( false );break;
        case reflection::String:
        {
            for ( unsigned int index = 0;index < vec->size();index ++ )
            {
                const auto *str = flatbuffers::GetAnyVectorElemPointer<
                                const flatbuffers::String>( vec,index );
                if ( !vfer.Verify( str ) )
                {
                    ERROR_WHAT( "verify string vector fail,not a valid flatbuffer" );
                    lua_pop( L,1 );return -1;
                }
                lua_pushstring( L,str->c_str() );
                lua_rawseti( L,stack,index + 1 );
            }
        }break;
        case reflection::Obj   :
        {
            const auto *sub_object = schema->objects()->Get( type->index() );
            if ( sub_object->is_struct() )
            {
                for ( unsigned int index = 0;index < vec->size();index ++ )
                {
                    const void *sub_root =
                        flatbuffers::GetAnyVectorElemAddressOf<
                                    const void>( vec, index, sz );
                    if ( decode_struct( L,schema,sub_object,vfer,sub_root ) < 0 )
                    {
                        lua_pop( L,1 );
                        return      -1;
                    }
                    assert( lua_gettop( L ) == stack + 1 );
                    lua_rawseti( L,stack,index + 1 );
                }
            }
            else
            {
                for ( unsigned int index = 0;index < vec->size();index ++ )
                {
                    const void *sub_root =
                        flatbuffers::GetAnyVectorElemPointer<
                                    const void>( vec, index );
                    if ( decode_table( L,schema,sub_object,vfer,sub_root ) < 0 )
                    {
                        lua_pop( L,1 );
                        return      -1;
                    }
                    assert( lua_gettop( L ) == stack + 1 );
                    lua_rawseti( L,stack,index + 1 );
                }
            }
        }break;
        case reflection::Bool  :
        {
            for ( unsigned int index = 0;index < vec->size();index ++ )
            {
                uint8_t val = VECTOR_GET( uint8_t );
                lua_pushboolean( L,val );
                lua_rawseti( L,stack,index + 1 );
            }
        }break;
        case reflection::UType :INTEGER_VECTOR( uint8_t);break;
        case reflection::Byte  :INTEGER_VECTOR(  int8_t);break;
        case reflection::UByte :INTEGER_VECTOR( uint8_t);break;
        case reflection::Short :INTEGER_VECTOR( int16_t);break;
        case reflection::UShort:INTEGER_VECTOR(uint16_t);break;
        case reflection::Int   :INTEGER_VECTOR( int32_t);break;
        case reflection::UInt  :INTEGER_VECTOR(uint32_t);break;
        case reflection::Long  :INTEGER_VECTOR( int64_t);break;
        case reflection::ULong :INTEGER_VECTOR(uint64_t);break;
        case reflection::Float :NUMBER_VECTOR (   float);break;
        case reflection::Double:NUMBER_VECTOR (  double);break;
    }

    return 0;

#undef VECTOR_GET
#undef INTEGER_VECTOR
#undef NUMBER_VECTOR
}

/* before this function,make sure you had successfully call encode */
const char *lflatbuffers::get_buffer( size_t &sz )
{
    sz = _fbb.GetSize();
    return reinterpret_cast< const char* >( _fbb.GetBufferPointer() );
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

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );

    lua_pushlstring( L,buffer,sz );
    return 1;
}

static int decode( lua_State *L )
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, LIB_NAME );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "encode argument #1 expect %s", LIB_NAME );
    }

    const char *schema = luaL_checkstring( L,2 );
    const char *object = luaL_checkstring( L,3 );

    size_t sz = 0;
    const char *buffer = luaL_checklstring( L,4,&sz );

    assert( buffer && sz > 0 );

    if ( (*lfb)->decode( L,schema,object,buffer,sz ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    /* a lua table will be return if success */
    return 1;
}

/* create a C++ object and push to lua stack */
static int __call( lua_State* L )
{
    /* lua调用__call,第一个参数是该元表所属的table.取构造函数参数要注意 */
    class lflatbuffers* obj = new class lflatbuffers( L );

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
