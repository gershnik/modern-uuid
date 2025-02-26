// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_UUID_H_INCLUDED
#define HEADER_MODERN_UUID_UUID_H_INCLUDED

// Config macros:
// MUUID_MULTITHREADED - auto-detected. Set to 1 to force multi-threaded build and 0 to force single threaded 1
// MUUID_SHARED - set to 1 when using/building a shared library version of the library
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
    #if (defined(__STDCPP_THREADS__) || defined(__MT__) || defined(_MT) || defined(_REENTRANT) \
        || defined(_PTHREADS) || defined(__APPLE__) || defined(__DragonFly__))
        
        #define MUUID_MULTITHREADED 1
    #else
        #define MUUID_MULTITHREADED 0
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

        static constexpr bool read_hex(const char * str, uint8_t & val) noexcept {
            uint8_t ret = 0;
            for (int i = 0; i < 2; ++i) {
                char c = *str++;
                uint8_t nibble;
                if (c >= '0' && c <= '9')
                    nibble = (c - '0');
                else if (c >= 'a' && c <= 'f')
                    nibble = (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    nibble = (c - 'A' + 10);
                else 
                    return false;
                ret = (ret << 4) | nibble;
            }
            val = ret;
            return true;
        }

        static constexpr void write_hex(uint8_t val, char * str, format fmt) noexcept {
            constexpr char digits[2][17] = {
                "0123456789abcdef",
                "0123456789ABCDEF",
            };
            *str++ = digits[fmt][uint8_t(val >> 4)];
            *str++ = digits[fmt][uint8_t(val & 0x0F)];
        }

    public:
        std::array<uint8_t, 16> bytes{};

    public:
        ///Constructs a Nil UUID
        constexpr uuid() noexcept = default;

        ///Constructs uuid from a string literal
        consteval uuid(const char (&src)[37]) noexcept {
            const char * str = src;
            uint8_t * data = this->bytes.data();
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

        /// Parses uuid from a span of characters
        template<size_t Extent>
        static constexpr std::optional<uuid> from_chars(std::span<const char, Extent> src) noexcept {
            if (src.size() < 36)
                return std::nullopt;
            
            uuid ret;
            const char * str = src.data();
            uint8_t * dest = ret.bytes.data();
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

        /// Parses uuid from anything convertible to a span of characters
        template<class T>
        requires( !impl::is_span<T> && requires(T & x) { 
            std::span{x}; 
            requires std::same_as<std::remove_cvref_t<decltype(*std::span{x}.begin())>, char>; 
        })
        static constexpr auto from_chars(const T & src) noexcept
            { return from_chars(std::span{src}); }

        /// Formats uuid into a span of characters
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
            const uint8_t * src = this->bytes.data();
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

        /// Formats uuid into anything convertible to a span of characters
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

        /// Returns a character array with formatted uuid
        constexpr auto to_chars(format fmt = lowercase) const noexcept -> std::array<char, 36> {
            std::array<char, 36> ret;
            to_chars(ret, fmt);
            return ret;
        }


    #if __cpp_lib_constexpr_string >= 201907L
        constexpr 
    #endif
        /// Returns a string with formatted uuid
        auto to_string(format fmt = lowercase) const -> std::string
        {
            std::string ret(36, '\0');
            (void)to_chars(ret, fmt);
            return ret;
        }

        /// Prints uuid into an ostream
        friend std::ostream & operator<<(std::ostream & str, const uuid val) {
            const auto flags = str.flags();
            const format fmt = (flags & std::ios_base::uppercase ? uppercase : lowercase);
            std::array<char, 36> buf;
            val.to_chars(buf, fmt);
            std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<char>(str));
            return str;
        }

        /// Reads uuid from an istream
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
        template<class Derived>
        struct formatter_base
        {
            uuid::format fmt = uuid::lowercase;

            template<class ParseContext>
            constexpr auto parse(ParseContext & ctx) -> typename ParseContext::iterator
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
                        static_cast<Derived *>(this)->raise_exception("Invalid format args");
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
template<>
struct std::formatter<::muuid::uuid> : public ::muuid::impl::formatter_base<std::formatter<::muuid::uuid>>
{
    void raise_exception(const char * message) {
        MUUID_THROW(std::format_error(message));
    }
};

#endif

#if defined(FMT_VERSION)

template<>
struct fmt::formatter<::muuid::uuid> : public ::muuid::impl::formatter_base<fmt::formatter<::muuid::uuid>>
{
    void raise_exception(const char * message) {
        FMT_THROW(fmt::format_error(message));
    }
};

#endif

namespace muuid {

    enum class node_id {
        detect_system,
        generate_random
    };
    MUUID_EXPORTED auto set_node_id(node_id type) -> std::span<const uint8_t, 6>;
    MUUID_EXPORTED void set_node_id(std::span<const uint8_t, 6> id);

    class clock_persistence {
    public:
        struct data {
            using time_point_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
            
            time_point_t when; 
            uint16_t seq;
            int32_t adjustment;
        };

        //all methods of this class are only accessed from a signle thread
        class per_thread {
        public:
            virtual void close() noexcept = 0;
    
            virtual void lock() = 0;
            virtual void unlock() = 0;
    
            //load and store are only called inside the lock
            //load is called once after this object is returned from get_for_current_thread
            //store may be called mutliple times
            virtual bool load(data & d) = 0;
            virtual void store(const data & d) = 0;
        protected:
            per_thread() noexcept = default;
            ~per_thread() noexcept = default;
            per_thread(const per_thread &) noexcept = default;
            per_thread & operator=(const per_thread &) noexcept = default;
        };
    public:
        virtual per_thread & get_for_current_thread() = 0;
    protected:
        clock_persistence() noexcept = default;
        ~clock_persistence() noexcept = default;
        clock_persistence(const clock_persistence &) noexcept = default;
        clock_persistence & operator=(const clock_persistence &) noexcept = default;
    };

    MUUID_EXPORTED void set_time_based_persistence(clock_persistence * persistence);
    MUUID_EXPORTED void set_reordered_time_based_persistence(clock_persistence * persistence);
    MUUID_EXPORTED void set_unix_time_based_persistence(clock_persistence * persistence);
}

#endif
