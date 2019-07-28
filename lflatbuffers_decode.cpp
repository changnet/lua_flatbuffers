#include "lflatbuffers.hpp"

/* this function original at src\reflection.cpp
 * but somehow it do not declare at any header file
 * 2019-07-22 flatbuffers 1.11.0
 */
bool VerifyStruct(flatbuffers::Verifier &v,
                  const flatbuffers::Table &parent_table,
                  flatbuffers::voffset_t field_offset, const reflection::Object &obj,
                  bool required) {
  auto offset = parent_table.GetOptionalFieldOffset(field_offset);
  if (required && !offset) { return false; }

  return !offset ||
         v.Verify(reinterpret_cast<const uint8_t *>(&parent_table), offset,
                  obj.bytesize());
}

int lflatbuffers::decode( lua_State *L,
    const char *schema,const char *object,const char *buffer,size_t sz)
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

    /* seems no way to reuse a verifier
     * like flatbufferbuilder,we create a new one
     */
    const uint8_t *ubuffer = reinterpret_cast<const uint8_t *>( buffer );
    flatbuffers::Verifier vfer( ubuffer, sz );

    /* verify first 4bytes
     * root object offset or root table offset in doc/README.md
     *
     * uoffset_t VerifyOffset(size_t start)
     * 检验 buffer + start处的uoffset_t是否存在，并且读取这个值，再次校验这个
     * 是否合法
     */
    flatbuffers::uoffset_t root_offset = vfer.VerifyOffset( 0 );
    if ( !root_offset )
    {
        ERROR_WHAT( "invalid buffer,no root object offset" );
        _error_collector.schema = schema;
        _error_collector.object = object;
        return -1;
    }

    /* we cal verify the whole buffer once for all
     * flatbuffers::Verify(*_schema,*_object,ubuffer,sz)
     * is't it a little slow ?
     */

    /* now we can safely get root table
     * struct can NOT be root type
     * other field,like vtable offset will be verify later
     */
    const flatbuffers::Table* root = flatbuffers::GetAnyRoot(ubuffer);

/* release -Wunused-variable */
#ifndef NDEBUG
    int top = lua_gettop( L );
#endif

    if ( decode_table( L,_schema,_object,vfer,*root ) < 0 )
    {
        _error_collector.schema = schema;
        _error_collector.object = object;
        return -1;
    }

    /* make sure stack clean after decode */
    assert( top + 1 == lua_gettop(L) );
    return 0;
}

int lflatbuffers::decode_struct(
    lua_State *L,const reflection::Schema *schema,
    const reflection::Object *object,const flatbuffers::Struct &root )
{
    // struct is fixed,no need to verify it's field again

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
            case reflection::None  :
            case reflection::String:
            case reflection::Vector:
            case reflection::Union : assert( false );break;
            case reflection::Bool  :
            {
                int64_t val = flatbuffers::GetAnyFieldI( root, *field );

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
                int64_t val = flatbuffers::GetAnyFieldI( root, *field );

                lua_pushinteger( L,val );
            }break;
            case reflection::Float :
            case reflection::Double:
            {
                double val = flatbuffers::GetAnyFieldF( root, *field );

                lua_pushnumber( L,val );
            }break;
            case reflection::Obj:
            {
                auto *sub_object = schema->objects()->Get( type->index() );

                assert( sub_object->is_struct() );

                const flatbuffers::Struct *sub_root =
                    root.GetStruct<const flatbuffers::Struct*>( field->offset() );
                if ( decode_struct( L,schema,sub_object,*sub_root ) < 0 )
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
    const reflection::Object *object,flatbuffers::Verifier &vfer,const flatbuffers::Table &root )
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

    /* root maybe NULL or illegal data,but it's safe to call VerifyTableStart */
    if ( !root.VerifyTableStart( vfer ) )
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
                VERIFY_FIELD( flatbuffers::uoffset_t,root,field );

                const flatbuffers::String* str = flatbuffers::GetFieldS( root,*field );
                if ( !str ) // return nullptr if string field not exist
                {
                    lua_pop( L,1 ); /* pop the key */
                    continue;
                }
                lua_pushstring( L,str->c_str() );
            }break;
            case reflection::Obj:
            {
                bool required = field->required();
                auto *sub_object = schema->objects()->Get( type->index() );
                if ( sub_object->is_struct() )
                {
                    if ( !VerifyStruct(
                        vfer,root,field->offset(),*sub_object,required) )
                    {
                        ERROR_WHAT( "table field verify fail,not a invalid flatbuffer" );
                        ERROR_TRACE( field->name()->c_str() );
                        lua_pop( L,2 );
                        return -1;
                    }

                    const flatbuffers::Struct *sub_root
                        = flatbuffers::GetFieldStruct(root,*field);
                    if (!sub_root) // optional struct field
                    {
                        lua_pop( L,1 );
                        continue;
                    }

                    if ( decode_struct( L,schema,sub_object,*sub_root) < 0 )
                    {
                        lua_pop( L,2 );
                        return -1;
                    }
                }
                else
                {
                    const flatbuffers::Table *sub_root
                        = flatbuffers::GetFieldT(root,*field);
                    if (!sub_root && !required) // optional table field
                    {
                        lua_pop( L,1 );
                        continue;
                    }
                    if ( decode_table( L,schema,sub_object,vfer,*sub_root ) < 0 )
                    {
                        lua_pop( L,2 );
                        return -1;
                    }
                }
            }break;
            case reflection::Vector:
            {
                VERIFY_FIELD( flatbuffers::uoffset_t,root,field );
                const auto* vec = root.GetPointer<const flatbuffers::VectorOfAny*>( field->offset() );
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
                VERIFY_FIELD( flatbuffers::uoffset_t,root,field );

                const flatbuffers::Table *sub_root 
                    = flatbuffers::GetFieldT(root, *field);
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


                // union类型在写入数据后，必定会接着写入union类型。见doc/README.MD
                // 由于写内存是从高位到低位，每个变量占一个voffset_t
                // 因此只要确定union数据存在，可以直接取union的类型
                flatbuffers::voffset_t utype_offset 
                    = field->offset() - sizeof(flatbuffers::voffset_t);
                auto union_type = root.GetField<uint8_t>(utype_offset, 0);

                auto enumdef = schema->enums()->Get( type->index() );
                auto enumval = enumdef->values()->LookupByKey( union_type );
                assert( enumval );

                /* push union type(xx_type ) to lua,nesscery ?? */
                lua_pushstring ( L,union_key );
                lua_pushinteger( L,union_type );
                lua_rawset( L,-4 );

                 const auto *sub_object = enumval->object();
                 assert( sub_object && !sub_object->is_struct() );

                 if ( decode_table( L,schema,sub_object,vfer,*sub_root) < 0 )
                 {
                     lua_pop( L,2 );
                     return      -1;
                 }
            }break;
            case reflection::Bool  :
            {
                VERIFY_FIELD( uint8_t,root,field );
                uint8_t val = flatbuffers::GetFieldI<uint8_t>( root,*field );
                lua_pushboolean( L,val );
            }break;
            case reflection::UType :DECODE_INTEGER_FIELD(  uint8_t,root,field );break;
            case reflection::Byte  :DECODE_INTEGER_FIELD(   int8_t,root,field );break;
            case reflection::UByte :DECODE_INTEGER_FIELD(  uint8_t,root,field );break;
            case reflection::Short :DECODE_INTEGER_FIELD(  int16_t,root,field );break;
            case reflection::UShort:DECODE_INTEGER_FIELD( uint16_t,root,field );break;
            case reflection::Int   :DECODE_INTEGER_FIELD(  int32_t,root,field );break;
            case reflection::UInt  :DECODE_INTEGER_FIELD( uint32_t,root,field );break;
            case reflection::Long  :DECODE_INTEGER_FIELD(  int64_t,root,field );break;
            case reflection::ULong :DECODE_INTEGER_FIELD( uint64_t,root,field );break;
            case reflection::Float :DECODE_NUMBER_FIELD (    float,root,field );break;
            case reflection::Double:DECODE_NUMBER_FIELD (   double,root,field );break;
        }

        lua_rawset( L,-3 );
    }

    // Called at the end of a table to pop the depth count
    // if not true,something went wrong during verify fields.
    if ( !vfer.EndTable() )
    {
        ERROR_WHAT( "verify end table fail" );
        return -1;
    }
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

    if ( !vfer.VerifyVectorOrString(reinterpret_cast<const uint8_t*>(vec), sz) )
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

    if ( reflection::Byte != et && reflection::UByte != et )
    {
        lua_newtable( L );
        stack ++;
    }

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
                if ( !vfer.VerifyString( str ) )
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
                    const flatbuffers::Struct *sub_root =
                        flatbuffers::GetAnyVectorElemAddressOf<
                                const flatbuffers::Struct>( vec, index, sz );
                    if ( decode_struct( L,schema,sub_object,*sub_root ) < 0 )
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
                    const flatbuffers::Table *sub_root =
                        flatbuffers::GetAnyVectorElemPointer<
                                    const flatbuffers::Table>( vec, index );
                    if ( decode_table( L,schema,sub_object,vfer,*sub_root ) < 0 )
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
        case reflection::Byte  :
        case reflection::UByte :
        {
            /* binary vector,decode as lua string */
            lua_pushlstring( L,
                reinterpret_cast<const char*>(vec->Data()),vec->size() );
        }break;
        case reflection::UType :INTEGER_VECTOR( uint8_t);break;
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
