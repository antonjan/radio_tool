/**
 * This file is part of radio_tool.
 * Copyright (c) 2021 Kieran Harkin <kieran+git@harkin.me>
 * 
 * radio_tool is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * radio_tool is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with radio_tool. If not, see <https://www.gnu.org/licenses/>.
 */
#include <radio_tool/fw/tyt_fw_sgl.hpp>
#include <radio_tool/util.hpp>

#include <iterator>

using namespace radio_tool::fw;

auto TYTSGLFW::Read(const std::string &file) -> void
{
    auto hdr = ReadHeader(file);

    auto i = std::ifstream(file, std::ios_base::binary);
    if (i.is_open())
    {
        i.seekg(-hdr.length, i.end);

        constexpr auto rbuf_len = 1024 * 16;
        unsigned char rbuf[rbuf_len];
        while (!i.eof())
        {
            i.read((char *)rbuf, rbuf_len);
            auto rlen = i.gcount();
            data.insert(data.end(), rbuf, rbuf + rlen);
        }
        memory_ranges.push_back(std::pair<uint32_t, uint32_t>(0, data.size()));
    }
    i.close();
}

auto TYTSGLFW::Write(const std::string &file) -> void
{
    std::ofstream fout(file, std::ios_base::binary);
    if (fout.is_open())
    {
        auto header = config->header.Serialize();
        fout.write((char *)header.data(), header.size());
        fout.write((char *)data.data(), data.size());
    }
    fout.close();
}

auto TYTSGLFW::ToString() const -> std::string
{
    std::stringstream out;
    out << "== TYT SGL Firmware == " << std::endl
        << "Radio: " << config->radio_model << std::endl
        << "Size:  " << FormatBytes(data.size()) << std::endl
        << "Data Segments: " << std::endl;
    auto n = 0;
    for (const auto &m : memory_ranges)
    {
        out << "  " << n++ << ": Length=0x" << std::setfill('0') << std::setw(8) << std::hex << m.second
            << std::endl;
    }
    return out.str();
}

auto TYTSGLFW::ReadHeader(const std::string &file) -> const SGLHeader
{
    std::ifstream i;
    i.open(file, i.binary);
    if (i.is_open())
    {
        constexpr auto HeaderLength = 0x100;

        uint8_t test_magic[4];
        i.read((char *)test_magic, 4);

        if (!std::equal(test_magic, test_magic + 4, tyt::config::sgl::Magic.begin()))
        {
            throw std::runtime_error("Invalid header magic");
        }

        //ENCV001
        uint8_t ver[8];
        i.read((char *)ver, 7);
        for (auto x = 0; x < 7; x++)
        {
            ver[x] ^= test_magic[x % 4];
        }
        ver[7] = 0x00;

        auto sgl_version = std::stoi((char *)ver + 4);

        i.seekg(0x0B, i.beg);

        uint8_t header_table[4];
        i.read((char *)header_table, 4);
        for (auto x = 0; x < 4; x++)
        {
            header_table[x] ^= test_magic[x % 2];
        }

        uint8_t header[HeaderLength];
        auto offset = header_table[0] + 256u * header_table[1];

        i.seekg(offset, i.beg);
        i.read((char *)header, HeaderLength);
        for (auto x = 0; x < HeaderLength; x++)
        {
            header[x] ^= header_table[2 + (x % 2)];
        }

        constexpr auto GroupOffset = 0x32;
        constexpr auto ModelOffset = 0x42;
        constexpr auto VersionOffset = 0x4a;
        constexpr auto KeyOffset = 0x5f;

        auto len = *(uint32_t *)(header + 0x06);
        auto group = std::vector<uint8_t>(header + GroupOffset, header + GroupOffset + 0x10);
        auto model = std::vector<uint8_t>(header + ModelOffset, header + ModelOffset + 0x08);
        auto version = std::vector<uint8_t>(header + VersionOffset, header + VersionOffset + 0x08);
        auto key = std::vector<uint8_t>(header + KeyOffset, header + KeyOffset + 0x08);

        return SGLHeader(sgl_version, len, group, model, version, key);
    }
    else
    {
        throw std::runtime_error("Can't open firmware file");
    }
}

auto TYTSGLFW::SupportsFirmwareFile(const std::string &file) -> bool
{
    auto header = ReadHeader(file);
    if (header.length != 0)
    {
        return true;
    }
    return false;
}

auto TYTSGLFW::SupportsRadioModel(const std::string &model) -> bool
{
    for (const auto &mx : tyt::config::sgl::All)
    {
        if (mx.radio_model == model)
        {
            return true;
        }
    }
    return false;
}

auto TYTSGLFW::GetRadioModel() const -> const std::string
{
    return config->radio_model;
}

auto TYTSGLFW::SetRadioModel(const std::string &model) -> void
{
    for (const auto &rg : tyt::config::sgl::All)
    {
        if (rg.radio_model == model)
        {
            config = &rg;
            break;
        }
    }
}

auto TYTSGLFW::Decrypt() -> void
{
    auto cx = 0;
    for (auto &dx : data)
    {
        dx = ~(((dx << 3) & 0b11111000) | ((dx >> 5) & 0b00000111));
        dx = dx ^ config->cipher[(config->xor_offset + cx++) % config->cipher_len];
    }
}

auto TYTSGLFW::Encrypt() -> void
{
    // before encrypting make sure the data is the correct length
    // if too short add padding
    // if too big? ...official firmware writing tool wont work probably
    // padding is normally handled by FirmwareSupport::AppendSegment
    if (data.size() < config->header.length)
    {
        std::fill_n(std::back_inserter(data), config->header.length - data.size(), 0xff);
    }

    auto cx = 0;
    for (auto &dx : data)
    {
        dx = dx ^ config->cipher[(config->xor_offset + cx++) % config->cipher_len];
        dx = ~(((dx >> 3) & 0b00011111) | ((dx << 5) & 0b11100000));
    }
}

auto SGLHeader::Serialize(bool encrypt) const -> std::vector<uint8_t>
{
    // after header1 up to offset
    auto junk1_size = 10;

    std::vector<uint8_t> junk2 = {
        0x02, 0x10, 0x00, 0x00, 0x00, 0x00,
        (uint8_t)(length & 0xff),
        (uint8_t)((length & 0xff00) >> 8),
        (uint8_t)((length & 0xff0000) >> 16),
        (uint8_t)((length & 0xff000000) >> 24)};
    junk2.resize(junk2.size() + 0x28);

    auto junk3_size = 10; // after strings before binary

    std::vector<uint8_t> ret(0x10 + junk1_size + junk2.size() + 0x28 + junk3_size);

    auto next = std::copy(tyt::config::sgl::Magic.begin(), tyt::config::sgl::Magic.end(), ret.begin());

    std::stringstream ver;
    ver << "ENCV" << std::setw(3) << std::setfill('0') << sgl_version;
    auto ver_str = ver.str();

    auto offset = 0x10 + junk1_size;
    std::vector<uint8_t> dummy_xor = {0xB0, 0x0B}; //s

    next = std::copy(ver_str.begin(), ver_str.end(), next);
    *next++ = 0xff; //Model ?
    *next++ = (int)offset % 255;
    *next++ = (int)offset / 255;
    *next++ = dummy_xor[0]; //xor[0]?
    *next++ = dummy_xor[1]; //xor[1]?

    std::advance(next, junk1_size);

    next = std::copy(junk2.begin(), junk2.end(), next);

    next = std::copy(radio_group.begin(), radio_group.end(), next);
    next = std::fill_n(next, 0x10 - radio_group.size(), 0xff);

    next = std::copy(radio_model.begin(), radio_model.end(), next);
    next = std::fill_n(next, 0x08 - radio_model.size(), 0xff);

    next = std::copy(protocol_version.begin(), protocol_version.end(), next);
    next = std::fill_n(next, 0x08 - protocol_version.size(), 0xff);

    next = std::copy(model_key.begin(), model_key.end(), next);
    next = std::fill_n(next, 0x08 - model_key.size(), 0xff);

    //binary appended here
    return ret;
}