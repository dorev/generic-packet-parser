# Generic Packet Parser

A header-only templated binary packet parser with a simple workflow:

* Create the expected packet fields
* Line them up in the expected order
* Parse directly to a `class` or `struct` or receive a relevant error message

A quick and flexible tool for prototyping:

```cpp
...

struct SubPacket {
    string name;
    uint32_t value;
    void setName(string s) { name = s; }
    void setValue(uint32_t v) { value = v; }
};

struct MyPacket {
    string name;
    uint32_t value;
    vector<SubPacket> array;
    void setName(string s) { name = s; }
    void setValue(uint32_t v) { value = v; }
    void addToArray(SubPacket& sp) { array.emplace_back(sp); }
};

...

auto parser = makePacketParser(
    TEXT_FIELD(&MyPacket::setName, 16),
    VALUE_FIELD(&MyPacket::setValue, uint32_t),
    DYNAMIC_ARRAY(uint8_t,
        MULTI_FIELD(SubPacket, &MyPacket::addToArray,
            TEXT_FIELD_ALLOW_EMPTY(&SubPacket::setName, 16),
            VALUE_FIELD_ENDIAN(&SubPacket::setValue, uint32_t))));

MyPacket output{"", 0};
PacketParserErrorId error = parser.parse(data, length, output);

...  
```



## Supported field types

### `ValueField`

...

### `TextField`

...

### `BinaryField`

... *not implemented yet*

### `MultiField`

...

### `DynamicFieldArray`

...

### `StaticFieldArray`

... *not implemented yet*

