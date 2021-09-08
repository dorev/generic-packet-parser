/*
#include <gtest/gtest.h>

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions) {
  // Expect two strings not to be equal.
  EXPECT_STRNE("hello", "world");
  // Expect equality.
  EXPECT_EQ(7 * 6, 42);
}
*/

#include "genericpacketparser.h"
#include <string>
#include <vector>

using namespace std;
using namespace GenericPacketParser;

struct SubPacket
{
    string name;
    uint32_t value;
    void setName(string s) { name = s; }
    void setValue(uint32_t v) { value = v; }
};

struct MyPacket
{
    string name;
    uint32_t value;
    vector<SubPacket> array;

    void setName(string s) { name = s; }
    void setValue(uint32_t v) { value = v; }
    void addToArray(SubPacket& sp) { array.emplace_back(sp); }
};

int main()
{
    // Data
    const unsigned char data[] =
    {
        'A', 'l', 'e', 'x', 'a', 'n', 'd', 'r', 'e', ' ', 'D', 'u', 'm', 'a', 's', 0,
        0x01, 0x01, 0x00, 0x00,
        0x04,
            //'D', '\'', 'A', 'r', 't', 'a', 'g', 'a', 'n', 0,
            0,
            0x00, 0x00, 0x00, 0x01, // <-- gotta reverse endianness!
            'A', 'r', 'a', 'm', 'i', 's', 0,
            0x00, 0x00, 0x00, 0x02,
            'A', 't', 'h', 'o', 's', 0,
            0x00, 0x00, 0x00, 0x03,
            'P', 'o', 'r', 't', 'h', 'o', 's', 0,
            0x00, 0x00, 0x00, 0x04,
    };
    size_t l1 = 10;
    size_t l2 = 59;

    // Declare fields and setters
    TEXT_FIELD(nameField, MyPacket::setName, 16);
    VALUE_FIELD(valueField, MyPacket::setValue, uint32_t);
    TEXT_FIELD_ALLOW_EMPTY(aliasField, SubPacket::setName, 16);
    VALUE_FIELD(aliasFlags, SubPacket::setValue, uint32_t);
    MULTI_FIELD2(aliasArrayElement, SubPacket, MyPacket::addToArray, aliasField, aliasFlags);
    FIELD_ARRAY(aliasArray, aliasArrayElement, uint8_t);

    // Packet parser type, order is important!
    PACKET_PARSER3(packetParser, nameField, valueField, aliasArray);

    MyPacket out{"", 0};
    PacketParserErrorId result = packetParser.parse(data, l2, out);
    
    // Output MyPacket
    cout << "Parsing result: " << result << '\n';

    cout << "Name: " << out.name << '\n'
        << "Value: " << out.value << '\n';

    cout << "Array content:\n";
    size_t i = 0;
    for (auto& element : out.array)
    {
        cout << "  " << i++ << ":\n";
        cout << "  Name: " << element.name << '\n';
        cout << "  Value: " << element.value << '\n';
    }
}
