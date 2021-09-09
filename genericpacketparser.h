#pragma once

#include <iostream>
#include <tuple>
#include <type_traits>
#include <cassert>

namespace GenericPacketParser
{

// =============================================================================
// Errors
// =============================================================================

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

std::ostream& operator<<(std::ostream& out, PacketParserErrorId error)
{
    switch (error)
    {
#define ERROR_TO_STREAM(value) case PacketParserErrorId::##value: out << #value; break;
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

// =============================================================================
// Endianness inversion
// =============================================================================

/**
* Template class used to reverse the endianness of a value.
*
* @note Implemented for primitives of size 2, 4 and 8. Other sizes will generate compilation errors
*/
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

// =============================================================================
// Fields types
// =============================================================================

enum class FieldTypeId
{
    TextField,
    ValueField,
    MultiField,
    DynamicFieldArray,
    StaticFieldArray,
    BinaryField
};

// =============================================================================
// ValueField
// =============================================================================

/**
* Struct use to configure a simple value field
*
* @tparam T Type of the value
* @tparam SetterSignature Type of the setter that will be called to store the value
* @tparam InvertEndianness Boolean value indicating if the endianness of the value should be inverted
*/
template <class T, class SetterSignature, bool InvertEndianness = false>
struct ValueField
{
    using ValueType = T;
    using SetterType = SetterSignature;
    static constexpr FieldTypeId typeId = FieldTypeId::ValueField;
    static constexpr bool invertEndianness = InvertEndianness;
    static const size_t length = sizeof(ValueType);

    /**
    * @param setter Setter used to store the parsed value
    * @see GenericPackerParser::makeValueField
    * @see GenericPackerParser::makeValueFieldEndian
    */
    ValueField(SetterSignature setter)
        : setter(setter)
    {
    }

    const SetterSignature setter;
};

// =============================================================================
// TextField
// =============================================================================

/**
* Struct used to configure a text field
*
* @tparam SetterSignature Type of the setter that will be called to store the parsed text
* @tparam AllowEmpty Boolean value indicating if the text field is allowed to be empty
*/
template <class SetterSignature, bool AllowEmpty = false>
struct TextField
{
    using SetterType = SetterSignature;
    using ValueType = char*;
    static constexpr bool allowEmpty = AllowEmpty;
    static constexpr FieldTypeId typeId = FieldTypeId::TextField;

    /**
    * @param setter Setter used to store the parsed text
    * @param maxLength Maximum expected length of the text field (including \0)
    * @see GenericPackerParser::makeTextField
    * @see GenericPackerParser::makeTextFieldAllowEmpty
    */
    TextField(SetterSignature setter, size_t maxLength)
        : setter(setter)
        , length(maxLength)
    {
        assert(("Text length must be greater than 0.", length > 0));
    }

    const SetterSignature setter;
    const size_t length;
};

// =============================================================================
// BinaryField
// =============================================================================

/**
* Struct used to configure a binary field
*
* @tparam PayloadSizeValueType Type of the value holding the length of the binary data
* @tparam SetterSignature Type of the setter that will be called to store the parsed value
*/
template <class PayloadSizeValueType, class SetterSignature>
struct BinaryField
{
    using ValueType = const unsigned char*;
    using SetterType = SetterSignature;
    using PayloadSizeType = PayloadSizeValueType;
    static constexpr FieldTypeId typeId = FieldTypeId::BinaryField;

    /**
    * @param setter Setter used to store the parsed binary data
    * @see GenericPackerParser::makeBinaryField
    */
    BinaryField(SetterSignature setter)
        : setter(setter)
    {
    }

    SetterSignature setter;
};

// =============================================================================
// MultiField
// =============================================================================

/**
* Struct used to configure a field containing multiple subfields
*
* @tparam OutputType Type of the intermediary object receiving the parsed subfield values
* @tparam SetterSignature Type of the setter that will be called to store the parsed value in the intermediary
* @tparam Fields Subfield types
*/
template <class OutputType, class SetterSignature, class... Fields>
struct MultiField
{
    using ValueType = OutputType;
    using SetterType = SetterSignature;
    static constexpr size_t fieldCount = sizeof...(Fields);
    static constexpr FieldTypeId typeId = FieldTypeId::MultiField;

    /**
    * @param setter Setter used to store the parsed subfields
    * @param fields Fields to parse
    * @see GenericPackerParser::makeBinaryField
    */
    MultiField(SetterSignature setter, Fields... fields)
        : setter(setter)
        , fields(fields...)
    {
    }

    SetterSignature setter;
    std::tuple<Fields...> fields;
};

// =============================================================================
// DynamicFieldArray
// =============================================================================

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

// =============================================================================
// StaticFieldArray
// =============================================================================

template <class T>
struct StaticFieldArray
{
    using ArrayFieldType = T;
    using ValueType = ArrayFieldType;
    static constexpr FieldTypeId typeId = FieldTypeId::StaticFieldArray;

    StaticFieldArray(ArrayFieldType field, size_t length)
        : field(field)
        , length(length)
    {
        assert(("Static array length must be greater than 0.", length > 0));
    }

    ArrayFieldType field;
    const size_t length;
};

// =============================================================================
// PacketParser
// =============================================================================

template<class... Fields>
class PacketParser
{
    using Data = const unsigned char*;

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
        return processAllFields(output, std::make_index_sequence<_fieldCount>());
    }

private:
    const static size_t _fieldCount = sizeof...(Fields);
    std::tuple<Fields...> _fields;
    Data _data;
    size_t _length;
    size_t _offset;

    template <class OutputType, size_t... I>
    PacketParserErrorId processAllFields(OutputType& output, std::index_sequence<I...>)
    {
        // Process all fields
        PacketParserErrorId error = PacketParserErrorId::NoError;
        (processField(output, std::get<I>(_fields), error), ...);
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

        // ValueField parsing
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

        // TextField parsing
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

        // Binary parsing
        else if constexpr (FieldType::typeId == FieldTypeId::BinaryField)
        {
            assert(("Binary parsing not implemented yet!", false));
        }

        // MultiField parsing
        else if constexpr (FieldType::typeId == FieldTypeId::MultiField)
        {
            ValueType intermediaryOutput;
            PacketParserErrorId intermediaryError = processMultiField(intermediaryOutput, field, std::make_index_sequence<field.fieldCount>());

            if (intermediaryError != PacketParserErrorId::NoError)
            {
                error = intermediaryError;
                return;
            }

            (output.*(field.setter))(intermediaryOutput);
            return;
        }

        // DynamicFieldArray parsing
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

        // StaticFieldArray parsing
        else if constexpr (FieldType::typeId == FieldTypeId::StaticFieldArray)
        {
            assert(("StaticFieldArray parsing not implemented yet!", false));
        }

        error = PacketParserErrorId::UnhandledFieldType;
    }

    template <class IntermediaryOutputType, class MultiFieldType, size_t... I>
    PacketParserErrorId processMultiField(IntermediaryOutputType& intermediaryOutput, MultiFieldType& MultiField, std::index_sequence<I...>)
    {
        PacketParserErrorId error = PacketParserErrorId::NoError;
        (processField(intermediaryOutput, std::get<I>(MultiField.fields), error), ...);
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


// =============================================================================
// Utilities
// =============================================================================

template<class T, class SetterSignature>
ValueField<T, SetterSignature> makeValueField(SetterSignature setter)
{
    return setter;
}

#define VALUE_FIELD(setter, type) makeValueField<type>(setter)

template<class T, class SetterSignature>
ValueField<T, SetterSignature, true> makeValueFieldEndian(SetterSignature setter)
{
    return setter;
}

#define VALUE_FIELD_ENDIAN(setter, type) makeValueFieldEndian<type>(setter)

template<class SetterSignature>
TextField<SetterSignature> makeTextField(SetterSignature setter, size_t maxLength)
{
    return {setter, maxLength};
}

#define TEXT_FIELD(setter, maxLength) makeTextField(setter, maxLength)

template<class SetterSignature>
TextField<SetterSignature, true> makeTextFieldAllowEmpty(SetterSignature setter, size_t maxLength)
{
    return {setter, maxLength};
}

#define TEXT_FIELD_ALLOW_EMPTY(setter, maxLength) makeTextFieldAllowEmpty(setter, maxLength)

template <class SizeType, class SetterSignature>
BinaryField<SizeType, SetterSignature> makeBinaryField(SetterSignature setter)
{
    return setter;
}

#define BINARY_FIELD(sizeType, setter) makeBinaryField<sizeType>(setter)

template <class OutputType, class SetterSignature, class... Fields>
MultiField<OutputType, SetterSignature, Fields...> makeMultiField(SetterSignature setter, Fields... fields)
{
    return {setter, fields...};
}

#define MULTI_FIELD(outputType, setter, ...) makeMultiField<outputType>(setter, ##__VA_ARGS__)

template <class SizeType, class FieldType>
DynamicFieldArray<FieldType, SizeType> makeDynamicFieldArray(FieldType field)
{
    return field;
}

#define DYNAMIC_ARRAY(sizeType, field) makeDynamicFieldArray<sizeType>(field)

template <class FieldType>
StaticFieldArray<FieldType> makeStaticFieldArray(size_t length, FieldType field)
{
    return {field, length};
}

#define STATIC_ARRAY(size, field) makeStaticFieldArray(size, field)

template <class... Fields>
PacketParser<Fields...> makePacketParser(Fields... fields)
{
    return {fields...};
}

} // namespace GenericPacketParser
