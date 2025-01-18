// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_UUID_H_INCLUDED
#define HEADER_MODERN_UUID_UUID_H_INCLUDED

// Config macros:
// MUUID_MULTITHREADED - auto-detected. Set to 1 to force multi-threaded build and 0 to force single threaded 1
// MUUID_BUILDING_MUUID - set 1 if building the library itself. 

#include <cstdint>
#include <cstring>

#include <concepts>
#include <compare>
#include <array>
#include <span>
#include <string_view>
#include <optional>
#include <limits>
#include <istream>
#include <ostream>

#if !defined(MUUID_MULTITHREADED)
    #if (defined(__STDCPP_THREADS__) || defined(__MT__) || defined(_MT) || defined(_REENTRANT) \
        || defined(_PTHREADS) || defined(__APPLE__) || defined(__DragonFly__))
        
        #define MUUID_MULTITHREADED 1
    #else
        #define MUUID_MULTITHREADED 0
    #endif
#endif

#if MUUID_SHARED
    #if defined(_WIN32) || defined(_WIN64)
        #if MUUID_BUILDING_MUUID
            #define MUUID_EXPORTED __declspec(dllexport)
        #else
            #define MUUID_EXPORTED __declspec(dllimport)
        #endif
    #else
        #define MUUID_EXPORTED __attribute__((visibility("default")))
    #endif
#else
    #define MUUID_EXPORTED
#endif


//See https://github.com/llvm/llvm-project/issues/77773 for the sad story of how feature test
//macros are useless with libc++
#if __cpp_lib_format >= 201907L || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 170000 && __has_include(<format>))

    #define MUUID_SUPPORTS_STD_FORMAT 1

#endif

#if MUUID_SUPPORTS_STD_FORMAT
    #include <format>
#endif

namespace muuid
{
    namespace impl {
        template<class T, size_t Extent>
        std::true_type is_span_helper(std::span<T, Extent> * x);

        std::false_type is_span_helper(...);

        template<class T>
        constexpr bool is_span = decltype(is_span_helper((T *)nullptr))::value;

        template<class T>
        concept byte_like = std::is_standard_layout_v<T> && 
                            sizeof(T) == sizeof(uint8_t) && 
        requires {
            static_cast<T>(uint8_t{});
            static_cast<uint8_t>(T{});
        };

        static_assert(byte_like<char>);
        static_assert(byte_like<unsigned char>);
        static_assert(byte_like<signed char>);
        static_assert(byte_like<std::byte>);

        void invalid_constexpr_call(const char *);
    }

    struct uuid_parts {
        uint32_t    time_low;
        uint16_t    time_mid;
        uint16_t    time_hi_and_version;
        uint16_t    clock_seq;
        uint8_t     node[6];
    };



    class uuid {
    public:
        //see https://datatracker.ietf.org/doc/html/rfc4122#section-4.1.1
        //and https://datatracker.ietf.org/doc/rfc9562/ section 4.1
        enum class variant: uint8_t {
            reserved_ncs        = 0,
            standard            = 1,
            reserved_microsoft  = 2,
            reserved_future     = 3
        };

        //see https://datatracker.ietf.org/doc/html/rfc4122#section-4.1.3
        //and https://datatracker.ietf.org/doc/rfc9562/ section 4.2
        enum class type : uint8_t {
            none                    = 0,
            time_based              = 1,
            dce_security            = 2,
            name_based_md5          = 3,
            random                  = 4,
            name_based_sha1         = 5,
            reordered_time_based    = 6,
            unix_time_based         = 7,
            custom                  = 8,
            reserved9               = 9,
            reserved10              = 10,
            reserved11              = 11,
            reserved12              = 12,
            reserved13              = 13,
            reserved14              = 14,
            reserved15              = 15
        };

        enum format {
            lowercase,
            uppercase
        };
    private:
        template<impl::byte_like Byte, class T>
        static constexpr const uint8_t * read_bytes(const Byte * bytes, T & val) noexcept {
            T tmp = uint8_t(*bytes++);
            for(unsigned i = 0; i < sizeof(T) - 1; ++i)
                tmp = (tmp << 8) | uint8_t(*bytes++);
            val = tmp;
            return bytes;
        }

        template<impl::byte_like Byte, class T>
        static constexpr uint8_t * write_bytes(T val, Byte * bytes) noexcept {
            for(unsigned i = 0; ; ) {
                bytes[sizeof(T) - i - 1] = Byte(static_cast<uint8_t>(val));
                if constexpr (sizeof(T) == 1) {
                    break;
                } else {
                    if (++i == sizeof(T))
                        break;
                    val >>= 8;
                }
            }
            return bytes + sizeof(T);
        }

        static constexpr bool read_hex(const char * str, uint8_t & val) noexcept {
            uint8_t ret = 0;
            for (int i = 0; i < 2; ++i) {
                char c = *str++;
                if (c >= '0' && c <= '9')
                    ret = (ret << 4) | (c - '0');
                else if (c >= 'a' && c <= 'f')
                    ret = (ret << 4) | (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    ret = (ret << 4) | (c - 'A' + 10);
                else 
                    return false;
            }
            val = ret;
            return true;
        }

        static constexpr void write_hex(uint8_t val, char * str, format fmt) noexcept {
            const uint8_t nibbles[] = {uint8_t(val >> 4), uint8_t(val & 0x0F)};
            constexpr char digits[2][17] = {
                "0123456789abcdef",
                "0123456789ABCDEF",
            };
            for (auto nibble: nibbles) {
                *str++ = digits[fmt][nibble];
            }
        }
    public:
        ///Construct a null uuid
        constexpr uuid() noexcept = default;

        ///Construct from a string literal
        consteval uuid(const char (&src)[37]) noexcept {
            const char * str = src;
            uint8_t * data = m_bytes.data();
            for (int i = 0; i < 4; ++i, str += 2, ++data) {
                if (!read_hex(str, *data))
                    impl::invalid_constexpr_call("invalid uuid string");
            }
            if (*str++ != '-')
                impl::invalid_constexpr_call("invalid uuid string");
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, str += 2, ++data) {
                    if (!read_hex(str, *data))
                        impl::invalid_constexpr_call("invalid uuid string");
                }
                if (*str++ != '-')
                    impl::invalid_constexpr_call("invalid uuid string");
            }
            for (int i = 0; i < 6; ++i, str += 2, ++data) {
                if (!read_hex(str, *data))
                    impl::invalid_constexpr_call("invalid uuid string");
            }
            if (*str != 0)
                impl::invalid_constexpr_call("invalid uuid string");
        }

        template<impl::byte_like Byte>
        constexpr uuid(std::span<Byte, 16> src) noexcept {
            std::copy(src.begin(), src.end(), m_bytes.data());
        }

        template<class T>
        requires( !impl::is_span<T> && requires(const T & x) { 
            std::span{x}; 
            requires impl::byte_like<std::remove_reference_t<decltype(*std::span{x}.begin())>>; 
            requires decltype(std::span{x})::extent == 16;
        })
        constexpr uuid(const T & src) noexcept:
            uuid{std::span{src}}
        {}

        constexpr uuid(const uuid_parts & parts) noexcept {
            auto dest = m_bytes.data();
            dest = write_bytes(parts.time_low, dest);
            dest = write_bytes(parts.time_mid, dest);
            dest = write_bytes(parts.time_hi_and_version, dest);
            dest = write_bytes(parts.clock_seq, dest);
            std::copy(std::begin(parts.node), std::end(parts.node), dest);
        }

        MUUID_EXPORTED static auto generate_random() noexcept -> uuid;
        MUUID_EXPORTED static auto generate_md5(uuid ns, std::string_view name) -> uuid;
        MUUID_EXPORTED static auto generate_sha1(uuid ns, std::string_view name) -> uuid;
        MUUID_EXPORTED static auto generate_time_based() -> uuid;
        MUUID_EXPORTED static auto generate_reordered_time_based() -> uuid;
        MUUID_EXPORTED static auto generate_unix_time_based() -> uuid;

        static constexpr uuid max() noexcept 
            { return uuid("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"); }

        constexpr void clear() noexcept {
            *this = uuid();
        }

        constexpr auto get_type() const noexcept -> type {
            return static_cast<type>(m_bytes[6] >> 4);
        }

        constexpr auto get_variant() const noexcept -> variant {
            uint8_t val = m_bytes[8];
            if ((val & 0x80) == 0)
                return variant::reserved_ncs;
            if ((val & 0x40) == 0)
                return variant::standard;
            if ((val & 0x20) == 0)
                return variant::reserved_microsoft;
            return variant::reserved_future;
        }

        constexpr auto bytes() const noexcept -> const std::array<uint8_t, 16> &
            { return m_bytes; }

        constexpr friend auto operator==(const uuid & lhs, const uuid & rhs) noexcept -> bool = default;
        constexpr friend auto operator<=>(const uuid & lhs, const uuid & rhs) noexcept -> std::strong_ordering = default;

        constexpr auto to_parts() const noexcept -> uuid_parts {
            uuid_parts ret;
            auto ptr = m_bytes.data();
            ptr = read_bytes(ptr, ret.time_low);
            ptr = read_bytes(ptr, ret.time_mid);
            ptr = read_bytes(ptr, ret.time_hi_and_version);
            ptr = read_bytes(ptr, ret.clock_seq);
            std::copy(ptr, ptr + 6, ret.node);
            return ret;
        }

        template<size_t Extent>
        static constexpr std::optional<uuid> from_chars(std::span<const char, Extent> src) noexcept {
            if (src.size() < 36)
                return std::nullopt;
            
            uuid ret;
            const char * str = src.data();
            uint8_t * dest = ret.m_bytes.data();
            for(int i = 0; i < 4; ++i, str += 2, ++dest) {
                if (!read_hex(str, *dest))
                    return std::nullopt;
            }
            if (*str++ != '-')
                return std::nullopt;
            for(int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, str += 2, ++dest) {
                    if (!read_hex(str, *dest))
                        return std::nullopt;
                }
                if (*str++ != '-')
                    return std::nullopt;
            }
            for (int i = 0; i < 6; ++i, str += 2, ++dest) {
                if (!read_hex(str, *dest))
                    return std::nullopt;
            }
            return ret;
        }

        template<class T>
        requires( !impl::is_span<T> && requires(T & x) { 
            std::span{x}; 
            requires std::same_as<std::remove_cvref_t<decltype(*std::span{x}.begin())>, char>; 
        })
        static constexpr auto from_chars(const T & src) noexcept
            { return from_chars(std::span{src}); }

        template<size_t Extent>
        [[nodiscard]]
        constexpr auto to_chars(std::span<char, Extent> dest, format fmt = lowercase) const noexcept ->
            std::conditional_t<Extent == std::dynamic_extent, bool, void> {
            
            if constexpr (Extent == std::dynamic_extent) {
                if (dest.size() < 36)
                    return false;
            } else {
                static_assert(Extent >= 36, "destination is too small");
            }

            char * out = dest.data();
            const uint8_t * src = m_bytes.data();
            for (int i = 0; i < 4; ++i, ++src, out += 2)
                write_hex(*src, out, fmt);
            *out++ = '-';
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, ++src, out += 2) {
                    write_hex(*src, out, fmt);
                }
                *out++ = '-';
            }
            for (int i = 0; i < 6; ++i, ++src, out += 2) 
                write_hex(*src, out, fmt);

            if constexpr (Extent == std::dynamic_extent)
                return true;
        }

        template<class T>
        requires( !impl::is_span<T> && requires(T & x) {
            std::span{x}; 
            requires std::is_same_v<std::remove_reference_t<decltype(*std::span{x}.begin())>, char>;
            requires !std::is_const_v<std::remove_reference_t<decltype(*std::span{x}.begin())>>;
        })
        [[nodiscard]]
        constexpr auto to_chars(T & dest, format fmt = lowercase) const noexcept {
            return to_chars(std::span{dest}, fmt);
        }

        constexpr auto to_chars(format fmt = lowercase) const noexcept -> std::array<char, 37> {
            std::array<char, 37> ret;
            to_chars(ret, fmt);
            ret[36] = 0;
            return ret;
        }

        auto to_string(format fmt = lowercase) const -> std::string
        {
            std::string ret(36, '\0');
            (void)to_chars(ret, fmt);
            return ret;
        }

        friend std::ostream & operator<<(std::ostream & str, const uuid val) {
            const auto flags = str.flags();
            const format fmt = (flags & std::ios_base::uppercase ? uppercase : lowercase);
            std::array<char, 36> buf;
            val.to_chars(buf, fmt);
            std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<char>(str));
            return str;
        }

        friend std::istream & operator>>(std::istream & str, uuid & val) {
            std::array<char, 36> buf;
            auto * strbuf = str.rdbuf();
            for(char & c: buf) {
                auto res = strbuf->sbumpc();
                if (res == std::char_traits<char>::eof()) {
                    str.setstate(std::ios_base::eofbit | std::ios_base::failbit);
                    return str;
                }
                c = char(res);
            }
            if (auto maybe_val = uuid::from_chars(buf))
                val = *maybe_val;
            else
                str.setstate(std::ios_base::failbit);
            return str;
        }

        size_t hash_code() const noexcept {
            static_assert(sizeof(uuid) > sizeof(size_t) && sizeof(uuid) % sizeof(size_t) == 0);
            size_t temp;
            const uint8_t * data = m_bytes.data();
            size_t ret = 0;
            for(unsigned i = 0; i < sizeof(uuid) / sizeof(size_t); ++i) {
                memcpy(&temp, data, sizeof(size_t));
                ret = hash_combine(ret, std::hash<size_t>()(temp));
                data += sizeof(size_t);
            }
            return ret;
        }

    private:
        template<std::same_as<size_t> S>
        static constexpr size_t hash_combine(S prev, S next) {
            constexpr auto digits = std::numeric_limits<S>::digits;
            static_assert(digits == 64 || digits == 32);
            
            if constexpr (digits == 64) {
                S x = prev + 0x9e3779b9 + next;
                const S m = 0xe9846af9b1a615d;
                x ^= x >> 32;
                x *= m;
                x ^= x >> 32;
                x *= m;
                x ^= x >> 28;
                return x;
            } else {
                S x = prev + 0x9e3779b9 + next;
                const S m1 = 0x21f0aaad;
                const S m2 = 0x735a2d97;
                x ^= x >> 16;
                x *= m1;
                x ^= x >> 15;
                x *= m2;
                x ^= x >> 15;
                return x;
            }
        }

    private:
        std::array<uint8_t, 16> m_bytes{};
    };

    static_assert(sizeof(uuid) == 16);

    namespace namespaces {
        /* Name string is a fully-qualified domain name */
        constexpr uuid dns("6ba7b810-9dad-11d1-80b4-00c04fd430c8");

        /* Name string is a URL */
        constexpr uuid url("6ba7b811-9dad-11d1-80b4-00c04fd430c8");

        /* Name string is an ISO OID */
        constexpr uuid oid("6ba7b812-9dad-11d1-80b4-00c04fd430c8");

        /* Name string is an X.500 DN (in DER or a text output format) */
        constexpr uuid x500("6ba7b814-9dad-11d1-80b4-00c04fd430c8");

    }
}

template<>
struct std::hash<muuid::uuid> {

    size_t operator()(muuid::uuid val) const noexcept {
        return val.hash_code();
    }
};


#if MUUID_SUPPORTS_STD_FORMAT

template<>
struct std::formatter<muuid::uuid>
{
    using uuid = muuid::uuid;

    uuid::format fmt = uuid::lowercase;

    template<class ParseContext>
    constexpr auto parse(ParseContext & ctx) -> ParseContext::iterator
    {
        auto it = ctx.begin();
        while(it != ctx.end()) {
            if (*it == 'l') {
                this->fmt = uuid::lowercase; ++it;
            } else if (*it == 'u') {
                this->fmt = uuid::uppercase; ++it;
            } else if (*it == '}') {
                break;
            } else {
                throw std::format_error("Invalid format args");
            }
        }
        return it;
    }

    template <typename FormatContext>
    auto format(uuid val, FormatContext & ctx) const -> decltype(ctx.out()) 
    {
        std::array<char, 36> buf;
        val.to_chars(buf, this->fmt);
        return std::copy(buf.begin(), buf.end(), ctx.out());
    }
};

#endif

#endif
