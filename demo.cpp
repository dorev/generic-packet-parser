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
            //'D', '\'', 'A', 'r', 't', 'a', 'g', 'a', 'n', 0,  // <-- test 0-length string
            0,                                                  // <--'
            0x00, 0x00, 0x00, 0x01, // <-- gotta reverse endianness!
            'A', 'r', 'a', 'm', 'i', 's', 0,
            0x00, 0x00, 0x00, 0x02,
            'A', 't', 'h', 'o', 's', 0,
            0x00, 0x00, 0x00, 0x03,
            'P', 'o', 'r', 't', 'h', 'o', 's', 0,
            0x00, 0x00, 0x00, 0x04,
    };

    size_t length = 59;

    auto parser = makePacketParser(
        TEXT_FIELD(&MyPacket::setName, 16),
        VALUE_FIELD(&MyPacket::setValue, uint32_t),
        DYNAMIC_ARRAY(uint8_t,
            MULTI_FIELD(SubPacket, &MyPacket::addToArray,
                TEXT_FIELD_ALLOW_EMPTY(&SubPacket::setName, 16),
                VALUE_FIELD_ENDIAN(&SubPacket::setValue, uint32_t)
            )
        )
    );

    MyPacket output{"", 0};
    PacketParserErrorId result = parser.parse(data, length, output);

    // Output MyPacket
    cout << "Parsing result: " << result << '\n'
         << "Name: " << output.name << '\n'
         << "Value: " << output.value << '\n'
         << "Array content:\n";

    size_t i = 0;
    for (auto& element : output.array)
    {
        cout << "  " << i++ << ":\n"
             << "  Name: " << element.name << '\n'
             << "  Value: " << element.value << '\n';
    }
}
