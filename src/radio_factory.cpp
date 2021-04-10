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
#include <radio_tool/radio/radio_factory.hpp>
#include <libusb-1.0/libusb.h>

#include <exception>
#include <functional>
#include <codecvt>
#include <cstring>

using namespace radio_tool::radio;

/**
 * A list of functions to test each radio handler
 */
const std::vector<std::pair<std::function<bool(const libusb_device_descriptor &)>, std::function<std::unique_ptr<RadioSupport>(RadioFactory *, libusb_device_handle *)>>> RadioSupports = {
    {TYTRadio::SupportsDevice, TYTRadio::Create},
    {TYTSGLRadio::SupportsDevice, TYTSGLRadio::Create}};

auto RadioFactory::GetRadioSupport(const uint16_t &dev_idx) -> std::unique_ptr<RadioSupport>
{
    libusb_device **devs;
    auto ndev = libusb_get_device_list(usb_ctx, &devs);
    int err = LIBUSB_SUCCESS;
    auto n_idx = 0;

    if (ndev >= 0)
    {
        for (auto x = 0; x < ndev; x++)
        {
            struct libusb_device_descriptor desc;
            libusb_config_descriptor *conf_desc = NULL;
            if (LIBUSB_SUCCESS == (err = libusb_get_device_descriptor(devs[x], &desc)))
            {
                for (const auto &fnSupport : RadioSupports)
                {
                    if (fnSupport.first(desc))
                    {
                        if (n_idx == dev_idx)
                        {
                            libusb_device_handle *h;
                            if (LIBUSB_SUCCESS == (err = libusb_open(devs[x], &h)))
                            {
                                //setup for HID device
                                if (libusb_get_active_config_descriptor(devs[x], &conf_desc) == LIBUSB_SUCCESS)
                                {
                                    for (auto iface = 0; iface < conf_desc->bNumInterfaces; iface++)
                                    {
                                        auto intf = &conf_desc->interface[iface];
                                        for (auto alt = 0; alt < intf->num_altsetting; alt++)
                                        {
                                            auto intf_desc = &intf->altsetting[alt];
                                            if (intf_desc->bInterfaceClass == LIBUSB_CLASS_HID)
                                            {
                                                if (libusb_kernel_driver_active(h, intf_desc->bInterfaceNumber) == 1)
                                                {
                                                    if (libusb_detach_kernel_driver(h, intf_desc->bInterfaceNumber) != LIBUSB_SUCCESS)
                                                    {
                                                        throw std::runtime_error("Failed to open HID device!");
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                libusb_free_device_list(devs, 1);
                                return fnSupport.second(this, h);
                            }
                            else
                            {
                                libusb_free_device_list(devs, 1);
                                throw std::runtime_error("Failed to open device");
                            }
                        }
                        n_idx++;
                        break;
                    }
                }
            }
        }

        libusb_free_device_list(devs, 1);
    }
    else
    {
        throw std::runtime_error(libusb_error_name(ndev));
    }
    throw std::runtime_error("Radio not supported");
}

auto RadioFactory::OpDeviceList(std::function<void(const libusb_device *, const libusb_device_descriptor &, const uint16_t &)> op) const -> void
{
    libusb_device **devs;
    auto ndev = libusb_get_device_list(usb_ctx, &devs);
    int err = LIBUSB_SUCCESS;
    auto n_idx = 0;

    if (ndev >= 0)
    {
        for (auto x = 0; x < ndev; x++)
        {
            libusb_device_descriptor desc;
            if (LIBUSB_SUCCESS == (err = libusb_get_device_descriptor(devs[x], &desc)))
            {
                for (const auto &fnSupport : RadioSupports)
                {
                    if (fnSupport.first(desc))
                    {
                        op(devs[x], desc, n_idx);
                        n_idx++;
                        break;
                    }
                }
            }
        }

        libusb_free_device_list(devs, 1);
    }
    else
    {
        throw std::runtime_error(libusb_error_name(ndev));
    }
}

auto RadioFactory::ListDevices() const -> const std::vector<RadioInfo>
{
    std::vector<RadioInfo> ret;

    OpDeviceList([&ret, this](const libusb_device *dev, const libusb_device_descriptor &desc, const uint16_t &idx) {
        int err = LIBUSB_SUCCESS;
        libusb_device_handle *h;
        if (LIBUSB_SUCCESS == (err = libusb_open(const_cast<libusb_device *>(dev), &h)))
        {
            auto mfg = GetDeviceString(desc.iManufacturer, h),
                 prd = GetDeviceString(desc.iProduct, h);

            auto nInf = RadioInfo(mfg, prd, desc.idVendor, desc.idProduct, idx);
            ret.push_back(nInf);
            libusb_close(h);
        }
    });

    return ret;
}

auto RadioFactory::GetDeviceString(const uint8_t &desc, libusb_device_handle *h) const -> std::wstring
{
    auto err = 0;
    size_t prd_len = 0;
    unsigned char lang[42], prd[255];
    memset(prd, 0, 255);

    err = libusb_get_string_descriptor(h, 0, 0, lang, 42);
    if (0 > (prd_len = libusb_get_string_descriptor(h, desc, lang[3] << 8 | lang[2], prd, 255)))
    {
        throw std::runtime_error(libusb_error_name(err));
    }

    if (prd_len >= 255)
    {
        return std::wstring(L"UNKNOWN");
    }
    //Encoded as UTF-16 (LE), Prefixed with length and some other byte.
    typedef std::codecvt_utf16<char16_t, 1114111UL, std::little_endian> cvt;
    auto u16 = std::wstring_convert<cvt, char16_t>().from_bytes((const char *)prd + 2, (const char *)prd + prd_len);
    return std::wstring(u16.begin(), u16.end());
}

auto RadioFactory::HandleEvents() const -> void
{
    std::cerr << "Events tread started: #" << std::this_thread::get_id() << std::endl;
    while (!shutdown)
    {
        auto err = libusb_handle_events(usb_ctx);
        if (err != LIBUSB_SUCCESS)
        {
            if (err != LIBUSB_ERROR_BUSY &&
                err != LIBUSB_ERROR_TIMEOUT &&
                err != LIBUSB_ERROR_OVERFLOW &&
                err != LIBUSB_ERROR_INTERRUPTED)
            {
                break;
            }
        }
    }
}