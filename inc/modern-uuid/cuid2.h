// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_CUID2_H_INCLUDED
#define HEADER_MODERN_UUID_CUID2_H_INCLUDED

#include <modern-uuid/common.h>

namespace muuid {

    namespace impl {

    
    #if defined(__SIZEOF_INT128__) && !(defined(__clang__) && defined(_WIN32))
    #ifdef __GNUC__
        #define MUUID_CPP_EXTENSION __extension__
    #endif
        class cuid2_repr {
        public:
            constexpr cuid2_repr() = default;

            constexpr cuid2_repr(std::span<const uint8_t, 15> src) {
                for (auto b: src) {
                    m_data <<= 8;
                    m_data |= b;
                }
            }

            constexpr void get_bytes(std::span<uint8_t, 15> dst) const {
                MUUID_CPP_EXTENSION unsigned __int128 val = m_data;
                for (int i = 15; i != 0; --i) {
                    dst[i - 1] = val & 0xFF;
                    val >>= 8;
                }
            }

            constexpr void push_base36_digit(uint8_t val) {
                m_data *= 36;
                m_data += val;
            }

            constexpr uint8_t pop_base36_digit() {
                uint8_t ret = m_data % 36;
                m_data /= 36;
                return ret;
            }
        private:
            MUUID_CPP_EXTENSION unsigned __int128 m_data = 0;
        };

    #else

        class cuid2_repr {
        public:
            constexpr cuid2_repr() = default;

            constexpr cuid2_repr(std::span<const uint8_t, 15> src) {
                int i = 0;
                for ( ; i < 7; ++i) {
                    m_parts[0] <<= 8;
                    m_parts[0] |= src[i];
                }
                for ( ; i < 15; ++i) {
                    m_parts[1] <<= 8;
                    m_parts[1] |= src[i];
                }
            }

            constexpr void get_bytes(std::span<uint8_t, 15> dst) const {
                uint64_t val = m_parts[1];
                int i = 15;
                for ( ; i != 7; --i) {
                    dst[i - 1] = val & 0xFF;
                    val >>= 8;
                }
                val = m_parts[0];
                for ( ; i != 0; --i) {
                    dst[i - 1] = val & 0xFF;
                    val >>= 8;
                }
            }

            constexpr void push_base36_digit(uint8_t digit) {
                //x * 36 = x * 32 + x * 4 = x << 5 + x << 2
                uint32_t carry = digit;
                uint32_t first_overflow = 0;
                uint32_t second_overflow = 0;
                for(int i = 2; i != 0; --i) {
                    uint64_t first = m_parts[i - 1];
                    uint64_t second = first;
                    uint64_t a = (first << 5) | first_overflow;
                    first_overflow = first >> 59;
                    uint64_t b = (second << 2) | second_overflow;
                    second_overflow = second >> 62;
                    uint64_t val = a + b;
                    uint32_t c = (val < a);
                    val += carry;
                    c += (val < carry);
                    m_parts[i - 1] = val;
                    carry = c;
                }
            }

            constexpr uint8_t pop_base36_digit() {
                // (h*2^64 + l) / 36 = qh*2^64 + ql + qB*rh + (rB*rh + rl)/36
                // qB = 2^64 / 36
                // rB = 2^64 % 36

                constexpr uint64_t qB = 512409557603043100;
                constexpr uint32_t rB = 16;

                uint64_t high = m_parts[0];
                uint64_t low = m_parts[1];

                uint64_t qh = high / 36;
                uint32_t rh = uint32_t(high % 36);
                uint64_t ql = low / 36;
                uint32_t rl = uint32_t(low % 36);
                
                uint64_t qBrh = qB * rh;
                uint32_t tail = rB * rh + rl;
                
                uint32_t extra = tail / 36;
                uint8_t ret = tail % 36;

                uint64_t val = ql + qBrh;
                uint32_t carry = (val < ql);
                val += extra;
                carry += (val < extra);
                m_parts[1] = val;
                val = qh + carry;
                m_parts[0] = val;

                return ret;
            }
        private:
            std::array<uint64_t, 2> m_parts{};
        };

    #endif


        template<class T> struct cuid2_char_traits {
            static constexpr T l = T(u8'l');
            static constexpr T u = T(u8'u');
            static constexpr T cl_br = T(u8'}');
            static constexpr unsigned max = 128;
        };
        template<> struct cuid2_char_traits<char> {
            static constexpr char l = 'l';
            static constexpr char u = 'u';
            static constexpr char cl_br = '}';
            static constexpr unsigned max = ('a' == u8'a' ? 128 : 256);
        };
        template<> struct cuid2_char_traits<wchar_t> {
            static constexpr wchar_t l = L'l';
            static constexpr wchar_t u = L'u';
            static constexpr wchar_t cl_br = L'}';
            static constexpr unsigned max = (L'a' == u8'a' ? 128 : 256);
        };


        #define MUUID_CUID2_ALPHABET(...) \
                __VA_ARGS__##"0123456789abcdefghijklmnopqrstuvwxyz" \
                __VA_ARGS__##"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"


        template<class C, size_t N>
        consteval auto make_reverse_cuid2_alphabet(const C (&chars)[N]) {
            using tr = cuid2_char_traits<C>;

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

        class cuid2_alphabet {
        private:
            static constexpr const char narrow[] = MUUID_CUID2_ALPHABET();
            static constexpr const wchar_t wide[] = MUUID_CUID2_ALPHABET(L);
            static constexpr const char8_t utf[] = MUUID_CUID2_ALPHABET(u8);

            
            static constexpr auto reverse_narrow = make_reverse_cuid2_alphabet(narrow);
            static constexpr auto reverse_wide = make_reverse_cuid2_alphabet(wide);
            static constexpr auto reverse_utf = make_reverse_cuid2_alphabet(utf);

        public:
            static constexpr size_t size = (std::size(utf) - 1) / 2;
            
            public:
            template<impl::char_like C> 
            static constexpr C encode(bool uppercase, uint8_t idx) noexcept {
                auto real_idx = idx + (uppercase ? cuid2_alphabet::size : 0);

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

        #undef MUUID_CUID2_ALPHABET
    }

    class cuid2 {
    public:
        /// Whether to print cuid2 in lower or upper case
        enum format {
            lowercase,
            uppercase
        };

        static constexpr size_t char_length = 24;
    
    private:
        template<impl::char_like T>
        static constexpr bool read(const T * str, std::array<uint8_t, 16> & dest) noexcept {
            uint8_t first = impl::cuid2_alphabet::decode(str[0]);
            if (first < 10 || first >= impl::cuid2_alphabet::size)
                return false;
            first -= 10;
            impl::cuid2_repr repr;
            for(size_t i = 1; i < cuid2::char_length; ++i) {
                T c = str[i];
                uint8_t val = impl::cuid2_alphabet::decode(c);
                if (val >= impl::cuid2_alphabet::size)
                    return false;
                repr.push_base36_digit(val);
            }
            dest[0] = first;
            repr.get_bytes(std::span<uint8_t, 15>(&dest[1], 15));
            return true;
        }

        template<impl::char_like T>
        static constexpr void write(const std::array<uint8_t, 16> & src, T * str, format fmt) noexcept {
            uint8_t first = src[0] + 10;
            str[0] = impl::cuid2_alphabet::encode<T>(fmt, first);
            impl::cuid2_repr repr(std::span<const uint8_t, 15>(&src[1], 15));
            for(int i = cuid2::char_length; i != 1; --i) {
                uint8_t val = repr.pop_base36_digit();
                str[i - 1] = impl::cuid2_alphabet::encode<T>(fmt, val);
            }
        }
    public:
        std::array<uint8_t, 16> bytes{};

    public:
        ///Constructs a zeroed out cuid2
        constexpr cuid2() noexcept = default;

        ///Constructs cuid2 from a string literal
        template<impl::char_like T>
        consteval cuid2(const T (&src)[cuid2::char_length + 1]) noexcept {            
            if (!cuid2::read(src, this->bytes) || src[cuid2::char_length] != 0)
                impl::invalid_constexpr_call("invalid cuid2 string");
        }

        /// Generates a cuid2
        MUUID_EXPORTED static auto generate() -> cuid2;

        /// Returns a Max cuid2
        static constexpr cuid2 max() noexcept 
            { return cuid2("ZZZZZZZZZZZZZZZZZZZZZZZZ"); }

        /// Resets the object to a Nil cuid2
        constexpr void clear() noexcept {
            *this = cuid2();
        }

        constexpr friend auto operator==(const cuid2 & lhs, const cuid2 & rhs) noexcept -> bool = default;
        constexpr friend auto operator<=>(const cuid2 & lhs, const cuid2 & rhs) noexcept -> std::strong_ordering = default;

        /// Constructs cuid2 from a span of 16 byte-like objects
        template<impl::byte_like Byte>
        static constexpr std::optional<cuid2> from_bytes(std::span<Byte, 16> src) noexcept {

            if (src[0] > 25)
                return {};
            auto max_bytes = cuid2::max().bytes;
            if (std::lexicographical_compare(max_bytes.begin() + 1, max_bytes.end(),
                                             src.begin() + 1, src.end()))
                return {};

            cuid2 ret;
            std::copy(src.begin(), src.end(), ret.bytes.data());
            return ret;
        }

        /// Constructs cuid2 from anything convertible to a span of 16 byte-like objects
        template<class T>
        requires( !impl::is_span<T> && requires(const T & x) { 
            std::span{x}; 
            requires impl::byte_like<std::remove_reference_t<decltype(*std::span{x}.begin())>>; 
            requires decltype(std::span{x})::extent == 16;
        })
        static constexpr std::optional<cuid2> from_bytes(const T & src) noexcept {
            return cuid2::from_bytes(std::span{src});
        }

        /// Parses cuid2 from a span of characters
        template<impl::char_like T, size_t Extent>
        static constexpr std::optional<cuid2> from_chars(std::span<const T, Extent> src) noexcept {
            if (src.size() < cuid2::char_length)
                return std::nullopt;
            cuid2 ret;
            if (!cuid2::read(src.data(), ret.bytes))
                return std::nullopt;
            return ret;
        }

        /// Parses cuid2 from anything convertible to a span of characters
        template<class T>
        requires( !impl::is_span<T> && requires(T & x) { 
            std::span{x}; 
            requires impl::char_like<std::remove_cvref_t<decltype(*std::span{x}.begin())>>; 
        })
        static constexpr auto from_chars(const T & src) noexcept
            { return cuid2::from_chars(std::span{src}); }


        /// Formats cuid2 into a span of characters
        template<impl::char_like T, size_t Extent>
        [[nodiscard]]
        constexpr auto to_chars(std::span<T, Extent> dest, format fmt = cuid2::lowercase) const noexcept ->
            std::conditional_t<Extent == std::dynamic_extent, bool, void> {
            
            if constexpr (Extent == std::dynamic_extent) {
                if (dest.size() < cuid2::char_length)
                    return false;
            } else {
                static_assert(Extent >= cuid2::char_length, "destination is too small");
            }

            cuid2::write(this->bytes, dest.data(), fmt);

            if constexpr (Extent == std::dynamic_extent)
                return true;
        }

        /// Formats cuid2 into anything convertible to a span of characters
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

        /// Returns a character array with formatted cuid2
        template<impl::char_like T = char>
        constexpr auto to_chars(format fmt = lowercase) const noexcept -> std::array<T, cuid2::char_length> {
            std::array<T, cuid2::char_length> ret;
            this->to_chars(ret, fmt);
            return ret;
        }


        template<impl::char_like T = char>
    #if __cpp_lib_constexpr_string >= 201907L
        constexpr 
    #endif
        /// Returns a string with formatted cuid2
        auto to_string(format fmt = lowercase) const -> std::basic_string<T>
        {
            std::basic_string<T> ret(cuid2::char_length, T(0));
            (void)to_chars(ret, fmt);
            return ret;
        }

        /// Prints cuid2 into an ostream
        template<impl::char_like T>
        friend std::basic_ostream<T> & operator<<(std::basic_ostream<T> & str, const cuid2 & val) {
            const auto flags = str.flags();
            const cuid2::format fmt = (flags & std::ios_base::uppercase ? cuid2::uppercase : cuid2::lowercase);
            std::array<T, cuid2::char_length> buf;
            val.to_chars(buf, fmt);
            std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<T>(str));
            return str;
        }

        /// Reads cuid2 from an istream
        template<impl::char_like T>
        friend std::basic_istream<T> & operator>>(std::basic_istream<T> & str, cuid2 & val) {
            std::array<T, cuid2::char_length> buf;
            auto * strbuf = str.rdbuf();
            for(T & c: buf) {
                auto res = strbuf->sbumpc();
                if (res == std::char_traits<T>::eof()) {
                    str.setstate(std::ios_base::eofbit | std::ios_base::failbit);
                    return str;
                }
                c = T(res);
            }
            if (auto maybe_val = cuid2::from_chars(buf))
                val = *maybe_val;
            else
                str.setstate(std::ios_base::failbit);
            return str;
        }

        /// Returns hash code for the uuid
        friend constexpr size_t hash_value(const cuid2 & val) noexcept {
            static_assert(sizeof(cuid2) > sizeof(size_t) && sizeof(cuid2) % sizeof(size_t) == 0);
            size_t temp;
            const uint8_t * data = static_cast<const uint8_t *>(static_cast<const void *>(&val.bytes));
            size_t ret = 0;
            for(unsigned i = 0; i < sizeof(cuid2) / sizeof(size_t); ++i) {
                memcpy(&temp, data, sizeof(size_t));
                ret = impl::hash_combine(ret, temp);
                data += sizeof(size_t);
            }
            return ret;
        }
    };

    static_assert(sizeof(cuid2) == 16);

    namespace impl {
        template<class Derived, class CharT>
        struct cuid2_formatter_base
        {
            cuid2::format fmt = cuid2::lowercase;

            template<class ParseContext>
            constexpr auto parse(ParseContext & ctx) -> typename ParseContext::iterator
            {
                using tr = cuid2_char_traits<CharT>;

                auto it = ctx.begin();
                while(it != ctx.end()) {
                    if (*it == tr::l) {
                        this->fmt = cuid2::lowercase; ++it;
                    } else if (*it == tr::u) {
                        this->fmt = cuid2::uppercase; ++it;
                    } else if (*it == tr::cl_br) {
                        break;
                    } else {
                        static_cast<Derived *>(this)->raise_exception("Invalid format args");
                    }
                }
                return it;
            }

            template <typename FormatContext>
            auto format(cuid2 val, FormatContext & ctx) const -> decltype(ctx.out()) 
            {
                std::array<CharT, cuid2::char_length> buf;
                val.to_chars(buf, this->fmt);
                return std::copy(buf.begin(), buf.end(), ctx.out());
            }
        };
    }
}

/// std::hash specialization for cuid2
template<>
struct std::hash<muuid::cuid2> {

    constexpr size_t operator()(const muuid::cuid2 & val) const noexcept {
        return hash_value(val);
    }
};


#if MUUID_SUPPORTS_STD_FORMAT

/// cuid2 formatter for std::format
template<class CharT>
struct std::formatter<::muuid::cuid2, CharT> : 
    public ::muuid::impl::cuid2_formatter_base<std::formatter<::muuid::cuid2, CharT>, CharT>
{
    [[noreturn]] void raise_exception(const char * message) {
        MUUID_THROW(std::format_error(message));
    }
};

#endif

#if MUUID_SUPPORTS_FMT_FORMAT

/// cuid2 formatter for fmt::format
template<class CharT>
struct fmt::formatter<::muuid::cuid2, CharT> : 
    public ::muuid::impl::cuid2_formatter_base<fmt::formatter<::muuid::cuid2, CharT>, CharT>
{
    void raise_exception(const char * message) {
        FMT_THROW(fmt::format_error(message));
    }
};

#endif


#endif
