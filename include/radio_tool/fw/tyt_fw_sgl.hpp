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

#include <radio_tool/fw/fw.hpp>
#include <radio_tool/fw/cipher/sgl.hpp>
#include <radio_tool/fw/blob/sgl_headers.hpp>

#include <fstream>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace radio_tool::fw
{
    /**
     * Class to store all config for each TYT radio model (SGL firmware)
     */
    class TYTSGLRadioConfig
    {
    public:
        TYTSGLRadioConfig(const std::string &model, const uint8_t *header, const uint32_t &header_len, const uint8_t *cipher, const uint32_t &cipher_l, const uint16_t &xor_offset, const uint32_t &file_length)
            : radio_model(model), header(header), header_len(header_len), cipher(cipher), cipher_len(cipher_l), xor_offset(xor_offset), file_length(file_length)
        {
        }

        auto operator==(const TYTSGLRadioConfig &other) const -> bool {
            return radio_model == other.radio_model;
        }

         auto operator!=(const TYTSGLRadioConfig &other) const -> bool {
            return !(*this == other);
        }

        /**
         * The model of the radio
         */
        const std::string radio_model;

        /**
         * Static header blob
         */
        const uint8_t *header;

        /**
         * Length of the static header blob
         */
        const uint32_t header_len;

        /**
         * The cipher key for encrypting/decrypting the firmware
         */
        const uint8_t *cipher;

        /**
         * The length of the cipher
         */
        const uint32_t cipher_len;

        /**
         * Offset into xor key 
         */
        const uint16_t xor_offset;

        /**
         * Length of the firmware file
         */
        const uint32_t file_length;

        /**
         * And empty config instance for reference
         */
        static auto Empty() -> const TYTSGLRadioConfig {
            return TYTSGLRadioConfig();
        }
        private:
            TYTSGLRadioConfig() 
                : radio_model(), header(nullptr), header_len(0), cipher(nullptr), cipher_len(0), xor_offset(0), file_length(0) 
            {}
    };

    namespace tyt::config::sgl
    {
        const std::vector<TYTSGLRadioConfig> All = {
            TYTSGLRadioConfig("GD77", fw::blob::Header318, fw::blob::Header318_length, fw::cipher::sgl, fw::cipher::sgl_length, 0x807, 0x77001 /* The header, from firmware version 3.1.8 expects the file to be 0x77001 long */),
            TYTSGLRadioConfig("GD77S", fw::blob::Header120, fw::blob::Header120_length, fw::cipher::sgl, fw::cipher::sgl_length, 0x2a8e, 0x77001 /* The header, from firmware version 1.2.0 expects the file to be 0x50001 long, but it has been hacked to 0x77001 */),
            TYTSGLRadioConfig("DM1801", fw::blob::Header219, fw::blob::Header219_length, fw::cipher::sgl, fw::cipher::sgl_length, 0x2c7c, 0x78001 /* The header, from firmware version 2.1.9 expects the file to be 0x78001 long */),
            TYTSGLRadioConfig("RD5R", fw::blob::Header217, fw::blob::Header217_length, fw::cipher::sgl, fw::cipher::sgl_length, 0x306e, 0x78001 /* The header, from firmware version 2.1.7 expects the file to be 0x78001 long */),
        };
    }

    class TYTSGLFW : public FirmwareSupport
    {
    public:
        TYTSGLFW() : FirmwareSupport(), config(nullptr) {}

        auto Read(const std::string &file) -> void override;
        auto Write(const std::string &file) -> void override;
        auto ToString() const -> std::string override;
        auto Decrypt() -> void override;
        auto Encrypt() -> void override;
        auto SetRadioModel(const std::string &) -> void override;

        /**
         * @note This is not the "firmware_model" which exists in the firmware header
         */
        auto GetRadioModel() const -> const std::string override;

        /**
         * Tests a file if its a valid firmware file
         */
        static auto SupportsFirmwareFile(const std::string &file) -> bool;

        /**
         * Tests if a radio model is supported by this firmware handler
         */
        static auto SupportsRadioModel(const std::string &model) -> bool;

        /**
         * Test the file if it matches any known headers
         */
        static auto DetectFirmware(const std::string &file) -> const TYTSGLRadioConfig*;

        /**
         * Create an instance of this class for the firmware factory
         */
        static auto Create() -> std::unique_ptr<FirmwareSupport>
        {
            return std::make_unique<TYTSGLFW>();
        }

    private:
        const TYTSGLRadioConfig *config;

        auto ApplyXOR() -> void;
    };

} // namespace radio_tool::fw