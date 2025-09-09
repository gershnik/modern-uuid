// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_BIT_PACKER_H_INCLUDED
#define HEADER_MODERN_UUID_BIT_PACKER_H_INCLUDED

#include <modern-uuid/common.h>


namespace muuid::impl {

    template<size_t BitsPerByte, size_t PackedBytes>
    requires(BitsPerByte > 0 && BitsPerByte < 8 && PackedBytes > 0)
    class bit_packer {
        static constexpr size_t bits_per_byte = BitsPerByte;
        static constexpr size_t total_bits = PackedBytes * 8;
        static constexpr auto remainder = bit_packer::total_bits % BitsPerByte;
        static constexpr size_t unpacked_bytes = (bit_packer::total_bits / BitsPerByte) + (remainder != 0);
        static constexpr size_t packed_bytes = PackedBytes;

        static constexpr size_t units = (bit_packer::total_bits / 64) + ((bit_packer::total_bits % 64) != 0);

    public:
        constexpr bit_packer() noexcept = default;

        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        constexpr bit_packer(std::span<Byte, bit_packer::packed_bytes> src) noexcept {
            for (size_t src_idx = 0; src_idx != bit_packer::packed_bytes; ++src_idx) {
                uint64_t carry = src[src_idx];
                for(size_t i = 0; i < bit_packer::units; ++i) {
                    auto val = this->m_units[i];
                    uint64_t next_carry = val >> 56;
                    val <<= 8;
                    val |= carry;
                    this->m_units[i] = val;
                    carry = next_carry;
                }
            }
        }

        constexpr void drain(std::span<uint8_t, bit_packer::packed_bytes> dst) noexcept {
            for (size_t dst_idx = bit_packer::packed_bytes; dst_idx != 0; --dst_idx) {
                uint64_t carry = 0;
                for(size_t i = bit_packer::units; i != 0 ; --i) {
                    uint64_t val = this->m_units[i - 1];
                    uint64_t next_carry = val & 0xFF;
                    val >>= 8;
                    val |= (carry << 56);
                    this->m_units[i - 1] = val;
                    carry = next_carry;
                }
                dst[dst_idx - 1] = uint8_t(carry);
            }
        }

        constexpr void push(uint8_t b) noexcept {
            uint64_t carry = b;
            for(size_t i = 0; i < bit_packer::units; ++i) {
                auto val = this->m_units[i];
                uint64_t next_carry = val >> (64 - bit_packer::bits_per_byte);
                val <<= bit_packer::bits_per_byte;
                val |= carry;
                this->m_units[i] = val;
                carry = next_carry;
            }
        }

        constexpr uint8_t pop() noexcept {
            uint64_t carry = 0;
            for(size_t i = bit_packer::units; i != 0 ; --i) {
                auto val = this->m_units[i - 1];
                uint64_t next_carry = val & ((1 << bit_packer::bits_per_byte) - 1);
                val >>= bit_packer::bits_per_byte;
                val |= (carry << (64 - bit_packer::bits_per_byte));
                this->m_units[i - 1] = val;
                carry = next_carry;
            }
            return uint8_t(carry);
        }
    private:
        std::array<uint64_t, bit_packer::units> m_units{};
    };


}

#endif 
