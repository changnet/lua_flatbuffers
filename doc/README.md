#### flatbuffers类图
![class](flatbuffers_class.png)

#### schema文件与类的对应关系
![schema file](flatbuffers_class_schema.png)

#### flatbuffers的内存模型
![flatbuffers memory](flatbuffers_vtable.png)

root table offset类型为uoffset_t，即uint32_t

#### flatbuffers的创建过程

先写一个极其简单的schema文件test_cpp.fbs
```flatbuffers
table simple_table
{
    x:int;
}
```

生成C++头文件test_cpp_generated.h
```shell
flact --cpp test_cpp.fbs
```

写个简单的测试程序test_cpp.cpp
```cpp
#include <iostream>

#include <flatbuffers/util.h>
#include "test_cpp_generated.h"

int main()
{
    const char *file = "test_cpp.bin";
    flatbuffers::FlatBufferBuilder builder;

    auto st = Createsimple_table( builder,9 );
    builder.Finish( st );

    const char *bufferpointer =
        reinterpret_cast<const char *>(builder.GetBufferPointer());
    uint16_t sz = builder.GetSize();

    flatbuffers::SaveFile( file,bufferpointer,sz,true );

    std::cout << "encode finish,size is " << sz << std::endl;

    std::string buff;

    if ( !flatbuffers::LoadFile( file,true,&buff ) ) return -1;

    const simple_table *_st = flatbuffers::GetRoot<simple_table>(
        reinterpret_cast<const uint8_t *>(buff.c_str()) );

    std::cout << _st->x() << std::endl;
    return 0;
}
```

编译运行并生成二进制文件
```shell
g++ -std=c++11 -o test_cpp test_cpp.cpp -lflatbuffe
./test_cpp
```

查看生成的二进制文件，内容为
```shell
0c00 0000 0000 0600 0800 0400 0600 0000 0900 0000
```

分析创建流程
![flatbuffers serialize](flatbuffers_serialize.png)

根据代码分析vtable的创建
```cpp
  // 传入的是元素开始的偏移量及元素个数
  uoffset_t EndTable(uoffset_t start, voffset_t numfields) {
    // 检测是否嵌套(table都要求是嵌套的)
    assert(nested);
    // 写入root table中vtable的偏移值。因为现在不知道vtable的大小，先写个0占位
    auto vtableoffsetloc = PushElement<soffset_t>(0);
    // 写入各个字段(从当前位置到root table中对应字段)的偏移值，这里只是全部填充0占位
    buf_.fill(numfields * sizeof(voffset_t));
    auto table_object_size = vtableoffsetloc - start;
    assert(table_object_size < 0x10000);  // Vtable use 16bit offsets.
    // 写入root table的大小
    PushElement<voffset_t>(static_cast<voffset_t>(table_object_size));
    // 写入vtable的大小，固定为4bytes + numfields*2bytes,
    // 4bytes为本身加上root table的大小
    PushElement<voffset_t>(FieldIndexToOffset(numfields));
    // Write the offsets into the table
    for (auto field_location = offsetbuf_.begin();
              field_location != offsetbuf_.end();
            ++field_location) {
      auto pos = static_cast<voffset_t>(vtableoffsetloc - field_location->off);
      // If this asserts, it means you've set a field twice.
      assert(!ReadScalar<voffset_t>(buf_.data() + field_location->id));
      // 这里开始填充各个字段(从当前位置到root table中对应字段)的偏移值
      WriteScalar<voffset_t>(buf_.data() + field_location->id, pos);
    }
    offsetbuf_.clear();
    auto vt1 = reinterpret_cast<voffset_t *>(buf_.data());
    auto vt1_size = ReadScalar<voffset_t>(vt1);
    auto vt_use = GetSize();
    // See if we already have generated a vtable with this exact same
    // layout before. If so, make it point to the old one, remove this one.
    for (auto it = vtables_.begin(); it != vtables_.end(); ++it) {
      auto vt2 = reinterpret_cast<voffset_t *>(buf_.data_at(*it));
      auto vt2_size = *vt2;
      if (vt1_size != vt2_size || memcmp(vt2, vt1, vt1_size)) continue;
      vt_use = *it;
      buf_.pop(GetSize() - vtableoffsetloc);
      break;
    }
    // If this is a new vtable, remember it.
    if (vt_use == GetSize()) {
      vtables_.push_back(vt_use);
    }
    // 填充root table中vtable的偏移值，注意这个值是当前位置向左
    WriteScalar(buf_.data_at(vtableoffsetloc),
                static_cast<soffset_t>(vt_use) -
                  static_cast<soffset_t>(vtableoffsetloc));

    nested = false;
    return vtableoffsetloc;
  }
```

#### 数组
把上面的schema文件稍微修改一下：
```flatbuffers
table simple_table
{
    x:[bool];
}
```
当内容为
```json
{x:[true]}
{x:[true,true]}
```
二进制内容分别为:
```shell
0c00 0000 0000 0600 0800 0400 0600 0000 0400 0000 0100 0000 0100 0000

0c00 0000 0000 0600 0800 0400 0600 0000 0400 0000 0200 0000 0101 0000
```
可以看到，其内容与上面例子不同的是0200 0000 0101 0000，其中0200 0000是一个uoffset_t,
即一个uint32_t.而0101 0000则是bool(uint8_t)按4bytes对齐后的结果。

分析创建数组的函数
```cpp
  uoffset_t EndVector(size_t len) {
    assert(nested);  // Hit if no corresponding StartVector.
    nested = false;
    return PushElement(static_cast<uoffset_t>(len));
  }

  void StartVector(size_t len, size_t elemsize) {
    NotNested();
    nested = true;
    PreAlign<uoffset_t>(len * elemsize);
    PreAlign(len * elemsize, elemsize);  // Just in case elemsize > uoffset_t.
  }
```
PreAlign会先按uoffset_t对齐，这样如果数组元素(内存大小，不是值)小于等于uoffset_t时，就OK了，如bool类型。
但如果是一个自定义的元素(比如一个比较大的table)，PreAlign(len * elemsize, elemsize)就会重新再对齐一次。

从上面的例子可以看出，如果事先不知道数组的长度，创建数组并不容易，因为无法对齐。对于lua中的table，如果想
当作数组使用，就会有这个问题。

#### 联合体union

先将一个包含union的简单schema文件以c++编译出来：
```flatbuffers
table simple_table { x:[bool]; }

table simple_table2 { y:[bool]; }

union any { simple_table,simple_table2 }

table simple { x:any; }
```
```cpp
enum any {
  any_NONE = 0,
  any_simple_table = 1,
  any_simple_table2 = 2,
  any_MIN = any_NONE,
  any_MAX = any_simple_table2
};

inline const char **EnumNamesany() {
  static const char *names[] = { "NONE", "simple_table", "simple_table2", nullptr };
  return names;
}

inline flatbuffers::Offset<simple> Createsimple(flatbuffers::FlatBufferBuilder &_fbb,
    any x_type = any_NONE,
    flatbuffers::Offset<void> x = 0) {
  simpleBuilder builder_(_fbb);
  builder_.add_x(x);
  builder_.add_x_type(x_type);
  return builder_.Finish();
}

inline bool Verifyany(flatbuffers::Verifier &verifier, const void *union_obj, any type) {
  switch (type) {
    case any_NONE: return true;
    case any_simple_table: return verifier.VerifyTable(reinterpret_cast<const simple_table *>(union_obj));
    case any_simple_table2: return verifier.VerifyTable(reinterpret_cast<const simple_table2 *>(union_obj));
    default: return false;
  }
}
```
可以看到，创建一个包含union必须要传入类型（add_x_type）。最终在内存中也会包含一个类型字段。而对于这个字段的产生，是
flatbuffers在编译，自动加了一个虚拟的字段，见Parser::ParseField::idl_parser.cpp::592
```cpp
  ...
  if (type.base_type == BASE_TYPE_UNION) {
    // For union fields, add a second auto-generated field to hold the type,
    // with a special suffix.
    ECHECK(AddField(struct_def, name + UnionTypeFieldSuffix(),
                    type.enum_def->underlying_type, &typefield));
  }
  ...
```
对应的用法见reflection.h的GetUnionType函数。这些都是1.4.0版本后的函数，1.3.0查不到。
创建一个uion的时候，一定要指定这个字段。

#### 内存对齐

上面的例子中会发现有很多内存对齐的地方，内存对齐有时候不仅会浪费空间，还会增加代码复杂度。而Protocol Buffer却没有要求内存对齐。
这是因为flatbuffers本身的设计原因。flatbuffers为了追求速度，省去了反序列化的过程。取而代之的是根据vtable地址直接访问字段的值。
根据地址访问变量！这不就是指针么。没错，flatbuffers就是把它当指针了.flatbuffers源码中大量使用了reinterpret_cast，这意味
flatbuffers将来自网络的flatbuffers数据直接当作你在内存中创建的对象来使用。但是，你在内存中创建的对象是编译器经过内存对齐处理的，
所以flatbuffers数据也必须做内存对齐处理。同时你还可以想到，你接收的数据可能是来自不同平台(i686、arm...)，不同的语言(c++、java、swift...).
不同的平台不同的语言不同的编译器会导致内存对齐都可能不一样。为了让他们统一起来，flatbuffers可是做了很多工作的。

#### 结束语
上面只是分析一个很简单的例子，其他复杂的内容如内存对齐、vtable共享、table嵌套、数组等一
下子也说不清楚。如果你感兴趣，就自己去研究源码，下面的参考链接里有分析。下面还有几个注意点：
* 内存从高往低写，这是因为flatbuffers的结构设计如此，必须先创建子对象(Do this is depth-first order to build up a tree to the root)
* 小端存储，用WriteScalar、ReadScalar就不会有太大问题
* 存在内存对齐，这个可能会干扰你分析问题

#### 其他参考
https://github.com/mzaks/FlatBuffersSwift/wiki/FlatBuffers-Explained  
https://github.com/google/flatbuffers/blob/master/docs/source/Internals.md  
