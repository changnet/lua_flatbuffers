#include "lflatbuffers.hpp"


int lflatbuffers::do_encode_struct(uint8_t *buffer,
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

    if ( lua_gettop( L ) > MAX_LUA_STACK )
    {
        ERROR_WHAT( "lua stack overflow" );

        return -1;
    }
    lua_checkstack( L,2 ); /* stack need to iterate lua table */

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

                if ( do_encode_struct( data,schema,sub_object,index + 1 ) < 0 )
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
            TC( TYPE );\
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

    if ( lua_gettop( L ) > MAX_LUA_STACK )
    {
        ERROR_WHAT( "lua stack overflow" );

        return -1;
    }
    lua_checkstack( L,2 ); /* stack need to iterate lua table */

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
        case reflection::Short: CREATE_NUMBER_VECTOR(int16_t );break;
        case reflection::UShort:CREATE_NUMBER_VECTOR(uint16_t);break;
        case reflection::Int:   CREATE_NUMBER_VECTOR(int32_t );break;
        case reflection::UInt:  CREATE_NUMBER_VECTOR(uint32_t);break;
        case reflection::Long:  CREATE_NUMBER_VECTOR(int64_t );break;
        case reflection::ULong: CREATE_NUMBER_VECTOR(uint64_t);break;
        case reflection::Float: CREATE_NUMBER_VECTOR(float   );break;
        case reflection::Double:CREATE_NUMBER_VECTOR(double  );break;
        case reflection::Byte:
        case reflection::UByte:
        {
            TYPE_CHECK( string );

            size_t sz = 0;
            const char *str = lua_tolstring( L,index,&sz );
            /* in lua,[byte] or [ubyte] can hold binary data and string,but not
             * a number vector.so int8_t or uint8_t is the same
             */
            offset = _fbb.CreateVector( reinterpret_cast<const uint8_t*>(str),sz ).o;
        }break;
        case reflection::Obj:
        {
            auto *sub_object = schema->objects()->Get( type->index() );
            if ( sub_object->is_struct() )
            {
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
                    TYPE_CHECK( table );
                    uint8_t *sub_buffer = buffer + bytesize*sub_index;
                    if ( do_encode_struct( sub_buffer,schema,sub_object,index + 2 ) < 0 )
                    {
                        lua_pop( L,2 );
                        return      -1;
                    }

                    /* in case table key is not number */
                    sub_index ++;
                    if ( sub_index >= length ) { lua_pop( L,2 );break; }

                    lua_pop( L,1 );
                }

                break;
            }

            /* handle table vector here */
            std::vector< flatbuffers::Offset<void> > offsets;
            lua_pushnil( L );
            while ( lua_next( L,index ) )
            {
                TYPE_CHECK( table );
                flatbuffers::uoffset_t sub_offset = 0;
                if ( encode_table( sub_offset,schema,sub_object,index + 2 ) < 0 )
                {
                    lua_pop( L,2 );
                    return      -1;
                }

                lua_pop( L,1 );
                offsets.push_back( sub_offset );
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
/* this macro contain continue,don't use do ... while(0) */
#define CHECK_FIELD()   \
    {\
        lua_getfield( L,index,field->name()->c_str() );\
        if ( lua_isnil( L,index + 1 ) )\
        {\
            lua_pop( L,1 );\
            continue; /* all field in table is optional */\
        }\
    }

#define TYPE_CHECK(TYPE)    \
    do{\
        if ( !lua_is##TYPE( L,index + 1 ) )\
        {\
            ERROR_WHAT( "expect "#TYPE",got " );\
            ERROR_APPEND( lua_typename(L, lua_type(L, index + 1)) );\
            ERROR_TRACE( field->name()->c_str() );\
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


    if ( lua_gettop( L ) > MAX_LUA_STACK )
    {
        ERROR_WHAT( "lua stack overflow" );

        return -1;
    }
    lua_checkstack( L,2 ); /* stack need to iterate lua table */

    /* flatbuffers has to build in post-order.this make code a little mess up.
     * we have to iterate fields to built nested field first,to avoid memory
     * allocate,we use array insted of
     * std::vector< std::pair<uint16_t,flatbuffers::uoffset_t> >
     * so one object may contain MAX_NESTED(128) nested fields max.
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
        switch ( type->base_type() )
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

                int element = type->element();
                if ( reflection::Byte == element || reflection::UByte == element )
                {
                    TYPE_CHECK(string);
                }
                else
                {
                    TYPE_CHECK(table);
                }

                if ( encode_vector( one_nested_offset,schema,field,index + 1 ) < 0 )
                {
                    ERROR_TRACE( field->name()->c_str() );
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
                    ERROR_TRACE( field->name()->c_str() );

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
                    ERROR_TRACE( field->name()->c_str() );

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
                auto *sub_object = schema->objects()->Get( type->index() );
                if ( sub_object->is_struct() ) continue;

                CHECK_FIELD();
                int rts = encode_table( one_nested_offset,schema,sub_object,index + 1 );
                if ( rts < 0 )
                {
                    ERROR_TRACE( field->name()->c_str() );

                    lua_pop( L,1 );
                    return      -1;
                }
            }break;
        }

        if ( nested_count >= MAX_NESTED )
        {
            ERROR_WHAT( "too many nested field" );
            ERROR_TRACE( field->name()->c_str() );

            lua_pop( L,1 );
            return      -1;
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
            case reflection::Union : continue;break;
            case reflection::Obj   :
            {
                auto *sub_object = schema->objects()->Get( field->type()->index() );
                if ( !sub_object->is_struct() ) continue;

                CHECK_FIELD();

                flatbuffers::uoffset_t offset = 0;
                if ( encode_struct( offset,schema,sub_object,index + 1 ) < 0 )
                {
                    ERROR_TRACE( field->name()->c_str() );

                    lua_pop( L,1 );
                    return      -1;
                }
                _fbb.AddStructOffset( off,offset );
            }break;
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

int lflatbuffers::encode_struct( flatbuffers::uoffset_t &offset,
    const reflection::Schema *schema,const reflection::Object *object,int index )
{
    assert( object->is_struct() );
    /* "structs" are flat structures that do not have an offset table
     * always have all members present.so we reserve a flat buffer at
     * FlatBufferBuilder,then fill every member.
     */
    _fbb.StartStruct( object->minalign() );
    uint8_t* buffer = _fbb.ReserveElements( object->bytesize(), 1 );
    if ( do_encode_struct( buffer,schema,object,index ) < 0 )
    {
        return -1;
    }
    offset =  _fbb.EndStruct();

    return 0;
}

int lflatbuffers::encode( lua_State *L,
    const char *schema,const char *object,int index )
{
    auto sch_itr = _bfbs_buffer.find( schema );
    if ( sch_itr == _bfbs_buffer.end() )
    {
        ERROR_WHAT( "no such schema:" );
        ERROR_APPEND( schema );

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

    int rts = _object->is_struct() ?
        encode_struct( offset,_schema,_object,index ) :
        encode_table ( offset,_schema,_object,index ) ;

    if ( rts <  0 )
    {
        _error_collector.schema = schema;
        _error_collector.object = object;
        return -1;
    }
    _fbb.Finish( flatbuffers::Offset<void>(offset) );

    return 0;
}

/* before this function,make sure you had successfully call encode */
const char *lflatbuffers::get_buffer( size_t &sz )
{
    sz = _fbb.GetSize();
    return reinterpret_cast< const char* >( _fbb.GetBufferPointer() );
}
