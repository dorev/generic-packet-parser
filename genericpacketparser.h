#pragma once

#include <iostream>
#include <cassert>
#include <tuple>
#include <type_traits>

namespace GenericPacketParser
{

using namespace std;

enum class FieldTypeId
{
    TextField,
    ValueField,
    CompoundedField,
    DynamicFieldArray
};

enum class PacketParserErrorId
{
    NoError,
    InvalidText,
    InvalidValue,
    MissingNullTerminator,
    EmptyTextNotAllowed,
    ExceededDataRange,
    UnhandledFieldType,
    Unknown
};

ostream& operator<<(ostream& out, PacketParserErrorId error)
{
    switch (error)
    {
#define ERROR_TO_STREAM(value) case PacketParserErrorId::##value: out << string(#value); break;
        ERROR_TO_STREAM(NoError);
        ERROR_TO_STREAM(InvalidText);
        ERROR_TO_STREAM(InvalidValue);
        ERROR_TO_STREAM(MissingNullTerminator);
        ERROR_TO_STREAM(EmptyTextNotAllowed);
        ERROR_TO_STREAM(ExceededDataRange);
        ERROR_TO_STREAM(UnhandledFieldType);
        ERROR_TO_STREAM(Unknown);
#undef ERROR_TO_STREAM
    default:
        out << "Unknown error id"; break;
    }
    return out;
}

template <class T, size_t TypeSize = sizeof(T)>
struct EndiannessInverter;

template <class T>
struct EndiannessInverter<T, 2>
{
    static T call(const T value)
    {
        return ((value << 8) & 0xff00)
            | (( value >> 8) & 0x00ff);
    }
};

template <class T>
struct EndiannessInverter<T, 4>
{
    static T call(const T value)
    {
        return ((value << 24) & 0xff000000)
            | (( value << 8)  & 0x00ff0000)
            | (( value >> 8)  & 0x0000ff00)
            | (( value >> 24) & 0x000000ff);
    }
};

template <class T>
struct EndiannessInverter<T, 8>
{
    static T call(const T value)
    {
        return ((value << 56) & 0xff00000000000000)
            | (( value << 40) & 0x00ff000000000000)
            | (( value << 24) & 0x0000ff0000000000)
            | (( value << 8)  & 0x000000ff00000000)
            | (( value >> 8)  & 0x00000000ff000000)
            | (( value >> 24) & 0x0000000000ff0000)
            | (( value >> 40) & 0x000000000000ff00)
            | (( value >> 56) & 0x00000000000000ff);
    }
};

template <class SetterSignature, bool AllowEmpty = false>
struct TextField
{
    using Setter = SetterSignature;
    using ValueType = char*;
    static constexpr bool allowEmpty = AllowEmpty;
    static constexpr FieldTypeId typeId = FieldTypeId::TextField;

    TextField(Setter setter, int maxLength)
        : setter(setter)
        , length(maxLength)
    {
        assert(("Text length must be greater than 0.", length > 0));
    }

    const Setter setter;
    const size_t length;
};

template <class T, class SetterSignature, bool InvertEndianness = false>
struct ValueField
{
    using ValueType = T;
    using Setter = SetterSignature;
    static constexpr FieldTypeId typeId = FieldTypeId::ValueField;
    static constexpr bool invertEndianness = InvertEndianness;

    ValueField(Setter setter)
        : setter(setter)
        , length(sizeof(ValueType))
    {
    }

    const Setter setter;
    const size_t length;
};

template <class OutputType, class SetterSignature, class... Fields>
struct CompoundedField
{
    using ValueType = OutputType;
    using Setter = SetterSignature;
    static constexpr size_t fieldCount = sizeof...(Fields);
    static constexpr FieldTypeId typeId = FieldTypeId::CompoundedField;

    CompoundedField(Setter setter, Fields... fields)
        : setter(setter)
        , fields(fields...)
    {
    }

    Setter setter;
    tuple<Fields...> fields;
};

template <class T, class ArraySizeValueType>
struct DynamicFieldArray
{
    using ArrayFieldType = T;
    using ValueType = ArrayFieldType;
    using ArraySizeType = ArraySizeValueType;
    static constexpr FieldTypeId typeId = FieldTypeId::DynamicFieldArray;

    DynamicFieldArray(ArrayFieldType field)
        : field(field)
    {
    }

    ArrayFieldType field;
};

template<class... Fields>
class PacketParser
{
private:
    using Data = const unsigned char*;
    const static size_t _fieldCount = sizeof...(Fields);
    tuple<Fields...> _fields;
    Data _data;
    size_t _length;
    size_t _offset;

public:
    template<class... Fields>
    PacketParser(Fields... fields)
        : _fields(fields...)
        , _data(nullptr)
        , _length(0)
        , _offset(0)
    {
    }

    template <class OutputType>
    PacketParserErrorId parse(Data data, size_t length, OutputType& output)
    {
        // Reset working values
        _offset = 0;
        _data = data;
        _length = length;
        return processAllFields(output, make_index_sequence<_fieldCount>());
    }

    template <class OutputType, size_t... I>
    PacketParserErrorId processAllFields(OutputType& output, index_sequence<I...>)
    {
        // Process all fields
        PacketParserErrorId error = PacketParserErrorId::NoError;
        (processField(output, get<I>(_fields), error), ...);
        return error;
    }

    template <class OutputType, class FieldType>
    void processField(OutputType& output, FieldType& field, PacketParserErrorId& error)
    {
        // Keep processing fields as long as they are valid
        if (error != PacketParserErrorId::NoError)
            return;

        processBinary(output, field, error);
    }

    template <class OutputType, class FieldType>
    void processBinary(OutputType& output, FieldType& field, PacketParserErrorId& error)
    {
        using ValueType = FieldType:: template ValueType;

        // ValueField parsing ==================================================

        if constexpr (FieldType::typeId == FieldTypeId::ValueField)
        {
            if(FieldType::invertEndianness)
                (output.*(field.setter))(EndiannessInverter<ValueType>::call(*(reinterpret_cast<const ValueType*>(&_data[_offset]))));
            else
                (output.*(field.setter))(*(reinterpret_cast<const ValueType*>(&_data[_offset])));

            _offset += field.length;
            if (_offset > _length)
                error = PacketParserErrorId::ExceededDataRange;

            return;
        }

        // TextField parsing ===================================================

        else if constexpr (FieldType::typeId == FieldTypeId::TextField)
        {
            size_t nullTerminatorDistance = 0;
            if (!rangeContainsNullTerminator(_offset, _offset + field.length, nullTerminatorDistance, error))
            {
                // Avoid potentially overwriting an error set by the rangeContainsNullTerminator call
                error = error == PacketParserErrorId::NoError
                    ? PacketParserErrorId::MissingNullTerminator
                    : error;
                return;
            }

            if (!field.allowEmpty && nullTerminatorDistance == 1)
            {
                error = PacketParserErrorId::EmptyTextNotAllowed;
                return;
            }

            (output.*(field.setter))((const ValueType)(&_data[_offset]));

            // Update field length to increment _offset correctly
            _offset += nullTerminatorDistance;

            return;
        }

        // CompoundedField parsing =============================================

        else if constexpr (FieldType::typeId == FieldTypeId::CompoundedField)
        {
            ValueType intermediaryOutput;
            PacketParserErrorId intermediaryError = processCompoundedField(intermediaryOutput, field, make_index_sequence<field.fieldCount>());

            if (intermediaryError != PacketParserErrorId::NoError)
            {
                error = intermediaryError;
                return;
            }

            (output.*(field.setter))(intermediaryOutput);
            return;
        }

        // DynamicFieldArray ===================================================

        else if constexpr (FieldType::typeId == FieldTypeId::DynamicFieldArray)
        {
            // Decode array size
            using SizeType = FieldType:: template ArraySizeType;
            SizeType arraySize = (*(reinterpret_cast<const SizeType*>(&_data[_offset])));

            _offset += sizeof(SizeType);
            if (_offset > _length)
            {
                error = PacketParserErrorId::ExceededDataRange;
                return;
            }

            // Process whole array
            for (size_t i = 0; i < arraySize; ++i)
                processField(output, field.field, error);

            return;
        }

        error = PacketParserErrorId::UnhandledFieldType;
    }

    template <class IntermediaryOutputType, class CompoundedFieldType, size_t... I>
    PacketParserErrorId processCompoundedField(IntermediaryOutputType& intermediaryOutput, CompoundedFieldType& compoundedField, index_sequence<I...>)
    {
        PacketParserErrorId error = PacketParserErrorId::NoError;
        (processField(intermediaryOutput, get<I>(compoundedField.fields), error), ...);
        return error;
    }

    bool rangeContainsNullTerminator(size_t beginOffset, size_t endOffset, size_t& nullTerminatorDistance, PacketParserErrorId& error)
    {
        nullTerminatorDistance = 0;
        for (size_t i = beginOffset; i < endOffset; ++i)
        {
            if (i > _length)
            {
                error = PacketParserErrorId::ExceededDataRange;
                return false;
            }

            nullTerminatorDistance++;
            if (_data[i] == 0)
                return true;
        }
        return false;
    }
};

} // namespace GenericPacketParser
