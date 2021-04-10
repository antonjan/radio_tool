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
#pragma once

#include <radio_tool/radio/radio.hpp>
#include <radio_tool/radio/tyt_radio.hpp>
#include <radio_tool/radio/tyt_sgl_radio.hpp>
#include <libusb-1.0/libusb.h>

#include <iostream>
#include <memory>
#include <functional>
#include <thread>

namespace radio_tool::radio
{
    class RadioFactory
    {
    public:
        RadioFactory() : usb_ctx(nullptr), shutdown(false)
        {
            auto err = libusb_init(&usb_ctx);
            if (err != LIBUSB_SUCCESS)
            {
                throw std::runtime_error(libusb_error_name(err));
            }
#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000107)
            libusb_set_log_cb(
                usb_ctx, [](libusb_context *ctx, enum libusb_log_level level, const char *str) {
                    std::wcout << str << std::endl;
                },
                LIBUSB_LOG_CB_CONTEXT);
#endif
        }
        
        ~RadioFactory()
        {
            libusb_exit(usb_ctx);
            usb_ctx = nullptr;
        }

        /**
         * Return the radio support handler for a specified usb device
         */
        auto GetRadioSupport(const uint16_t &idx) -> std::unique_ptr<RadioSupport>;

        /**
         * Gets info about currently supported devices
         */
        auto ListDevices() const -> const std::vector<RadioInfo>;

        /**
         * Get a string from a USB descriptor
         */
        auto GetDeviceString(const uint8_t &, libusb_device_handle *) const -> std::wstring;

        /**
         * Lists radio models supporting ALL operations
         */
        static auto ListRadioSupport() -> std::vector<std::string>
        {
            std::vector<std::string> ret;

            //TODO: redo

            return ret;
        }

        auto SetupAsyncIO(libusb_device_handle *handle) -> void 
        {
            events = std::thread(&RadioFactory::HandleEvents, this);
        }
    private:
        auto OpDeviceList(std::function<void(const libusb_device *, const libusb_device_descriptor &, const uint16_t &)>) const -> void;

        libusb_context *usb_ctx;
        std::thread events;
        bool shutdown;
        
        auto HandleEvents() const -> void;
    };
} // namespace radio_tool::radio