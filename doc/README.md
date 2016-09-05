#### flatbuffers类图
![class](flatbuffers_class.png)
#### schema文件与类的对应关系
![schema file](flatbuffers_class_schema.png)
#### flatbuffers的内存模型
！[flatbuffers memory](flatbuffers vtable.png)
#### vtable构成
引用internal.md里的一句话：
> The elements of a vtable are all of type voffset_t, which is a uint16_t. The first element is the size of the vtable in bytes, including the size element. The second one is the size of the object, in bytes (including the vtable offset). This size could be used for streaming, to know how many bytes to read to be able to access all inline fields of the object. The remaining elements are the N offsets, where N is the amount of fields declared in the schema when the code that constructed this buffer was compiled (thus, the size of the table is N + 2)

虚表都是voffset_t(uint16_t)类型。第一个元素是虚表的的大小，这个值包含本身。第二个元素是整个对象大小(包含虚表的偏移值)。...。剩下的元素是N个偏移值，N是schema文件里该对象所有字段的数量。所以，整个虚表的大小是N + 2。

现在来看一下代码，vtable是怎样创建的：
```cpp
  // 传入的是元素开始的偏移量及元素个数
  uoffset_t EndTable(uoffset_t start, voffset_t numfields) {
    // If you get this assert, a corresponding StartTable wasn't called.
    assert(nested);
    // Write the vtable offset, which is the start of any Table.
    // We fill it's value later.
    auto vtableoffsetloc = PushElement<soffset_t>(0);
    // Write a vtable, which consists entirely of voffset_t elements.
    // It starts with the number of offsets, followed by a type id, followed
    // by the offsets themselves. In reverse:
    buf_.fill(numfields * sizeof(voffset_t));
    auto table_object_size = vtableoffsetloc - start;
    assert(table_object_size < 0x10000);  // Vtable use 16bit offsets.
    PushElement<voffset_t>(static_cast<voffset_t>(table_object_size));
    PushElement<voffset_t>(FieldIndexToOffset(numfields));
    // Write the offsets into the table
    for (auto field_location = offsetbuf_.begin();
              field_location != offsetbuf_.end();
            ++field_location) {
      auto pos = static_cast<voffset_t>(vtableoffsetloc - field_location->off);
      // If this asserts, it means you've set a field twice.
      assert(!ReadScalar<voffset_t>(buf_.data() + field_location->id));
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
    // Fill the vtable offset we created above.
    // The offset points from the beginning of the object to where the
    // vtable is stored.
    // Offsets default direction is downward in memory for future format
    // flexibility (storing all vtables at the start of the file).
    WriteScalar(buf_.data_at(vtableoffsetloc),
                static_cast<soffset_t>(vt_use) -
                  static_cast<soffset_t>(vtableoffsetloc));

    nested = false;
    return vtableoffsetloc;
  }
```
http://www.cnblogs.com/menggucaoyuan/p/3801274.html
https://github.com/mzaks/FlatBuffersSwift/wiki/FlatBuffers-Explained
http://wenku.baidu.com/view/54aacd7e4afe04a1b171de7c
http://www.race604.com/flatbuffers-intro/
