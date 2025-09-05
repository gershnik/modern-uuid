// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_BIT_PACKER_H_INCLUDED
#define HEADER_MODERN_UUID_BIT_PACKER_H_INCLUDED

#include <modern-uuid/common.h>

//We are fallthrough switches throughout this file. Shut the GCC warning up
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif


namespace muuid::impl {

    template<size_t BitsPerByte, size_t PackedBytes> struct bit_packer_impl {
        static constexpr size_t bits_per_byte = BitsPerByte;
        static constexpr size_t total_bits = PackedBytes * 8;
        static constexpr auto remainder = bit_packer_impl::total_bits % BitsPerByte;
        static constexpr size_t unpacked_bytes = (bit_packer_impl::total_bits / BitsPerByte) + (remainder != 0);
        static constexpr size_t packed_bytes = PackedBytes;
    
        static constexpr size_t lcm = std::lcm(BitsPerByte, 8);
        static constexpr auto full_rounds = bit_packer_impl::total_bits / bit_packer_impl::lcm;
    };

    template<size_t BitsPerByte, size_t PackedBytes> class bit_packer;

    template<size_t PackedBytes> 
    requires(PackedBytes > 0)
    class bit_packer<2, PackedBytes> : private bit_packer_impl<2, PackedBytes> {
        static_assert(bit_packer::unpacked_bytes == 4 * bit_packer::packed_bytes);
    public:
        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void pack_bits(std::span<Byte, bit_packer::unpacked_bytes> src, std::span<uint8_t, bit_packer::packed_bytes> dst) noexcept {
            size_t i = 0, j = 0;
            do {
                dst[i] = src[j++] << 6;
                dst[i] |= src[j++] << 4;
                dst[i] |= src[j++] << 2;
                dst[i] |= src[j++];
                ++i;
            } while (i < bit_packer::packed_bytes);
        }

        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void unpack_bits(std::span<Byte, bit_packer::packed_bytes> src, std::span<uint8_t, bit_packer::unpacked_bytes> dst) noexcept {
            size_t i = 0, j = 0;
            do {
                dst[i++] = src[j] >> 6;
                dst[i++] = (src[j] >> 4) & 0x03u;
                dst[i++] = (src[j] >> 2) & 0x03u;
                dst[i++] = src[j] & 0x03u;
                j++;
            } while (i < bit_packer::unpacked_bytes);
        }
    };

    template<size_t PackedBytes> 
    requires(PackedBytes > 0)
    class bit_packer<3, PackedBytes> : private bit_packer_impl<3, PackedBytes> {
    public:
        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void pack_bits(std::span<Byte, bit_packer::unpacked_bytes> src, std::span<uint8_t, bit_packer::packed_bytes> dst) noexcept {
            if constexpr (bit_packer::remainder != 0)
                dst[0] = 0;

            size_t i = 0, j = 0;
            //For reasons unknown MSVC doesn't compile Duff's device properly here in constexpr mode
            //so we use a mutable state and hope that optimizer will figure it out :(
            auto state = bit_packer::remainder;
            do {
                switch (state) {
                case 0: dst[i] = src[j++] << 5;   // |0|0|0| | | | | |
                        dst[i] |= src[j++] << 2;  // | | | |1|1|1| | |
                        dst[i] |= src[j] >> 1;    // | | | | | | |2|2|
                        ++i;
                case 1: dst[i] = src[j++] << 7;   // |2| | | | | | | |
                        dst[i] |= src[j++] << 4;  // | |3|3|3| | | | |
                        dst[i] |= src[j++] << 1;  // | | | | |4|4|4| |
                        dst[i] |= src[j] >> 2;    // | | | | | | | |5|
                        ++i;
                case 2: dst[i] = src[j++] << 6;   // |5|5| | | | | | |
                        dst[i] |= src[j++] << 3;  // | | |6|6|6| | | |
                        dst[i] |= src[j++];       // | | | | | |7|7|7|
                        ++i;
                }
                state = 0;
            } while(i < bit_packer::full_rounds * (bit_packer::lcm / 8));
        }

        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void unpack_bits(std::span<Byte, bit_packer::packed_bytes> src, std::span<uint8_t, bit_packer::unpacked_bytes> dst) noexcept {
            if constexpr (bit_packer::remainder != 0)
                dst[0] = 0;

            size_t i = 0, j = 0;
            //For reasons unknown MSVC doesn't compile Duff's device properly here in constexpr mode
            //so we use a mutable state and hope that optimizer will figure it out :(
            auto state = bit_packer::remainder;
            do {
                switch (state) {
                case 0: dst[i] = src[j] >> 5;              // | | | | | |0|0|0|
                        ++i;
                        dst[i] = (src[j] >> 2) & 0x7u;     // | | | | | |0|0|0|
                        ++i;
                        dst[i] = (src[j++] << 1) & 0x7u;   // | | | | | |0|0| |
                case 1: dst[i] |= src[j] >> 7;             // | | | | | | | |1|
                        ++i;
                        dst[i] = (src[j] >> 4) & 0x7u;     // | | | | | |1|1|1|
                        ++i;
                        dst[i] = (src[j] >> 1) & 0x7u;     // | | | | | |1|1|1|
                        ++i;
                        dst[i] = (src[j++] << 2) & 0x7u;   // | | | | | |1| | |
                case 2: dst[i] |= src[j] >> 6;             // | | | | | | |2|2|
                        ++i;
                        dst[i] = (src[j] >> 3) & 0x7u;     // | | | | | |2|2|2|
                        ++i;
                        dst[i] = src[j++] & 0x7u;          // | | | | | |2|2|2|
                        ++i;
                }
                state = 0;
            } while(i < bit_packer::full_rounds * bit_packer::lcm / bit_packer::bits_per_byte);
        }
    };


    template<size_t PackedBytes> 
    requires(PackedBytes > 0)
    class bit_packer<4, PackedBytes> : private bit_packer_impl<4, PackedBytes> {
        static_assert(bit_packer::unpacked_bytes == 2 * bit_packer::packed_bytes);
    public:
        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void pack_bits(std::span<Byte, bit_packer::unpacked_bytes> src, std::span<uint8_t, bit_packer::packed_bytes> dst) noexcept {
            size_t i = 0, j = 0;
            do {
                dst[i] = src[j++] << 4;
                dst[i] |= src[j++];
                ++i;
            } while (i < bit_packer::packed_bytes);
        }

        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void unpack_bits(std::span<Byte, bit_packer::packed_bytes> src, std::span<uint8_t, bit_packer::unpacked_bytes> dst) noexcept {
            size_t i = 0, j = 0;
            do {
                dst[i++] = src[j] >> 4;
                dst[i++] = src[j] & 0x0Fu;
                j++;
            } while (i < bit_packer::unpacked_bytes);
        }

    };

    template<size_t PackedBytes> 
    requires(PackedBytes > 0)
    class bit_packer<5, PackedBytes> : private bit_packer_impl<5, PackedBytes> {
    public:
        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void pack_bits(std::span<Byte, bit_packer::unpacked_bytes> src, std::span<uint8_t, bit_packer::packed_bytes> dst) noexcept {
            if constexpr (bit_packer::remainder != 0)
                dst[0] = 0;

            size_t i = 0, j = 0;
            //For reasons unknown MSVC doesn't compile Duff's device properly here in constexpr mode
            //so we use a mutable state and hope that optimizer will figure it out :(
            auto state = bit_packer::remainder;
            do {
                switch (state) {
                case 0: dst[i] = src[j++] << 3;   // |0|0|0|0|0| | | |
                        dst[i] |= src[j] >> 2;    // | | | | | |1|1|1|
                        ++i;
                case 2: dst[i] = src[j++] << 6;   // |1|1| | | | | | |
                        dst[i] |= src[j++] << 1;  // | | |2|2|2|2|2| |
                        dst[i] |= src[j] >> 4;    // | | | | | | | |3|
                        ++i; 
                case 4: dst[i] = src[j++] << 4;   // |3|3|3|3| | | | |
                        dst[i] |= src[j] >> 1;    // | | | | |4|4|4|4|
                        ++i;
                case 1: dst[i] = src[j++] << 7;   // |4| | | | | | | |
                        dst[i] |= src[j++] << 2;  // | |5|5|5|5|5| | |
                        dst[i] |= src[j] >> 3;    // | | | | | | |6|6|
                        ++i;
                case 3: dst[i] = src[j++] << 5;   // |6|6|6| | | | | |
                        dst[i] |= src[j++];       // | | | |7|7|7|7|7|
                        ++i;
                }
                state = 0;
            } while(i < bit_packer::full_rounds * (bit_packer::lcm / 8));
        }

        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void unpack_bits(std::span<Byte, bit_packer::packed_bytes> src, std::span<uint8_t, bit_packer::unpacked_bytes> dst) noexcept {
            if constexpr (bit_packer::remainder != 0)
                dst[0] = 0;

            size_t i = 0, j = 0;
            //For reasons unknown MSVC doesn't compile Duff's device properly here in constexpr mode
            //so we use a mutable state and hope that optimizer will figure it out :(
            auto state = bit_packer::remainder;
            do {
                switch (state) {
                case 0: dst[i] = src[j] >> 3;              // | | | |0|0|0|0|0|
                        ++i;
                        dst[i] = (src[j++] << 2) & 0x1Fu;  // | | | |0|0|0| | |
                case 2: dst[i] |= src[j] >> 6;             // | | | | | | |1|1|
                        ++i;
                        dst[i] = (src[j] >> 1) & 0x1Fu;    // | | | |1|1|1|1|1|
                        ++i;
                        dst[i] = (src[j++] << 4) & 0x1Fu;  // | | | |1| | | | |
                case 4: dst[i] |= src[j] >> 4;             // | | | | |2|2|2|2|
                        ++i;
                        dst[i] = (src[j++] << 1) & 0x1Fu;  // | | | |2|2|2|2| |
                case 1: dst[i] |= src[j] >> 7;             // | | | | | | | |3|
                        ++i;
                        dst[i] = (src[j] >> 2) & 0x1Fu;    // | | | |3|3|3|3|3|
                        ++i;
                        dst[i] = (src[j++] << 3) & 0x1Fu;  // | | | |3|3| | | |
                case 3: dst[i] |= src[j] >> 5;             // | | | | | |4|4|4|
                        ++i;
                        dst[i] =  src[j++] & 0x1Fu;        // | | | |4|4|4|4|4|
                        ++i;
                }
                state = 0;
            } while(i < bit_packer::full_rounds * bit_packer::lcm / bit_packer::bits_per_byte);
        }
    };

    template<size_t PackedBytes> 
    requires(PackedBytes > 0)
    class bit_packer<6, PackedBytes> : private bit_packer_impl<6, PackedBytes> {
    public:
        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void pack_bits(std::span<Byte, bit_packer::unpacked_bytes> src, std::span<uint8_t, bit_packer::packed_bytes> dst) noexcept {
            if constexpr (bit_packer::remainder != 0)
                dst[0] = 0;

            size_t i = 0, j = 0;
            //For reasons unknown MSVC doesn't compile Duff's device properly here in constexpr mode
            //so we use a mutable state and hope that optimizer will figure it out :(
            auto state = bit_packer::remainder;
            do {
                switch (state) {
                case 0: dst[i] = src[j++] << 2;   // |0|0|0|0|0|0| | |
                        dst[i] |= src[j] >> 4;    // | | | | | | |1|1|
                        ++i;
                case 4: dst[i] = src[j++] << 4;   // |1|1|1|1| | | | |
                        dst[i] |= src[j] >> 2;    // | | | | |2|2|2|2|
                        ++i;
                case 2: dst[i] = src[j++] << 6;   // |2|2| | | | | | |
                        dst[i] |= src[j++];       // | | |3|3|3|3|3|3|
                        ++i;
                }
                state = 0;
            } while(i < bit_packer::full_rounds * (bit_packer::lcm / 8));
        }

        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void unpack_bits(std::span<Byte, bit_packer::packed_bytes> src, std::span<uint8_t, bit_packer::unpacked_bytes> dst) noexcept {
            if constexpr (bit_packer::remainder != 0)
                dst[0] = 0;

            size_t i = 0, j = 0;
            //For reasons unknown MSVC doesn't compile Duff's device properly here in constexpr mode
            //so we use a mutable state and hope that optimizer will figure it out :(
            auto state = bit_packer::remainder;
            do {
                switch (state) {
                case 0: dst[i] = src[j] >> 2;              // | | |0|0|0|0|0|0|
                        ++i;
                        dst[i] = (src[j++] << 4) & 0x3Fu;  // | | |0|0| | | | |
                case 4: dst[i] |= src[j] >> 4;             // | | | | |1|1|1|1|
                        ++i;
                        dst[i] = (src[j++] << 2) & 0x3Fu;  // | | |1|1|1|1| | |
                case 2: dst[i] |= src[j] >> 6;             // | | | | | | |2|2|
                        ++i;
                        dst[i] = src[j++] & 0x3Fu;         // | | |2|2|2|2|2|2|
                        ++i;
                }
                state = 0;
            } while(i < bit_packer::full_rounds * bit_packer::lcm / bit_packer::bits_per_byte);
        }

    };

    template<size_t PackedBytes> 
    requires(PackedBytes > 0)
    class bit_packer<7, PackedBytes> : private bit_packer_impl<7, PackedBytes> {
    public:
        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void pack_bits(std::span<Byte, bit_packer::unpacked_bytes> src, std::span<uint8_t, bit_packer::packed_bytes> dst) noexcept {
            if constexpr (bit_packer::remainder != 0)
                dst[0] = 0;

            size_t i = 0, j = 0;
            //For reasons unknown MSVC doesn't compile Duff's device properly here in constexpr mode
            //so we use a mutable state and hope that optimizer will figure it out :(
            auto state = bit_packer::remainder;
            do {
                switch (state) {
                case 0: dst[i] = src[j++] << 1;   // |0|0|0|0|0|0|0| |
                        dst[i] |= src[j] >> 6;    // | | | | | | | |1|
                        ++i;
                case 6: dst[i] = src[j++] << 2;   // |1|1|1|1|1|1| | |
                        dst[i] |= src[j] >> 5;    // | | | | | | |2|2|
                        ++i;
                case 5: dst[i] = src[j++] << 3;   // |2|2|2|2|2| | | |
                        dst[i] |= src[j] >> 4;    // | | | | | |3|3|3|
                        ++i;
                case 4: dst[i] = src[j++] << 4;   // |3|3|3|3| | | | |
                        dst[i] |= src[j] >> 3;    // | | | | |4|4|4|4|
                        ++i;
                case 3: dst[i] = src[j++] << 5;   // |4|4|4| | | | | |
                        dst[i] |= src[j] >> 2;    // | | | |5|5|5|5|5|
                        ++i;
                case 2: dst[i] = src[j++] << 6;   // |5|5| | | | | | |
                        dst[i] |= src[j] >> 1;    // | | |6|6|6|6|6|6|
                        ++i;
                case 1: dst[i] = src[j++] << 7;   // |6| | | | | | | |
                        dst[i] |= src[j++];       // | |7|7|7|7|7|7|7|
                        ++i;
                }
                state = 0;
            } while(i < bit_packer::full_rounds * (bit_packer::lcm / 8));
        }

        template<class Byte>
        requires(std::is_same_v<std::remove_cv_t<Byte>, uint8_t>)
        static constexpr 
        void unpack_bits(std::span<Byte, bit_packer::packed_bytes> src, std::span<uint8_t, bit_packer::unpacked_bytes> dst) noexcept {
            if constexpr (bit_packer::remainder != 0)
                dst[0] = 0;

            size_t i = 0, j = 0;
            //For reasons unknown MSVC doesn't compile Duff's device properly here in constexpr mode
            //so we use a mutable state and hope that optimizer will figure it out :(
            auto state = bit_packer::remainder;
            do {
                switch (state) {
                case 0: dst[i] = src[j] >> 1;              // | |0|0|0|0|0|0|0|
                        ++i;
                        dst[i] = (src[j++] << 6) & 0x7Fu;  // | |0| | | | | | |
                case 6: dst[i] |= src[j] >> 2;             // | | |1|1|1|1|1|1|
                        ++i;
                        dst[i] = (src[j++] << 5) & 0x7Fu;  // | |1|1| | | | | |
                case 5: dst[i] |= src[j] >> 3;             // | | | |2|2|2|2|2|
                        ++i;
                        dst[i] = (src[j++] << 4) & 0x7Fu;  // | |2|2|2| | | | |
                case 4: dst[i] |= src[j] >> 4;             // | | | | |3|3|3|3|
                        ++i;
                        dst[i] = (src[j++] << 3) & 0x7Fu;  // | |3|3|3|3| | | |
                case 3: dst[i] |= src[j] >> 5;             // | | | | | |4|4|4|
                        ++i;
                        dst[i] = (src[j++] << 2) & 0x7Fu;  // | |4|4|4|4|4| | |
                case 2: dst[i] |= src[j] >> 6;             // | | | | | | |5|5|
                        ++i;
                        dst[i] = (src[j++] << 1) & 0x7Fu;  // | |5|5|5|5|5|5| |
                case 1: dst[i] |= src[j] >> 7;             // | | | | | | | |6|
                        ++i;
                        dst[i] = src[j++] & 0x7Fu;         // | |6|6|6|6|6|6|6|
                        ++i;
                }
                state = 0;
            } while(i < bit_packer::full_rounds * bit_packer::lcm / bit_packer::bits_per_byte);
        }

    };

}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#endif 
