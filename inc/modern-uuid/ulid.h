// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_ULID_H_INCLUDED
#define HEADER_MODERN_UUID_ULID_H_INCLUDED

#include <modern-uuid/common.h>

namespace muuid {
    namespace impl {
        template<char_like C> struct ulid_char_traits {
            static constexpr unsigned max = 128;

            static constexpr C i = C(u8'i');
            static constexpr C I = C(u8'I');
            static constexpr C o = C(u8'o');
            static constexpr C O = C(u8'O');
            static constexpr C l = C(u8'l');
            static constexpr C L = C(u8'L');
            static constexpr C u = C(u8'u');
            static constexpr C cl_br = C(u8'}');
        };

        template<> struct ulid_char_traits<char> {
            static constexpr unsigned max = ('a' == u8'a' ? 128 : 256);

            static constexpr char i = 'i';
            static constexpr char I = 'I';
            static constexpr char o = 'o';
            static constexpr char O = 'O';
            static constexpr char l = 'l';
            static constexpr char L = 'L';
            static constexpr char u = 'u';
            static constexpr char cl_br = '}';
        };

        template<> struct ulid_char_traits<wchar_t> {
            static constexpr unsigned max = (L'a' == u8'a' ? 128 : 256);

            static constexpr wchar_t i = L'i';
            static constexpr wchar_t I = L'I';
            static constexpr wchar_t o = L'o';
            static constexpr wchar_t O = L'O';
            static constexpr wchar_t l = L'l';
            static constexpr wchar_t L = L'L';
            static constexpr wchar_t u = L'u';
            static constexpr wchar_t cl_br = L'}';
        };


        #define MUUID_ULID_ALPHABET(...) \
                __VA_ARGS__##"0123456789abcdefghjkmnpqrstvwxyz" \
                __VA_ARGS__##"0123456789ABCDEFGHJKMNPQRSTVWXYZ"

        template<class C, size_t N>
        static consteval auto make_reverse_ulid_alphabet(const C (&chars)[N]) {
            using tr = ulid_char_traits<C>;

            std::array<uint8_t, tr::max> ret;
            for (size_t i = 0; i < std::size(ret); ++i) {
                auto val = uint8_t(std::find(std::begin(chars), std::end(chars) - 1, C(i)) - std::begin(chars));
                if (val != N - 1) {
                    ret[i] = val % ((N - 1) / 2);
                } else {
                    ret[i] = (N - 1) / 2;
                }
            }
            ret[tr::i] = 1;
            ret[tr::I] = 1;
            ret[tr::l] = 1;
            ret[tr::L] = 1;
            ret[tr::o] = 0;
            ret[tr::O] = 0;
            return ret; 
        }

        class ulid_alphabet {
        private:
            static constexpr const char narrow[] = MUUID_ULID_ALPHABET();
            static constexpr const wchar_t wide[] = MUUID_ULID_ALPHABET(L);
            static constexpr const char8_t utf[] = MUUID_ULID_ALPHABET(u8);

            
            static constexpr auto reverse_narrow = make_reverse_ulid_alphabet(narrow);
            static constexpr auto reverse_wide = make_reverse_ulid_alphabet(wide);
            static constexpr auto reverse_utf = make_reverse_ulid_alphabet(utf);

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

        #undef MUUID_ULID_ALPHABET
    }

    class ulid {
    public:
        /// Whether to print ulid in lower or upper case
        enum format {
            lowercase,
            uppercase
        };

        /// Number of characters in string representation of ULID
        static constexpr size_t char_length = 26;
    private:
        template<impl::char_like T>
        static constexpr bool read(const T * str, std::span<uint8_t, 16> dest) noexcept {
            impl::bit_packer<5, 16> packer;
            uint8_t val = impl::ulid_alphabet::decode(str[0]);
            if (val > 7)
                return false;
            packer.push(val);
            for(size_t i = 1; i < ulid::char_length; ++i) {
                T c = str[i];
                val = impl::ulid_alphabet::decode(c);
                if (val >= impl::ulid_alphabet::size)
                    return false;
                packer.push(val);
            }
            packer.drain(dest);
            return true;
        }

        template<impl::char_like T>
        static constexpr void write(std::span<const uint8_t, 16> src, T * str, format fmt) noexcept {
            impl::bit_packer<5, 16> packer(src);
            for (size_t i = ulid::char_length; i != 0; --i) {
                uint8_t val = packer.pop();
                str[i - 1] = impl::ulid_alphabet::encode<T>(fmt, val);
            }
        }
    public:
        std::array<uint8_t, 16> bytes{};

    public:
        ///Constructs a zeroed out ULID
        constexpr ulid() noexcept = default;

        ///Constructs ulid from a string literal
        template<impl::char_like T>
        consteval ulid(const T (&src)[ulid::char_length + 1]) noexcept {            
            if (!ulid::read(src, this->bytes) || src[ulid::char_length] != 0)
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
            if (src.size() < ulid::char_length)
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
                if (dest.size() < ulid::char_length)
                    return false;
            } else {
                static_assert(Extent >= ulid::char_length, "destination is too small");
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
        constexpr auto to_chars(format fmt = lowercase) const noexcept -> std::array<T, ulid::char_length> {
            std::array<T, ulid::char_length> ret;
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
            std::basic_string<T> ret(ulid::char_length, T(0));
            (void)to_chars(ret, fmt);
            return ret;
        }

        /// Prints ulid into an ostream
        template<impl::char_like T>
        friend std::basic_ostream<T> & operator<<(std::basic_ostream<T> & str, const ulid val) {
            const auto flags = str.flags();
            const ulid::format fmt = (flags & std::ios_base::uppercase ? ulid::uppercase : ulid::lowercase);
            std::array<T, ulid::char_length> buf;
            val.to_chars(buf, fmt);
            std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<T>(str));
            return str;
        }

        /// Reads ulid from an istream
        template<impl::char_like T>
        friend std::basic_istream<T> & operator>>(std::basic_istream<T> & str, ulid & val) {
            std::array<T, ulid::char_length> buf;
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
                    if (*it == tr::l) {
                        this->fmt = ulid::lowercase; ++it;
                    } else if (*it == tr::u) {
                        this->fmt = ulid::uppercase; ++it;
                    } else if (*it == tr::cl_br) {
                        break;
                    } else {
                        static_cast<Derived *>(this)->raise_exception("Invalid format args");
                    }
                }
                return it;
            }

            template <typename FormatContext>
            auto format(ulid val, FormatContext & ctx) const -> decltype(ctx.out())  {
                std::array<CharT, ulid::char_length> buf;
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
