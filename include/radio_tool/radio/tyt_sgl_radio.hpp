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
#pragma once

#include <radio_tool/radio/radio.hpp>
#include <radio_tool/hid/tyt_hid.hpp>

#include <functional>

namespace radio_tool::radio
{
    class RadioFactory;
    class TYTSGLRadio : public RadioSupport
    {
    public:
        TYTSGLRadio(RadioFactory *f, libusb_device_handle *h);

        auto WriteFirmware(const std::string &file) -> void override;
        auto ToString() -> const std::string override;

        static auto SupportsDevice(const libusb_device_descriptor &dev) -> bool
        {
            if (dev.idVendor == hid::TYTHID::VID && dev.idProduct == hid::TYTHID::PID)
            {
                return true;
            }
            return false;
        }

        static auto Create(RadioFactory *f, libusb_device_handle *h) -> std::unique_ptr<TYTSGLRadio>
        {
            return std::unique_ptr<TYTSGLRadio>(new TYTSGLRadio(f, h));
        }

    private:
        uint16_t dev_index;
        hid::TYTHID device;
    };
} // namespace radio_tool::radio