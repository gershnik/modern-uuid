// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_UUID_H_INCLUDED
#define HEADER_MODERN_UUID_UUID_H_INCLUDED

// Config macros:
//
// The following macros must be defined identically when using and building this library:
//
// MUUID_MULTITHREADED - auto-detected. Set to 1 to force multi-threaded build and 0 to force single threaded 1
// MUUID_USE_EXCEPTIONS - auto-detected. Set to 1 to force usage of exceptions and 0 to force not using them
// MUUID_SHARED - set to 1 when using/building a shared library version of the library
//
// The following macro should be set when building the library itself but not when using it:
//
// MUUID_BUILDING_MUUID - set 1 if building the library itself. 

#include <cstdint>
#include <cstring>
#include <cstdio>

#include <concepts>
#include <compare>
#include <array>
#include <span>
#include <string_view>
#include <optional>
#include <limits>
#include <chrono>
#include <istream>
#include <ostream>

#if !defined(MUUID_MULTITHREADED)
    #if defined(_MSC_VER) && !defined(_MT)
        #define MUUID_MULTITHREADED 0
    #elif defined(_LIBCPP_VERSION) && (defined(_LIBCPP_HAS_NO_THREADS) || defined(_LIBCPP_HAS_THREADS) && !_LIBCPP_HAS_THREADS)
        #define MUUID_MULTITHREADED 0
    #elif defined(__GLIBCXX__) && !_GLIBCXX_HAS_GTHREADS
        #define MUUID_MULTITHREADED 0
    #elif !__has_include(<thread>) || !__has_include(<mutex>) || !__has_include(<atomic>)
        #define MUUID_MULTITHREADED 0
    #else
        #define MUUID_MULTITHREADED 1
    #endif
#endif

#if !defined(MUUID_USE_EXCEPTIONS)
    #if defined(__GNUC__) && !defined(__EXCEPTIONS)
        #define MUUID_USE_EXCEPTIONS 0
    #elif defined(__clang__) && !defined(__cpp_exceptions)
        #define MUUID_USE_EXCEPTIONS 0
    #elif defined(_MSC_VER) && !_HAS_EXCEPTIONS 
        #define MUUID_USE_EXCEPTIONS 0
    #else
        #define MUUID_USE_EXCEPTIONS 1
    #endif
#endif

#if MUUID_SHARED
    #if defined(_WIN32) || defined(_WIN64)
        #if MUUID_BUILDING_MUUID
            #define MUUID_EXPORTED __declspec(dllexport)
        #else
            #define MUUID_EXPORTED __declspec(dllimport)
        #endif
    #elif defined(__GNUC__)
        #define MUUID_EXPORTED [[gnu::visibility("default")]]
    #else
        #define MUUID_EXPORTED
    #endif
#else
    #define MUUID_EXPORTED
#endif


//See https://github.com/llvm/llvm-project/issues/77773 for the sad story of how feature test
//macros are useless with libc++
#if (__cpp_lib_format >= 201907L || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 170000)) && __has_include(<format>)

    #define MUUID_SUPPORTS_STD_FORMAT 1

#endif

#if defined(FMT_VERSION) && FMT_VERSION >= 60000 && defined(FMT_THROW)

    #define MUUID_SUPPORTS_FMT_FORMAT 1

#endif

#if MUUID_USE_FMT && !MUUID_SUPPORTS_FMT_FROMAT

    #error "MUUID_USE_FMT is requested but fmt library (of version >= 5.0) is not detected. Did you forget to include <fmt/format.h> before this header?"

#endif

#if MUUID_SUPPORTS_STD_FORMAT
    #include <format>
#endif

#if defined(__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(_WIN32)
    #include <guiddef.h>
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

        template<class T>
        concept char_like = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> ||
                            std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

        template<char_like C> struct uuid_char_traits;

        template<> struct uuid_char_traits<char> { 
            static constexpr char dash = '-'; 
            static constexpr char fmt_l = 'l';
            static constexpr char fmt_u = 'u';
            static constexpr char fmt_cl_br = '}';
            static inline constexpr char digits[2][17] = {
                "0123456789abcdef",
                "0123456789ABCDEF",
            };
        };
        template<> struct uuid_char_traits<wchar_t> { 
            static constexpr wchar_t dash = L'-'; 
            static constexpr wchar_t fmt_l = L'l';
            static constexpr wchar_t fmt_u = L'u';
            static constexpr wchar_t fmt_cl_br = L'}';
            static inline constexpr wchar_t digits[2][17] = {
                L"0123456789abcdef",
                L"0123456789ABCDEF",
            };
        };
        template<> struct uuid_char_traits<char16_t> { 
            static constexpr char16_t dash = u'-'; 
            static constexpr char16_t fmt_l = u'l';
            static constexpr char16_t fmt_u = u'u';
            static constexpr char16_t fmt_cl_br = u'}';
            static inline constexpr char16_t digits[2][17] = {
                u"0123456789abcdef",
                u"0123456789ABCDEF",
            };
        };
        template<> struct uuid_char_traits<char32_t> { 
            static constexpr char32_t dash = U'-'; 
            static constexpr char32_t fmt_l = U'l';
            static constexpr char32_t fmt_u = U'u';
            static constexpr char32_t fmt_cl_br = U'}';
            static inline constexpr char32_t digits[2][17] = {
                U"0123456789abcdef",
                U"0123456789ABCDEF",
            };
        };
        template<> struct uuid_char_traits<char8_t> { 
            static constexpr char8_t dash = u8'-'; 
            static constexpr char8_t fmt_l = u8'l';
            static constexpr char8_t fmt_u = u8'u';
            static constexpr char8_t fmt_cl_br = u8'}';
            static inline constexpr char8_t digits[2][17] = {
                u8"0123456789abcdef",
                u8"0123456789ABCDEF",
            };
        };


        void invalid_constexpr_call(const char *);

        #if MUUID_USE_EXCEPTIONS
            #define MUUID_THROW(x) throw x
        #else
            [[noreturn]] inline void fail(const char* message) {
                fprintf(stderr, "muuid: fatal error: %s", message);
                abort();
            }
            #define MUUID_THROW(x) ::muuid::impl::fail((x).what())
        #endif

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
        /// UUID variant
        /// see https://datatracker.ietf.org/doc/html/rfc4122#section-4.1.1
        /// and https://datatracker.ietf.org/doc/rfc9562/ section 4.1
        enum class variant: uint8_t {
            reserved_ncs        = 0,
            standard            = 1,
            reserved_microsoft  = 2,
            reserved_future     = 3
        };

        /// UUID type
        /// Only valid for variant::standard UUIDs
        /// see https://datatracker.ietf.org/doc/html/rfc4122#section-4.1.3
        /// and https://datatracker.ietf.org/doc/rfc9562/ section 4.2
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

        /// Whether to print uuid in lower or upper case
        enum format {
            lowercase,
            uppercase
        };

        struct namespaces;
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
            bytes[sizeof(T) - 1] = Byte(static_cast<uint8_t>(val));
            if constexpr (sizeof(T) > 1) {
                for(unsigned i = 1; i != sizeof(T); ++i) {
                    val >>= 8;
                    bytes[sizeof(T) - i - 1] = Byte(static_cast<uint8_t>(val));
                }
            }
            return bytes + sizeof(T);
        }

        template<impl::char_like T>
        static constexpr bool read_hex(const T * str, uint8_t & val) noexcept {
            using tr = impl::uuid_char_traits<T>;
            constexpr T zero = tr::digits[0][0];
            constexpr T nine = tr::digits[0][9];
            constexpr T letter_a = tr::digits[0][10];
            constexpr T letter_f = tr::digits[0][15];
            constexpr T letter_A = tr::digits[1][10];
            constexpr T letter_F = tr::digits[1][15];

            uint8_t ret = 0;
            for (int i = 0; i < 2; ++i) {
                T c = *str++;
                uint8_t nibble;
                if (c >= zero && c <= nine)
                    nibble = uint8_t(c - zero);
                else if (c >= letter_a && c <= letter_f)
                    nibble = uint8_t(c - letter_a + 10);
                else if (c >= letter_A && c <= letter_F)
                    nibble = uint8_t(c - letter_A + 10);
                else 
                    return false;
                ret = (ret << 4) | nibble;
            }
            val = ret;
            return true;
        }

        template<impl::char_like T>
        static constexpr void write_hex(uint8_t val, T * str, format fmt) noexcept {
            using tr = impl::uuid_char_traits<T>;
            *str++ = tr::digits[fmt][uint8_t(val >> 4)];
            *str++ = tr::digits[fmt][uint8_t(val & 0x0F)];
        }

    public:
        std::array<uint8_t, 16> bytes{};

    public:
        ///Constructs a Nil UUID
        constexpr uuid() noexcept = default;

        ///Constructs uuid from a string literal
        template<impl::char_like T>
        consteval uuid(const T (&src)[37]) noexcept {
            using tr = impl::uuid_char_traits<T>;

            const T * str = src;
            uint8_t * data = this->bytes.data();
            for (int i = 0; i < 4; ++i, str += 2, ++data) {
                if (!read_hex(str, *data))
                    impl::invalid_constexpr_call("invalid uuid string");
            }
            if (*str++ != tr::dash)
                impl::invalid_constexpr_call("invalid uuid string");
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, str += 2, ++data) {
                    if (!read_hex(str, *data))
                        impl::invalid_constexpr_call("invalid uuid string");
                }
                if (*str++ != tr::dash)
                    impl::invalid_constexpr_call("invalid uuid string");
            }
            for (int i = 0; i < 6; ++i, str += 2, ++data) {
                if (!read_hex(str, *data))
                    impl::invalid_constexpr_call("invalid uuid string");
            }
            if (*str != 0)
                impl::invalid_constexpr_call("invalid uuid string");
        }

        /// Constructs uuid from a span of 16 byte-like objects
        template<impl::byte_like Byte>
        constexpr uuid(std::span<Byte, 16> src) noexcept {
            std::copy(src.begin(), src.end(), this->bytes.data());
        }

        /// Constructs uuid from anything convertible to a span of 16 byte-like objects
        template<class T>
        requires( !impl::is_span<T> && requires(const T & x) { 
            std::span{x}; 
            requires impl::byte_like<std::remove_reference_t<decltype(*std::span{x}.begin())>>; 
            requires decltype(std::span{x})::extent == 16;
        })
        constexpr uuid(const T & src) noexcept:
            uuid{std::span{src}}
        {}

        /// Constructs uuid from an old-style uuid_parts struct
        constexpr uuid(const uuid_parts & parts) noexcept {
            auto dest = this->bytes.data();
            dest = write_bytes(parts.time_low, dest);
            dest = write_bytes(parts.time_mid, dest);
            dest = write_bytes(parts.time_hi_and_version, dest);
            dest = write_bytes(parts.clock_seq, dest);
            std::copy(std::begin(parts.node), std::end(parts.node), dest);
        }

    #ifdef __APPLE__
        /// Constructs uuid from Apple's CFUUIDBytes
        constexpr uuid(const CFUUIDBytes & bytes) noexcept {
            static_assert(sizeof(this->bytes) == sizeof(bytes));
            std::copy(&bytes.byte0, &bytes.byte0 + sizeof(this->bytes), this->bytes.data());
        }

        /// Constructs uuid from Apple's CFUUIDRef
        uuid(CFUUIDRef obj) noexcept: uuid(CFUUIDGetUUIDBytes(obj)) 
        {}
    #endif

    #ifdef _WIN32
        /// Constructs uuid from Windows GUID
        constexpr uuid(const GUID & guid) noexcept {
            auto dest = this->bytes.data();
            dest = write_bytes(guid.Data1, dest);
            dest = write_bytes(guid.Data2, dest);
            dest = write_bytes(guid.Data3, dest);
            std::copy(std::begin(guid.Data4), std::end(guid.Data4), dest);
        }
    #endif

        /// Generates a version 1 UUID
        MUUID_EXPORTED static auto generate_time_based() -> uuid;
        /// Generates a version 3 UUID
        MUUID_EXPORTED static auto generate_md5(uuid ns, std::string_view name) noexcept -> uuid;
        /// Generates a version 4 UUID
        MUUID_EXPORTED static auto generate_random() noexcept -> uuid;
        /// Generates a version 5 UUID
        MUUID_EXPORTED static auto generate_sha1(uuid ns, std::string_view name) noexcept -> uuid;
        /// Generates a version 6 UUID
        MUUID_EXPORTED static auto generate_reordered_time_based() -> uuid;
        /// Generates a version 7 UUID
        MUUID_EXPORTED static auto generate_unix_time_based() -> uuid;

        /// Returns a Max UUID
        static constexpr uuid max() noexcept 
            { return uuid("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"); }

        /// Resets the object to a Nil UUID
        constexpr void clear() noexcept {
            *this = uuid();
        }

        /// Returns the UUID variant
        constexpr auto get_variant() const noexcept -> variant {
            uint8_t val = this->bytes[8];
            if ((val & 0x80) == 0)
                return variant::reserved_ncs;
            if ((val & 0x40) == 0)
                return variant::standard;
            if ((val & 0x20) == 0)
                return variant::reserved_microsoft;
            return variant::reserved_future;
        }

        /// Returns the UUID type
        /// Only meaningful if get_variant() returns variant::standard
        constexpr auto get_type() const noexcept -> type {
            return static_cast<type>(this->bytes[6] >> 4);
        }

        constexpr friend auto operator==(const uuid & lhs, const uuid & rhs) noexcept -> bool = default;
        constexpr friend auto operator<=>(const uuid & lhs, const uuid & rhs) noexcept -> std::strong_ordering = default;

        /// Converts uuid to an old-style uuid_parts struct
        constexpr auto to_parts() const noexcept -> uuid_parts {
            uuid_parts ret;
            auto ptr = this->bytes.data();
            ptr = read_bytes(ptr, ret.time_low);
            ptr = read_bytes(ptr, ret.time_mid);
            ptr = read_bytes(ptr, ret.time_hi_and_version);
            ptr = read_bytes(ptr, ret.clock_seq);
            std::copy(ptr, ptr + 6, ret.node);
            return ret;
        }

    #ifdef __APPLE__
        /// Converts uuid to Apple's CFUUIDBytes
        constexpr auto to_CFUUIDBytes() const noexcept -> CFUUIDBytes {
            static_assert(sizeof(this->bytes) == sizeof(bytes));

            CFUUIDBytes ret;
            std::copy(this->bytes.begin(), this->bytes.end(), &ret.byte0);
            return ret;
        }

        /// Converts uuid to Apple's CFUUIDRef
        auto to_CFUUID(CFAllocatorRef alloc=nullptr) const noexcept -> CFUUIDRef {
            static_assert(sizeof(this->bytes) == sizeof(CFUUIDBytes));
            return CFUUIDCreateFromUUIDBytes(alloc, *(CFUUIDBytes *)this->bytes.data());
        }
    #endif

    #ifdef _WIN32
        /// Converts uuid to Windows GUID
        constexpr auto to_GUID() const noexcept -> GUID {
            GUID ret;
            auto ptr = this->bytes.data();
            ptr = read_bytes(ptr, ret.Data1);
            ptr = read_bytes(ptr, ret.Data2);
            ptr = read_bytes(ptr, ret.Data3);
            std::copy(ptr, ptr + 8, ret.Data4);
            return ret;
        }
    #endif

        /// Parses uuid from a span of characters
        template<impl::char_like T, size_t Extent>
        static constexpr std::optional<uuid> from_chars(std::span<const T, Extent> src) noexcept {
            if (src.size() < 36)
                return std::nullopt;

            using tr = impl::uuid_char_traits<T>;
            
            uuid ret;
            const T * str = src.data();
            uint8_t * dest = ret.bytes.data();
            for(int i = 0; i < 4; ++i, str += 2, ++dest) {
                if (!read_hex(str, *dest))
                    return std::nullopt;
            }
            if (*str++ != tr::dash)
                return std::nullopt;
            for(int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, str += 2, ++dest) {
                    if (!read_hex(str, *dest))
                        return std::nullopt;
                }
                if (*str++ != tr::dash)
                    return std::nullopt;
            }
            for (int i = 0; i < 6; ++i, str += 2, ++dest) {
                if (!read_hex(str, *dest))
                    return std::nullopt;
            }
            return ret;
        }

        /// Parses uuid from anything convertible to a span of characters
        template<class T>
        requires( !impl::is_span<T> && requires(T & x) { 
            std::span{x}; 
            requires impl::char_like<std::remove_cvref_t<decltype(*std::span{x}.begin())>>; 
        })
        static constexpr auto from_chars(const T & src) noexcept
            { return from_chars(std::span{src}); }

        /// Formats uuid into a span of characters
        template<impl::char_like T, size_t Extent>
        [[nodiscard]]
        constexpr auto to_chars(std::span<T, Extent> dest, format fmt = lowercase) const noexcept ->
            std::conditional_t<Extent == std::dynamic_extent, bool, void> {
            
            if constexpr (Extent == std::dynamic_extent) {
                if (dest.size() < 36)
                    return false;
            } else {
                static_assert(Extent >= 36, "destination is too small");
            }

            using tr = impl::uuid_char_traits<T>;

            T * out = dest.data();
            const uint8_t * src = this->bytes.data();
            for (int i = 0; i < 4; ++i, ++src, out += 2)
                write_hex(*src, out, fmt);
            *out++ = tr::dash;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, ++src, out += 2) {
                    write_hex(*src, out, fmt);
                }
                *out++ = tr::dash;
            }
            for (int i = 0; i < 6; ++i, ++src, out += 2) 
                write_hex(*src, out, fmt);

            if constexpr (Extent == std::dynamic_extent)
                return true;
        }

        /// Formats uuid into anything convertible to a span of characters
        template<class T>
        requires( !impl::is_span<T> && requires(T & x) {
            std::span{x}; 
            requires impl::char_like<std::remove_reference_t<decltype(*std::span{x}.begin())>>;
            requires !std::is_const_v<std::remove_reference_t<decltype(*std::span{x}.begin())>>;
        })
        [[nodiscard]]
        constexpr auto to_chars(T & dest, format fmt = lowercase) const noexcept {
            return to_chars(std::span{dest}, fmt);
        }

        /// Returns a character array with formatted uuid
        template<impl::char_like T = char>
        constexpr auto to_chars(format fmt = lowercase) const noexcept -> std::array<T, 36> {
            std::array<T, 36> ret;
            to_chars(ret, fmt);
            return ret;
        }


        template<impl::char_like T = char>
    #if __cpp_lib_constexpr_string >= 201907L
        constexpr 
    #endif
        /// Returns a string with formatted uuid
        auto to_string(format fmt = lowercase) const -> std::basic_string<T>
        {
            std::basic_string<T> ret(36, T(0));
            (void)to_chars(ret, fmt);
            return ret;
        }

        /// Prints uuid into an ostream
        template<impl::char_like T>
        friend std::basic_ostream<T> & operator<<(std::basic_ostream<T> & str, const uuid val) {
            const auto flags = str.flags();
            const format fmt = (flags & std::ios_base::uppercase ? uppercase : lowercase);
            std::array<T, 36> buf;
            val.to_chars(buf, fmt);
            std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<T>(str));
            return str;
        }

        /// Reads uuid from an istream
        template<impl::char_like T>
        friend std::basic_istream<T> & operator>>(std::basic_istream<T> & str, uuid & val) {
            std::array<T, 36> buf;
            auto * strbuf = str.rdbuf();
            for(T & c: buf) {
                auto res = strbuf->sbumpc();
                if (res == std::char_traits<T>::eof()) {
                    str.setstate(std::ios_base::eofbit | std::ios_base::failbit);
                    return str;
                }
                c = T(res);
            }
            if (auto maybe_val = uuid::from_chars(buf))
                val = *maybe_val;
            else
                str.setstate(std::ios_base::failbit);
            return str;
        }

        /// Returns hash code for the uuid
        friend constexpr size_t hash_value(const uuid & val) noexcept {
            static_assert(sizeof(uuid) > sizeof(size_t) && sizeof(uuid) % sizeof(size_t) == 0);
            size_t temp;
            const uint8_t * data = val.bytes.data();
            size_t ret = 0;
            for(unsigned i = 0; i < sizeof(uuid) / sizeof(size_t); ++i) {
                memcpy(&temp, data, sizeof(size_t));
                ret = impl::hash_combine(ret, temp);
                data += sizeof(size_t);
            }
            return ret;
        }
    };

    static_assert(sizeof(uuid) == 16);

    /// Well-known namespaces for uuid::generate_md5() and uuid::generate_sha1()
    struct uuid::namespaces {
        /// Name string is a fully-qualified domain name
        static constexpr uuid dns{"6ba7b810-9dad-11d1-80b4-00c04fd430c8"};

        /// Name string is a URL 
        static constexpr uuid url{"6ba7b811-9dad-11d1-80b4-00c04fd430c8"};

        /// Name string is an ISO OID 
        static constexpr uuid oid{"6ba7b812-9dad-11d1-80b4-00c04fd430c8"};

        /// Name string is an X.500 DN (in DER or a text output format) 
        static constexpr uuid x500{"6ba7b814-9dad-11d1-80b4-00c04fd430c8"};

        namespaces() = delete;
        ~namespaces() = delete;
        namespaces(const namespaces &) = delete;
        namespaces & operator=(const namespaces &) = delete;
    };

    namespace impl {
        template<class Derived, class CharT>
        struct formatter_base
        {
            uuid::format fmt = uuid::lowercase;

            template<class ParseContext>
            constexpr auto parse(ParseContext & ctx) -> typename ParseContext::iterator
            {
                using tr = uuid_char_traits<CharT>;

                auto it = ctx.begin();
                while(it != ctx.end()) {
                    if (*it == tr::fmt_l) {
                        this->fmt = uuid::lowercase; ++it;
                    } else if (*it == tr::fmt_u) {
                        this->fmt = uuid::uppercase; ++it;
                    } else if (*it == tr::fmt_cl_br) {
                        break;
                    } else {
                        static_cast<Derived *>(this)->raise_exception("Invalid format args");
                    }
                }
                return it;
            }

            template <typename FormatContext>
            auto format(uuid val, FormatContext & ctx) const -> decltype(ctx.out()) 
            {
                std::array<CharT, 36> buf;
                val.to_chars(buf, this->fmt);
                return std::copy(buf.begin(), buf.end(), ctx.out());
            }
        };
    }
}

/// std::hash specialization for uuid
template<>
struct std::hash<muuid::uuid> {

    constexpr size_t operator()(const muuid::uuid & val) const noexcept {
        return hash_value(val);
    }
};


#if MUUID_SUPPORTS_STD_FORMAT

/// uuid formatter for std::format
template<class CharT>
struct std::formatter<::muuid::uuid, CharT> : public ::muuid::impl::formatter_base<std::formatter<::muuid::uuid, CharT>, CharT>
{
    [[noreturn]] void raise_exception(const char * message) {
        MUUID_THROW(std::format_error(message));
    }
};

#endif

#if MUUID_SUPPORTS_FMT_FORMAT

/// uuid formatter for fmt::format
template<class CharT>
struct fmt::formatter<::muuid::uuid, CharT> : public ::muuid::impl::formatter_base<fmt::formatter<::muuid::uuid, CharT>, CharT>
{
    void raise_exception(const char * message) {
        FMT_THROW(fmt::format_error(message));
    }
};

#endif

namespace muuid {

    /// How to generate node id for uuid::generate_time_based()
    enum class node_id {
        detect_system,
        generate_random
    };
    /**
     * Sets how to generate node id for uuid::generate_time_based()
     * 
     * This call affects all subsequent calls to those functions
     * 
     * @returns the generated node id. You can save it somewhere and then use the other overload of
     * set_node_id on subsequent runs to ensure one fixed node_id
     */
    MUUID_EXPORTED auto set_node_id(node_id type) -> std::span<const uint8_t, 6>;
    /** 
     * Sets a specific node id to use for uuid::generate_time_based()
     * 
     * This call affects all subsequent calls to those functions
     */
    MUUID_EXPORTED void set_node_id(std::span<const uint8_t, 6> id);

    /// Callback interface to handle persistence of clock data for all time based UUID generation
    class clock_persistence {
    public:
        /// Clock persistence data
        struct data {
            using time_point_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
            
            /**
             * The last known clock reading.
             *  
             * You can also use this value to optimize writing to persistent storage
             */
            time_point_t when; 
            /// Opaque value. Save/restore it but do not otherwise depend on its value
            uint16_t seq;
            /// Opaque value. Save/restore it but do not otherwise depend on its value
            int32_t adjustment;
        };

        /**
         * Per thread persistance callback.
         * 
         * All methods of this class are only accessed from a signle thread
         */
        class per_thread {
        public:
            /**
             * Called when this object is no longer used. 
             * 
             * It is safe to dispose of it (e.g. do `delete this` for example) when this is called
             */
            virtual void close() noexcept = 0;
    
            /// Lock access to persistent data against other threads/processes
            virtual void lock() = 0;
            /// Unlock access to persistent data against other threads/processes
            virtual void unlock() = 0;
    
            /**
             * Load persistent data if any
             * 
             * This is called once after this object is returned from get_for_current_thread().
             * The call happens inside a lock() call.
             * 
             * @returns `true` if data was loaded, false otherwise
             */
            virtual bool load(data & d) = 0;
            
            /**
             * Save persistent data if desired
             * 
             * This can be called multiple times (whenever UUID using the clock is generated).
             * The call happens inside a lock() call.
             */
            virtual void store(const data & d) = 0;
        protected:
            per_thread() noexcept = default;
            ~per_thread() noexcept = default;
            per_thread(const per_thread &) noexcept = default;
            per_thread & operator=(const per_thread &) noexcept = default;
        };
    public:
        /**
         * Return per_thread object for the calling thread.
         * 
         * This could be either a newly allocated object or some resused one
         * depending on your implementation.
         * 
         * The return time is reference rather than pointer because you are not
         * allowed to return nullptr.
         */
        virtual per_thread & get_for_current_thread() = 0;

        /**
         * Increment the reference count for this interface.
         * 
         * The object must remain alive as long as reference count is greater than 0
         */
        virtual void add_ref() noexcept = 0;
        /**
         * Decrement the reference count for this interface.
         * 
         * The object must remain alive as long as reference count is greater than 0
         */
        virtual void sub_ref() noexcept = 0;
    protected:
        clock_persistence() noexcept = default;
        ~clock_persistence() noexcept = default;
        clock_persistence(const clock_persistence &) noexcept = default;
        clock_persistence & operator=(const clock_persistence &) noexcept = default;
    };

    /**
     * Set the clock_persistence instance for uuid::generate_time_based()
     * 
     * Pass `nullptr` to remove.
     */
    MUUID_EXPORTED void set_time_based_persistence(clock_persistence * persistence);
    /**
     * Set the clock_persistence instance for uuid::generate_reordered_time_based()
     * 
     * Pass `nullptr` to remove.
     */
    MUUID_EXPORTED void set_reordered_time_based_persistence(clock_persistence * persistence);
    /**
     * Set the clock_persistence instance for uuid::generate_unix_time_based()
     * 
     * Pass `nullptr` to remove.
     */
    MUUID_EXPORTED void set_unix_time_based_persistence(clock_persistence * persistence);
}




#if defined(_MSC_VER) || (defined(_WIN32) && defined(__clang__))

    namespace muuid {
        /**
         * Obtain UUID associated with type
         * 
         * By default uses __uuidof operator. Can be overriden via
         * MUUID_ASSIGN_UUID macro
         */
        template<class T>
        constexpr uuid uuidof = __uuidof(T);
    }

#else

    namespace muuid {
        /**
        * Obtain UUID associated with type
        * 
        * UUIDs should be assigned via
        * MUUID_ASSIGN_UUID macro
        */
        template<class T>
        constexpr uuid uuidof;
    }
        
#endif

/**
 * Assign UUID to a type
 * 
 * This must be used in global namespace scope
 */
#define MUUID_ASSIGN_UUID(type, str) \
    template<> constexpr muuid::uuid muuid::uuidof<type>{str}


#endif
