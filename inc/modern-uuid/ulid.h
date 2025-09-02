// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_ULID_H_INCLUDED
#define HEADER_MODERN_UUID_ULID_H_INCLUDED

#include <modern-uuid/common.h>
#include <modern-uuid/bit_packer.h>

namespace muuid {
    namespace impl {
        template<char_like C> struct ulid_char_traits;

        #define MUUID_MAKE_ULID_CHAR_TRAITS(type, pr) \
            template<> struct ulid_char_traits<type> { \
                static constexpr type fmt_l = pr##'l'; \
                static constexpr type fmt_u = pr##'u'; \
                static constexpr type fmt_cl_br = pr##'}'; \
                \
                static constexpr type zero = pr##'0'; \
                static constexpr type nine = pr##'9'; \
                static constexpr type a = pr##'a'; \
                static constexpr type z = pr##'z'; \
                static constexpr type A = pr##'A'; \
                static constexpr type Z = pr##'Z'; \
                \
                static inline constexpr type base32_alphabet[2][33] = { \
                    pr##"0123456789abcdefghjkmnpqrstvwxyz", \
                    pr##"0123456789ABCDEFGHJKMNPQRSTVWXYZ", \
                }; \
            }

        MUUID_MAKE_ULID_CHAR_TRAITS(char, );
        MUUID_MAKE_ULID_CHAR_TRAITS(wchar_t, L);
        MUUID_MAKE_ULID_CHAR_TRAITS(char16_t, u);
        MUUID_MAKE_ULID_CHAR_TRAITS(char32_t, U);
        MUUID_MAKE_ULID_CHAR_TRAITS(char8_t, u8);

        #undef MUUID_MAKE_ULID_CHAR_TRAITS

        static inline constexpr uint8_t base32_letter_values[26] = {
        //   a  b  c  d  e  f  g  h  i  j  k  l  m  n  o  p  q  r  s  t  u  v  w  x  y  z
            10,11,12,13,14,15,16,17, 1,18,19, 1,20,21, 0,22,23,24,25,26,32,27,28,29,30,31
        };

    }

    class ulid {
    public:
        /// Whether to print ulid in lower or upper case
        enum format {
            lowercase,
            uppercase
        };
    private:
        template<impl::char_like T>
        static constexpr bool read(const T * str, std::span<uint8_t, 16> dest) noexcept {
            using tr = impl::ulid_char_traits<T>;

            uint8_t buf[26];
            auto acc = std::span(buf);
            for(int i = 0; i < 26; ++i) {
                T c = str[i];
                uint8_t val = 32;
                if (c >= tr::zero && c <= tr::nine)
                    val = uint8_t(c - tr::zero);
                else if (c >= tr::a && c <= tr::z)
                    val = impl::base32_letter_values[c - tr::a];
                else if (c >= tr::A && c <= tr::Z)
                    val = impl::base32_letter_values[c - tr::A];
                if (val > 31)
                    return false;
                acc[i] = val;
            }
            if (acc[0] > 7)
                return false;
            impl::bit_packer<5, 16>::pack_bits(acc, dest);
            return true;
        }

        template<impl::char_like T>
        static constexpr void write(std::span<const uint8_t, 16> src, T * str, format fmt) noexcept {
            using tr = impl::ulid_char_traits<T>;

            uint8_t buf[26];
            impl::bit_packer<5, 16>::unpack_bits(src, std::span(buf));
            for(uint8_t val: buf) {
                *str++ = tr::base32_alphabet[fmt][val];
            }
        }
    public:
        std::array<uint8_t, 16> bytes{};

    public:
        ///Constructs a zeroed out ULID
        constexpr ulid() noexcept = default;

        ///Constructs ulid from a string literal
        template<impl::char_like T>
        consteval ulid(const T (&src)[27]) noexcept {            
            if (!ulid::read(src, this->bytes) || src[26] != 0)
                impl::invalid_constexpr_call("invalid ulid string");
        }

        /// Constructs ulid from a span of 16 byte-like objects
        template<impl::byte_like Byte>
        constexpr ulid(std::span<Byte, 16> src) noexcept {
            std::copy(src.begin(), src.end(), this->bytes.data());
        }

        /// Constructs ulid from anything convertible to a span of 16 byte-like objects
        template<class T>
        requires( !impl::is_span<T> && requires(const T & x) { 
            std::span{x}; 
            requires impl::byte_like<std::remove_reference_t<decltype(*std::span{x}.begin())>>; 
            requires decltype(std::span{x})::extent == 16;
        })
        constexpr ulid(const T & src) noexcept:
            ulid{std::span{src}}
        {}

        /// Generates a ULID
        MUUID_EXPORTED static auto generate() -> ulid;

        /// Returns a Max ULID
        static constexpr ulid max() noexcept 
            { return ulid("7ZZZZZZZZZZZZZZZZZZZZZZZZZ"); }

        /// Resets the object to a Nil ULID
        constexpr void clear() noexcept {
            *this = ulid();
        }

        constexpr friend auto operator==(const ulid & lhs, const ulid & rhs) noexcept -> bool = default;
        constexpr friend auto operator<=>(const ulid & lhs, const ulid & rhs) noexcept -> std::strong_ordering = default;


        /// Parses ulid from a span of characters
        template<impl::char_like T, size_t Extent>
        static constexpr std::optional<ulid> from_chars(std::span<const T, Extent> src) noexcept {
            if (src.size() < 26)
                return std::nullopt;
            ulid ret;
            if (!ulid::read(src.data(), ret.bytes))
                return std::nullopt;
            return ret;
        }

        /// Parses ulid from anything convertible to a span of characters
        template<class T>
        requires( !impl::is_span<T> && requires(T & x) { 
            std::span{x}; 
            requires impl::char_like<std::remove_cvref_t<decltype(*std::span{x}.begin())>>; 
        })
        static constexpr auto from_chars(const T & src) noexcept
            { return ulid::from_chars(std::span{src}); }


        /// Formats ulid into a span of characters
        template<impl::char_like T, size_t Extent>
        [[nodiscard]]
        constexpr auto to_chars(std::span<T, Extent> dest, format fmt = ulid::lowercase) const noexcept ->
            std::conditional_t<Extent == std::dynamic_extent, bool, void> {
            
            if constexpr (Extent == std::dynamic_extent) {
                if (dest.size() < 26)
                    return false;
            } else {
                static_assert(Extent >= 26, "destination is too small");
            }

            ulid::write(this->bytes, dest.data(), fmt);

            if constexpr (Extent == std::dynamic_extent)
                return true;
        }

        /// Formats ulid into anything convertible to a span of characters
        template<class T>
        requires( !impl::is_span<T> && requires(T & x) {
            std::span{x}; 
            requires impl::char_like<std::remove_reference_t<decltype(*std::span{x}.begin())>>;
            requires !std::is_const_v<std::remove_reference_t<decltype(*std::span{x}.begin())>>;
        })
        [[nodiscard]]
        constexpr auto to_chars(T & dest, format fmt = lowercase) const noexcept {
            return this->to_chars(std::span{dest}, fmt);
        }

        /// Returns a character array with formatted ulid
        template<impl::char_like T = char>
        constexpr auto to_chars(format fmt = lowercase) const noexcept -> std::array<T, 26> {
            std::array<T, 26> ret;
            this->to_chars(ret, fmt);
            return ret;
        }


        template<impl::char_like T = char>
    #if __cpp_lib_constexpr_string >= 201907L
        constexpr 
    #endif
        /// Returns a string with formatted ulid
        auto to_string(format fmt = lowercase) const -> std::basic_string<T>
        {
            std::basic_string<T> ret(26, T(0));
            (void)to_chars(ret, fmt);
            return ret;
        }

        /// Prints ulid into an ostream
        template<impl::char_like T>
        friend std::basic_ostream<T> & operator<<(std::basic_ostream<T> & str, const ulid val) {
            const auto flags = str.flags();
            const ulid::format fmt = (flags & std::ios_base::uppercase ? ulid::uppercase : ulid::lowercase);
            std::array<T, 26> buf;
            val.to_chars(buf, fmt);
            std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<T>(str));
            return str;
        }

        /// Reads ulid from an istream
        template<impl::char_like T>
        friend std::basic_istream<T> & operator>>(std::basic_istream<T> & str, ulid & val) {
            std::array<T, 26> buf;
            auto * strbuf = str.rdbuf();
            for(T & c: buf) {
                auto res = strbuf->sbumpc();
                if (res == std::char_traits<T>::eof()) {
                    str.setstate(std::ios_base::eofbit | std::ios_base::failbit);
                    return str;
                }
                c = T(res);
            }
            if (auto maybe_val = ulid::from_chars(buf))
                val = *maybe_val;
            else
                str.setstate(std::ios_base::failbit);
            return str;
        }

        /// Returns hash code for the ulid
        friend constexpr size_t hash_value(const ulid & val) noexcept {
            static_assert(sizeof(ulid) > sizeof(size_t) && sizeof(ulid) % sizeof(size_t) == 0);
            size_t temp;
            const uint8_t * data = val.bytes.data();
            size_t ret = 0;
            for(unsigned i = 0; i < sizeof(ulid) / sizeof(size_t); ++i) {
                memcpy(&temp, data, sizeof(size_t));
                ret = impl::hash_combine(ret, temp);
                data += sizeof(size_t);
            }
            return ret;
        }
    };

    static_assert(sizeof(ulid) == 16);

    namespace impl {
        template<class Derived, class CharT>
        struct ulid_formatter_base {
            ulid::format fmt = ulid::lowercase;

            template<class ParseContext>
            constexpr auto parse(ParseContext & ctx) -> typename ParseContext::iterator {
                using tr = ulid_char_traits<CharT>;

                auto it = ctx.begin();
                while(it != ctx.end()) {
                    if (*it == tr::fmt_l) {
                        this->fmt = ulid::lowercase; ++it;
                    } else if (*it == tr::fmt_u) {
                        this->fmt = ulid::uppercase; ++it;
                    } else if (*it == tr::fmt_cl_br) {
                        break;
                    } else {
                        static_cast<Derived *>(this)->raise_exception("Invalid format args");
                    }
                }
                return it;
            }

            template <typename FormatContext>
            auto format(ulid val, FormatContext & ctx) const -> decltype(ctx.out())  {
                std::array<CharT, 26> buf;
                val.to_chars(buf, this->fmt);
                return std::copy(buf.begin(), buf.end(), ctx.out());
            }
        };
    }
}

/// std::hash specialization for ulid
template<>
struct std::hash<muuid::ulid> {

    constexpr size_t operator()(const muuid::ulid & val) const noexcept {
        return hash_value(val);
    }
};


#if MUUID_SUPPORTS_STD_FORMAT

/// ulid formatter for std::format
template<class CharT>
struct std::formatter<::muuid::ulid, CharT> : 
    public ::muuid::impl::ulid_formatter_base<std::formatter<::muuid::ulid, CharT>, CharT>
{
    [[noreturn]] void raise_exception(const char * message) {
        MUUID_THROW(std::format_error(message));
    }
};

#endif

#if MUUID_SUPPORTS_FMT_FORMAT

/// ulid formatter for fmt::format
template<class CharT>
struct fmt::formatter<::muuid::ulid, CharT> : 
    public ::muuid::impl::ulid_formatter_base<fmt::formatter<::muuid::ulid, CharT>, CharT>
{
    void raise_exception(const char * message) {
        FMT_THROW(fmt::format_error(message));
    }
};

#endif


namespace muuid {

    /// Clock persistence data for ULID
    struct ulid_persistence_data {
        using time_point_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
        
        /**
         * The last known clock reading.
         *  
         * You can also use this value to optimize writing to persistent storage
         */
        time_point_t when; 
        /// Opaque value. Save/restore it but do not otherwise depend on its value
        int32_t adjustment;
        /// Opaque value. Save/restore it but do not otherwise depend on its value
        uint8_t random[80];
    };

    /// Callback interface to handle persistence of clock data for ULID
    using ulid_clock_persistence = generic_clock_persistence<ulid_persistence_data>;

    /**
     * Set the ulid_clock_persistence instance for ulid::generate()
     * 
     * Pass `nullptr` to remove.
     */
    MUUID_EXPORTED void set_ulid_persistence(ulid_clock_persistence * persistence);

}

#endif
