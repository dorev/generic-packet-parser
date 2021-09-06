#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

using namespace std;

struct SubPacket
{
    SubPacket(string n, uint16_t f)
        : name(n)
        , flags(f)
    {}

    string name;
    uint16_t flags;
};

struct MyPacket
{
    string name;
    uint32_t value;
    vector<SubPacket> array;

    void setName(string s) { name = s; }
    void setValue(uint32_t v) { value = v; }
    void emplaceInArray(string s, uint16_t f) { array.emplace_back(s, f);}
};

template <class SetterSignature>
struct TextField
{
    using Setter = SetterSignature;
    using ValueType = char*;

    TextField(Setter setter, int maxLength)
        : setter(setter)
        , length(maxLength)
    {
    }

    const Setter setter;
    const size_t length;
};

template <class T, class SetterSignature>
struct ValueField
{
    using Setter = SetterSignature;
    using ValueType = T;

    ValueField(Setter setter)
        : setter(setter)
        , length(sizeof(ValueType))
    {
    }

    const Setter setter;
    const size_t length;
};

template<class OutputType, class... Fields>
struct PacketParser
{
    using Data = const unsigned char*;
    tuple<Fields...> _fields;
    const static int _fieldCount = sizeof...(Fields);
    Data _data;
    size_t _length;
    size_t _offset;
    OutputType _output;
    bool _packetIsValid;

    template<class... Fields>
    PacketParser(Fields... fields)
        : _fields(fields...)
        , _data(nullptr)
        , _length(0)
        , _offset(0)
        , _packetIsValid(false)
    {
    }

    bool parse(Data data, size_t length, OutputType& output)
    {
        // Reset working values
        _offset = 0;
        _data = data;
        _length = length;
        _packetIsValid = true;
        _output = OutputType();

        processAllFields(make_index_sequence<_fieldCount>());

        if (_packetIsValid)
        {
            output = _output;
            return true;
        }

        return false;
    }

    template <size_t... I>
    void processAllFields(index_sequence<I...>)
    {
        (processOneField(get<I>(_fields)), ...);
    }

    template <class FieldType>
    void processOneField(FieldType& field)
    {
        // Keep processing fields as long as they are valid
        if(!_packetIsValid || !processBinary(field))
            return;
    }

    template <class FieldType>
    bool processBinary(FieldType& field)
    {
        size_t fieldLength = field.length;

        if constexpr (is_same<FieldType, TextField<FieldType:: template Setter>>::value)
        {
            // Null-terminator detection
            size_t nullTerminatorDistance = 0;
            if (!rangeContainsNullTerminator(_offset, _offset + field.length, nullTerminatorDistance)
                || nullTerminatorDistance <= 1)
            {
                return _packetIsValid = false;
            }

            // Update field length to increment _offset correctly
            fieldLength = nullTerminatorDistance;
        }

        callSetter<FieldType:: template ValueType>(field);
        _offset += fieldLength;
        return true;
    }

    bool rangeContainsNullTerminator(size_t beginOffset, size_t endOffset, size_t& nullTerminatorDistance)
    {
        nullTerminatorDistance = 0;
        for (size_t i = beginOffset; i < endOffset; ++i)
        {
            nullTerminatorDistance++;
            if (_data[i] == 0)
                return true;
        }
        return false;
    }

    template <class ValueType, class FieldType>
    void callSetter(FieldType field)
    {
        if constexpr(is_pointer<const ValueType>::value)
            // c-style cast to fake constexpr reinterpret_cast<const CastType>(...)
            (_output.*(field.setter))((const ValueType)(&_data[_offset]));
        else
            (_output.*(field.setter))(*(reinterpret_cast<const ValueType*>(&_data[_offset])));
    }
};

// =============================================================================
// TEST
// =============================================================================

int main()
{
    // Data
    const unsigned char data[] = {'H', 'e', 'l', 'l', 'o', '\0', 0x01, 0x01, 0x00, 0x00};
    size_t length = 10;

    // Declare fields and setters
    ValueField<uint32_t, decltype(&MyPacket::setValue)> vf(&MyPacket::setValue);
    TextField<decltype(&MyPacket::setName)> tf(&MyPacket::setName, 16);

    // Packet parser type
    // Requires decltypes to properly instanciate the tuple containing the fields
    // The ordering must be the same!
    PacketParser<MyPacket, decltype(tf), decltype(vf)>
        packetParser(tf, vf);

    MyPacket out;
    if (packetParser.parse(data, length, out))
    {
        cout << "Name: " << out.name << '\n'
            << "Value: " << out.value << '\n';
    }
}
