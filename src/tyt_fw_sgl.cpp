/**
 * This file is part of radio_tool.
 * Copyright (c) 2020 Kieran Harkin <kieran+git@harkin.me>
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

using namespace radio_tool::fw;

auto TYTSGLFW::Read(const std::string &file) -> void
{
    auto cfg = DetectFirmware(file);
    if (cfg != nullptr)
    {
        config = cfg;
        auto i = std::ifstream(file, std::ios_base::binary);
        if (i.is_open())
        {
            //skip header its static
            i.seekg(cfg->header_len, i.beg);

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
    else
    {
        throw std::runtime_error("Not a valid SGL firmware file!");
    }
}

auto TYTSGLFW::Write(const std::string &file) -> void
{
    std::ofstream fout(file, std::ios_base::binary);
    if (fout.is_open())
    {
        fout.write((char*)config->header, config->header_len);
        fout.write((char*)data.data(), data.size());
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

auto TYTSGLFW::DetectFirmware(const std::string &file) -> const TYTSGLRadioConfig *
{
    std::ifstream i;
    i.open(file, i.binary);
    if (i.is_open())
    {
        //read 128bytes and test known headers
        unsigned char header[128];
        i.read((char *)header, 128);
        i.close();

        for (const auto &cfg : tyt::config::sgl::All)
        {
            if (std::equal(header, header + 128, cfg.header))
            {
                return &cfg;
            }
        }
    }
    else
    {
        throw std::runtime_error("Can't open firmware file");
    }
    return nullptr;
}

auto TYTSGLFW::SupportsFirmwareFile(const std::string &file) -> bool
{
    auto fw = DetectFirmware(file);
    if (fw != nullptr)
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
    ApplyXOR();
}

auto TYTSGLFW::Encrypt() -> void
{
    // before encrypting make sure the data is the correct length
    // if too short add padding
    // if too big? ...official firmware writing tool wont work probably
    // padding is normally handled by FirmwareSupport::AppendSegment
    if(data.size() < config->file_length) 
    {
        std::fill_n(std::back_inserter(data), config->file_length - data.size(), 0xff);
    }
    ApplyXOR();
}

auto TYTSGLFW::ApplyXOR() -> void
{
    radio_tool::ApplyXOR(data, config->cipher, config->cipher_len, config->xor_offset);
    for (auto &dx : data)
    {
        dx = ~(((dx >> 3) & 0x1F) | ((dx << 5) & 0xE0));
    }
}