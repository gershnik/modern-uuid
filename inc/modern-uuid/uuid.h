// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_UUID_H_INCLUDED
#define HEADER_MODERN_UUID_UUID_H_INCLUDED

#include <modern-uuid/common.h>

#if defined(__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(_WIN32)
    #include <guiddef.h>
#endif

namespace muuid {
    namespace impl {

        template<char_like C> struct uuid_char_traits {
            static constexpr unsigned max = 128;

            static constexpr C l = C(u8'l');
            static constexpr C u = C(u8'u');
            static constexpr C cl_br = C(u8'}');
            static constexpr C dash = C(u8'-');
        };

        template<> struct uuid_char_traits<char> {
            static constexpr unsigned max = ('a' == u8'a' ? 128 : 256);

            static constexpr char l = 'l';
            static constexpr char u = 'u';
            static constexpr char cl_br = '}';
            static constexpr char dash = '-';
        };

        template<> struct uuid_char_traits<wchar_t> {
            static constexpr unsigned max = (L'a' == u8'a' ? 128 : 256);

            static constexpr wchar_t l = L'l';
            static constexpr wchar_t u = L'u';
            static constexpr wchar_t cl_br = L'}';
            static constexpr wchar_t dash = L'-';
        };

        #define MUUID_UUID_ALPHABET(...) \
                __VA_ARGS__##"0123456789abcdef" \
                __VA_ARGS__##"0123456789ABCDEF"

        template<class C, size_t N>
        static consteval auto make_reverse_uuid_alphabet(const C (&chars)[N]) {
            using tr = uuid_char_traits<C>;

            std::array<uint8_t, tr::max> ret;
            for (size_t i = 0; i < std::size(ret); ++i) {
                auto val = uint8_t(std::find(std::begin(chars), std::end(chars) - 1, C(i)) - std::begin(chars));
                if (val != N - 1) {
                    ret[i] = val % ((N - 1) / 2);
                } else {
                    ret[i] = (N - 1) / 2;
                }
            }
            return ret; 
        }

        class uuid_alphabet {
        private:
            static constexpr const char narrow[] = MUUID_UUID_ALPHABET();
            static constexpr const wchar_t wide[] = MUUID_UUID_ALPHABET(L);
            static constexpr const char8_t utf[] = MUUID_UUID_ALPHABET(u8);

            
            static constexpr auto reverse_narrow = make_reverse_uuid_alphabet(narrow);
            static constexpr auto reverse_wide = make_reverse_uuid_alphabet(wide);
            static constexpr auto reverse_utf = make_reverse_uuid_alphabet(utf);

        public:
            static constexpr size_t size = (std::size(utf) - 1) / 2;

        public:
            template<impl::char_like C> 
            static constexpr C encode(bool uppercase, uint8_t idx) noexcept {
                auto real_idx = idx + (uppercase << ct_log2<size>::value);

                if constexpr (std::is_same_v<C, char32_t> || 
                              std::is_same_v<C, char16_t> || 
                              std::is_same_v<C, char8_t> ||
                              (std::is_same_v<C, wchar_t> && L'a' == u8'a') ||
                              (std::is_same_v<C, wchar_t> && 'a' == u8'a')) {
                    return C(utf[real_idx]);
                } else if constexpr (std::is_same_v<C, wchar_t>) {
                    return wide[real_idx];
                } else {
                    return narrow[real_idx];
                }
            }
            
            template<impl::char_like C>
            static constexpr uint8_t decode(C c) noexcept {
                if constexpr (std::is_same_v<C, char32_t> || 
                              std::is_same_v<C, char16_t> || 
                              std::is_same_v<C, char8_t> ||
                              (std::is_same_v<C, wchar_t> && L'a' == u8'a') ||
                              (std::is_same_v<C, wchar_t> && 'a' == u8'a')) {
                
                    if (unsigned(c) >= std::size(reverse_utf))
                        return size;
                    return reverse_utf[unsigned(c)];
                
                } else if constexpr (std::is_same_v<C, wchar_t>) {

                    if (unsigned(c) >= std::size(reverse_wide))
                        return size;
                    return reverse_wide[unsigned(c)];

                } else {

                    if (unsigned(c) >= std::size(reverse_narrow))
                        return size;
                    return reverse_narrow[unsigned(c)];
                }
            }
        };

        #undef MUUID_UUID_ALPHABET
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
        template<impl::char_like T>
        static constexpr bool read_hex(const T * str, uint8_t & val) noexcept {
            uint8_t ret = 0;
            for (int i = 0; i < 2; ++i) {
                T c = *str++;
                uint8_t nibble = impl::uuid_alphabet::decode(c);
                if (nibble >= impl::uuid_alphabet::size)
                    return false;
                ret = (ret << 4) | nibble;
            }
            val = ret;
            return true;
        }

        template<impl::char_like T>
        static constexpr void write_hex(uint8_t val, T * str, format fmt) noexcept {
            *str++ = impl::uuid_alphabet::encode<T>(fmt, uint8_t(val >> 4));
            *str++ = impl::uuid_alphabet::encode<T>(fmt, uint8_t(val & 0x0F));
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
                if (!uuid::read_hex(str, *data))
                    impl::invalid_constexpr_call("invalid uuid string");
            }
            if (*str++ != tr::dash)
                impl::invalid_constexpr_call("invalid uuid string");
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, str += 2, ++data) {
                    if (!uuid::read_hex(str, *data))
                        impl::invalid_constexpr_call("invalid uuid string");
                }
                if (*str++ != tr::dash)
                    impl::invalid_constexpr_call("invalid uuid string");
            }
            for (int i = 0; i < 6; ++i, str += 2, ++data) {
                if (!uuid::read_hex(str, *data))
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
            dest = impl::write_bytes(parts.time_low, dest);
            dest = impl::write_bytes(parts.time_mid, dest);
            dest = impl::write_bytes(parts.time_hi_and_version, dest);
            dest = impl::write_bytes(parts.clock_seq, dest);
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
            dest = impl::write_bytes(guid.Data1, dest);
            dest = impl::write_bytes(guid.Data2, dest);
            dest = impl::write_bytes(guid.Data3, dest);
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
            ptr = impl::read_bytes(ptr, ret.time_low);
            ptr = impl::read_bytes(ptr, ret.time_mid);
            ptr = impl::read_bytes(ptr, ret.time_hi_and_version);
            ptr = impl::read_bytes(ptr, ret.clock_seq);
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
            ptr = impl::read_bytes(ptr, ret.Data1);
            ptr = impl::read_bytes(ptr, ret.Data2);
            ptr = impl::read_bytes(ptr, ret.Data3);
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
                if (!uuid::read_hex(str, *dest))
                    return std::nullopt;
            }
            if (*str++ != tr::dash)
                return std::nullopt;
            for(int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, str += 2, ++dest) {
                    if (!uuid::read_hex(str, *dest))
                        return std::nullopt;
                }
                if (*str++ != tr::dash)
                    return std::nullopt;
            }
            for (int i = 0; i < 6; ++i, str += 2, ++dest) {
                if (!uuid::read_hex(str, *dest))
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
            { return uuid::from_chars(std::span{src}); }

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
                uuid::write_hex(*src, out, fmt);
            *out++ = tr::dash;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 2; ++j, ++src, out += 2) {
                    uuid::write_hex(*src, out, fmt);
                }
                *out++ = tr::dash;
            }
            for (int i = 0; i < 6; ++i, ++src, out += 2) 
                uuid::write_hex(*src, out, fmt);

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
            return this->to_chars(std::span{dest}, fmt);
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
            const uuid::format fmt = (flags & std::ios_base::uppercase ? uuid::uppercase : uuid::lowercase);
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
                    if (*it == tr::l) {
                        this->fmt = uuid::lowercase; ++it;
                    } else if (*it == tr::u) {
                        this->fmt = uuid::uppercase; ++it;
                    } else if (*it == tr::cl_br) {
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


    /// Clock persistence data for UUID
    struct uuid_persistence_data {
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

    /// Callback interface to handle persistence of clock data for all time based ID generations
    using uuid_clock_persistence = generic_clock_persistence<uuid_persistence_data>;


    /**
     * Deprecated synonym for uuid_clock_persistence
     * 
     * It is a derived class rather than typedef for ABI compatibility
     */ 
    class [[deprecated("please use uuid_clock_persistence instead")]] clock_persistence : public uuid_clock_persistence {
    protected:
        clock_persistence() noexcept = default;
        ~clock_persistence() noexcept = default;
        clock_persistence(const clock_persistence &) noexcept = default;
        clock_persistence & operator=(const clock_persistence &) noexcept = default;
    };

    /**
     * Set the uuid_clock_persistence instance for uuid::generate_time_based()
     * 
     * Pass `nullptr` to remove.
     */
    MUUID_EXPORTED void set_time_based_persistence(uuid_clock_persistence * persistence);
    /**
     * Set the uuid_clock_persistence instance for uuid::generate_reordered_time_based()
     * 
     * Pass `nullptr` to remove.
     */
    MUUID_EXPORTED void set_reordered_time_based_persistence(uuid_clock_persistence * persistence);
    /**
     * Set the uuid_clock_persistence instance for uuid::generate_unix_time_based()
     * 
     * Pass `nullptr` to remove.
     */
    MUUID_EXPORTED void set_unix_time_based_persistence(uuid_clock_persistence * persistence);
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
