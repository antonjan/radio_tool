#include <iostream>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <vector>

#define HEX(x) std::setw(x) << std::setfill('0') << std::hex

static auto PrintHex(std::vector<uint8_t>::const_iterator &&begin, std::vector<uint8_t>::const_iterator &&end) -> void
{
    auto c = 1u;

    constexpr auto asciiZero = 0x30;
    constexpr auto asciiA = 0x61 - 0x0a;

    constexpr auto wordSize = 4;
    constexpr auto wordCount = 4;

    char eol_ascii[wordSize * wordCount + 1] = {};
    char aV, bV;
    std::stringstream prnt;
    auto size = std::distance(begin, end);
    while (begin != end)
    {
        auto v = (*begin);
        auto a = v & 0x0f;
        auto b = v >> 4;
        aV = (a <= 9 ? asciiZero : asciiA) + a;
        bV = (b <= 9 ? asciiZero : asciiA) + b;
        prnt << bV << aV << " ";

        auto col = c % (wordSize * wordCount);
        eol_ascii[col] = v >= 32 && v <= 127 ? (char)v : '.';
        if (col == 0 && c != size)
        {
            //prnt << " " << eol_ascii << std::endl;
            prnt << std::endl;
        }
        else if (c % wordSize == 0)
        {
            prnt << "  ";
        }
        if (c == size)
        {
            if (col > 0)
            {
                eol_ascii[col + 1] = 0;
            }
            //prnt << (col != 0 ? " " : "") << eol_ascii;
        }
        c++;
        std::advance(begin, 1);
    }

    std::cerr << prnt.str() << std::endl;
}

int main(int argc, char **argv)
{
    auto magic = "SGL!";

    std::ifstream in(argv[1], std::ifstream::binary);
    if (in.is_open())
    {
        std::cout << argv[1] << std::endl;
        in.seekg(4, std::ifstream::beg);

        auto h1_len = 12;
        char h1[h1_len];
        in.read(h1, h1_len);

        for (auto x0 = 0; x0 < h1_len; x0++)
        {
            h1[x0] ^= magic[x0 % 4];
        }

        std::cout << "Header 1:" << std::endl;
        auto v0 = std::vector<uint8_t>(h1, h1 + h1_len);
        PrintHex(v0.begin(), v0.end());

        auto h2_len = ((uint8_t)h1[8] + 256u * (uint8_t)h1[9]) - 16;
        char h2[h2_len];
        in.read(h2, h2_len);

        for (auto x0 = 0; x0 < h2_len; x0++)
        {
            h2[x0] ^= h1[10 + (x0 % 2)];
            //h2[x0] ^= magic[x0 % 4];
        }
        std::cout << "Header 2 (" << h2_len << "):" << std::endl;
        auto v1 = std::vector<uint8_t>(h2, h2 + h2_len);
        PrintHex(v1.begin(), v1.end());

        auto h3_len = 512;
        char h3[h3_len];
        in.read(h3, h3_len);
        for (auto x0 = 0; x0 < h3_len; x0++)
        {
            h3[x0] ^= h1[10 + (x0 % 2)];
            //h2[x0] ^= magic[x0 % 4];
        }
        std::cout << "Header 3 (" << h3_len << "):" << std::endl;
        auto v2 = std::vector<uint8_t>(h3, h3 + h3_len);
        PrintHex(v2.begin(), v2.end());

        std::cout << std::endl;
        /*return;
        auto rh = 1024;
        char h512[rh];
        in.seekg(offset, std::ifstream::beg);
        in.read(h512, rh);

        for(auto x1 = 0; x1 < rh; x1++) 
        {
            h512[x1] = h512[x1] ^ header_xor[x1 % 2];
        }

        auto vp = std::vector<uint8_t>(h512, h512+rh);
        PrintHex(vp.begin(), vp.end());*/
    }
}