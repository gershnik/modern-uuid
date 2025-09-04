// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_NANOID_H_INCLUDED
#define HEADER_MODERN_UUID_NANOID_H_INCLUDED

#include <modern-uuid/common.h>
#include <modern-uuid/bit_packer.h>

namespace muuid {

    namespace impl {

        template<class T> struct nanoid_char_traits {
            static constexpr T fmt_cl_br = T(u8'}');
            static constexpr unsigned max = 128;
        };
        template<> struct nanoid_char_traits<char> {
            static constexpr char fmt_cl_br = '}';
            static constexpr unsigned max = ('a' == u8'a' ? 128 : 256);
        };
        template<> struct nanoid_char_traits<wchar_t> {
            static constexpr wchar_t fmt_cl_br = L'}';
            static constexpr unsigned max = (L'a' == u8'a' ? 128 : 256);
        };

        template<class C, size_t N>
        consteval bool valid_nanoid_alphabet(const C (&values)[N]) {
            return std::find_if(std::begin(values), std::end(values) - 1, [](auto c) {
                return unsigned(c) >= nanoid_char_traits<C>::max;
            }) == std::end(values) - 1;
        }

        template<class C, size_t N>
        consteval auto make_reverse_nanoid_alphabet(const C (&values)[N]) {
            std::array<uint8_t, nanoid_char_traits<C>::max> ret;
            for (size_t i = 0; i < std::size(ret); ++i) {
                auto val = uint8_t(std::find(std::begin(values), std::end(values) - 1, C(i)) - std::begin(values));
                ret[i] = val;
            }
            return ret; 
        }

        template<ct_string narrow, ct_string wide, ct_string utf>
        class nanoid_alphabet {
            static_assert(valid_nanoid_alphabet(utf.chars), "alphabet characters must be in Unicode range (0, 127]");
            static_assert(utf.size() > 1, "alphabet must contain at least 2 characters");
            static_assert(utf.size() <= 128, "alphabet must contain at most 128 characters");
            
            static_assert(utf.size() == narrow.size(), "multi-byte characters are not allowed");
            static_assert(utf.size() == wide.size(), "multi-wchar_t characters are not allowed");

            
            static_assert(valid_nanoid_alphabet(wide.chars), "wide alphabet characters are out of allowed range");
            static_assert(valid_nanoid_alphabet(narrow.chars), "alphabet characters are out of allowed range");

        private:
            static constexpr auto reverse_narrow = make_reverse_nanoid_alphabet(narrow.chars);
            static constexpr auto reverse_wide = make_reverse_nanoid_alphabet(wide.chars);
            static constexpr auto reverse_utf = make_reverse_nanoid_alphabet(utf.chars);

        public:
            static constexpr size_t size = utf.size();
            static constexpr bool is_full = size_t(size_t(1) << ct_log2<size>::value) == size;
            static constexpr size_t bits_per_char = ct_log2<size>::value + !is_full;
            

        public:
            template<impl::char_like C> 
            static constexpr C encode(uint8_t idx) noexcept {
                if constexpr (std::is_same_v<C, char32_t> || 
                              std::is_same_v<C, char16_t> || 
                              std::is_same_v<C, char8_t> ||
                              (std::is_same_v<C, wchar_t> && L'a' == u8'a') ||
                              (std::is_same_v<C, wchar_t> && 'a' == u8'a')) {
                    return C(utf.chars[idx]);
                } else if constexpr (std::is_same_v<C, wchar_t>) {
                    return wide.chars[idx];
                } else {
                    return narrow.chars[idx];
                }
            }
            
            template<impl::char_like C>
            static constexpr uint8_t decode(C c) noexcept {
                if constexpr (std::is_same_v<C, char32_t> || 
                              std::is_same_v<C, char16_t> || 
                              std::is_same_v<C, char8_t> ||
                              (std::is_same_v<C, wchar_t> && L'a' == u8'a') ||
                              (std::is_same_v<C, wchar_t> && 'a' == u8'a')) {
                
                    if (unsigned(c) >= std::size(nanoid_alphabet::reverse_utf))
                        return size;
                    return nanoid_alphabet::reverse_utf[unsigned(c)];
                
                } else if constexpr (std::is_same_v<C, wchar_t>) {

                    if (unsigned(c) >= std::size(nanoid_alphabet::reverse_wide))
                        return size;
                    return nanoid_alphabet::reverse_wide[unsigned(c)];

                } else {

                    if (unsigned(c) >= std::size(nanoid_alphabet::reverse_narrow))
                        return size;
                    return nanoid_alphabet::reverse_narrow[unsigned(c)];

                }
            }
        };

        #define MUUID_DECLARE_NANOID_ALPHABET(name, str) struct name : ::muuid::impl::nanoid_alphabet<str, L##str, u8##str> {}

        MUUID_EXPORTED void generate_nanoid(std::span<uint8_t> dest, uint8_t max);
    }
    

    MUUID_DECLARE_NANOID_ALPHABET(nanoid_default_alphabet, "useandom-26T198340PX75pxJACKVERYMINDBUSHWOLF_GQZbfghjklqvwyzrict");

    static_assert(nanoid_default_alphabet::bits_per_char == 6);

    template<class Alphabet, size_t CharCount>
    class basic_nanoid {
    private:
        static constexpr size_t bits_in_string = Alphabet::bits_per_char * CharCount;
        static constexpr size_t bytes_count = bits_in_string / 8 + (bits_in_string % 8 != 0);
        static constexpr size_t bits_count = bytes_count * 8;
        static constexpr size_t unpack_buf_size = bits_count / Alphabet::bits_per_char + (bits_count % Alphabet::bits_per_char != 0);


        template<impl::char_like T>
        static constexpr bool read(const T * str, std::span<uint8_t, bytes_count> dest) noexcept {
            uint8_t buf[unpack_buf_size];
            constexpr size_t padding = unpack_buf_size - CharCount;
            
            size_t i = 0; 
            for(; i < padding; ++i)
                buf[i] = 0;

            for(; i < unpack_buf_size; ++i) {
                auto c = str[i - padding];
                uint8_t val = Alphabet::decode(c);
                if (val >= Alphabet::size)
                    return false;
                buf[i] = val;
            }
            impl::bit_packer<Alphabet::bits_per_char, bytes_count>::pack_bits(std::span(buf), dest);
            return true;
        }

        template<impl::char_like T>
        static constexpr void write(std::span<const uint8_t, bytes_count> src, T * str) noexcept {
            uint8_t buf[unpack_buf_size];
            constexpr size_t padding = unpack_buf_size - CharCount;

            impl::bit_packer<Alphabet::bits_per_char, bytes_count>::unpack_bits(src, std::span(buf));
            for(size_t i = padding; i < std::size(buf); ++i) {
                *str++ = Alphabet::template encode<T>(buf[i]);
            }
        }

        static constexpr void sanitize_first_byte(std::array<uint8_t, bytes_count> & bytes) {
            if constexpr (basic_nanoid::bits_in_string <  basic_nanoid::bits_count) {
                constexpr uint8_t mask = uint8_t((1u << (basic_nanoid::bits_in_string % 8)) - 1);
                bytes[0] &= mask;
            }
        }

    public:
        std::array<uint8_t, bytes_count> bytes{};

    public:
        ///Constructs a zeroed out nanoid
        constexpr basic_nanoid() noexcept = default;

        ///Constructs nanoid from a string literal
        template<impl::char_like T>
        consteval basic_nanoid(const T (&src)[CharCount + 1]) noexcept {            
            if (!basic_nanoid::read(src, this->bytes) || src[CharCount] != 0)
                impl::invalid_constexpr_call("invalid nanoid string");
        }

        /// Generates a nanoid
        static auto generate() -> basic_nanoid {
            std::array<uint8_t, bytes_count> buf;
            if constexpr (Alphabet::is_full) {
                impl::generate_nanoid(buf, 255);
                basic_nanoid::sanitize_first_byte(buf);
            } else {
                uint8_t unpacked[unpack_buf_size];
                std::span<uint8_t> unpacked_span(unpacked);
                constexpr size_t padding = unpack_buf_size - CharCount;
                
                for(size_t i = 0; i < padding; ++i)
                    unpacked_span[i] = 0;
                
                unpacked_span = unpacked_span.subspan(padding);
                impl::generate_nanoid(unpacked_span, Alphabet::size - 1);
                impl::bit_packer<Alphabet::bits_per_char, bytes_count>::pack_bits(std::span(unpacked), buf);
            }
            return *reinterpret_cast<basic_nanoid *>(&buf);
        }

        /// Returns a Max nanoid
        static constexpr basic_nanoid max() noexcept {
            constexpr impl::ct_string<char, CharCount + 1> ctstr(Alphabet::template encode<char>(Alphabet::size - 1));
            return basic_nanoid(ctstr.chars);
        }

        /// Resets the object to a Nil nanoid
        constexpr void clear() noexcept {
            *this = basic_nanoid();
        }

        constexpr friend auto operator==(const basic_nanoid & lhs, const basic_nanoid & rhs) noexcept -> bool = default;
        constexpr friend auto operator<=>(const basic_nanoid & lhs, const basic_nanoid & rhs) noexcept -> std::strong_ordering = default;


        /// Constructs nanoid from a span of bytes_count byte-like objects
        template<impl::byte_like Byte>
        static constexpr std::optional<basic_nanoid> from_bytes(std::span<Byte, bytes_count> src) noexcept {

            basic_nanoid ret;
            std::copy(src.begin(), src.end(), ret.bytes.data());

            if constexpr (!Alphabet::is_full) {
                uint8_t buf[unpack_buf_size];
                constexpr size_t padding = unpack_buf_size - CharCount;

                impl::bit_packer<Alphabet::bits_per_char, bytes_count>::unpack_bits(ret.bytes, std::span(buf));
                for(size_t i = padding; i < std::size(buf); ++i) {
                    if (buf[i] >= Alphabet::size)
                        return {};
                }

            } else if constexpr (basic_nanoid::bits_in_string <  basic_nanoid::bits_count) {

                constexpr uint8_t mask = uint8_t((1u << (basic_nanoid::bits_in_string % 8)) - 1);
                if (ret.bytes[0] > mask)
                    return {};
            }
            return ret;
        }

        /// Constructs nanoid from anything convertible to a span of bytes_count byte-like objects
        template<class T>
        requires( !impl::is_span<T> && requires(const T & x) { 
            std::span{x}; 
            requires impl::byte_like<std::remove_reference_t<decltype(*std::span{x}.begin())>>; 
            requires decltype(std::span{x})::extent == bytes_count;
        })
        static constexpr std::optional<basic_nanoid> from_bytes(const T & src) noexcept {
            return from_bytes(std::span{src});
        }

        /// Parses nanoid from a span of characters
        template<impl::char_like T, size_t Extent>
        static constexpr std::optional<basic_nanoid> from_chars(std::span<const T, Extent> src) noexcept {
            if (src.size() < CharCount)
                return std::nullopt;
            basic_nanoid ret;
            if (!basic_nanoid::read(src.data(), ret.bytes))
                return std::nullopt;
            return ret;
        }

        /// Parses nanoid from anything convertible to a span of characters
        template<class T>
        requires( !impl::is_span<T> && requires(T & x) { 
            std::span{x}; 
            requires impl::char_like<std::remove_cvref_t<decltype(*std::span{x}.begin())>>; 
        })
        static constexpr auto from_chars(const T & src) noexcept
            { return basic_nanoid::from_chars(std::span{src}); }


        /// Formats nanoid into a span of characters
        template<impl::char_like T, size_t Extent>
        [[nodiscard]]
        constexpr auto to_chars(std::span<T, Extent> dest) const noexcept ->
            std::conditional_t<Extent == std::dynamic_extent, bool, void> {
            
            if constexpr (Extent == std::dynamic_extent) {
                if (dest.size() < CharCount)
                    return false;
            } else {
                static_assert(Extent >= CharCount, "destination is too small");
            }

            basic_nanoid::write(this->bytes, dest.data());

            if constexpr (Extent == std::dynamic_extent)
                return true;
        }

        /// Formats nanoid into anything convertible to a span of characters
        template<class T>
        requires( !impl::is_span<T> && requires(T & x) {
            std::span{x}; 
            requires impl::char_like<std::remove_reference_t<decltype(*std::span{x}.begin())>>;
            requires !std::is_const_v<std::remove_reference_t<decltype(*std::span{x}.begin())>>;
        })
        [[nodiscard]]
        constexpr auto to_chars(T & dest) const noexcept {
            return this->to_chars(std::span{dest});
        }

        /// Returns a character array with formatted nanoid
        template<impl::char_like T = char>
        constexpr auto to_chars() const noexcept -> std::array<T, CharCount> {
            std::array<T, CharCount> ret;
            this->to_chars(ret);
            return ret;
        }


        template<impl::char_like T = char>
    #if __cpp_lib_constexpr_string >= 201907L
        constexpr 
    #endif
        /// Returns a string with formatted nanoid
        auto to_string() const -> std::basic_string<T>
        {
            std::basic_string<T> ret(CharCount, T(0));
            (void)to_chars(ret);
            return ret;
        }

        /// Prints nanoid into an ostream
        template<impl::char_like T>
        friend std::basic_ostream<T> & operator<<(std::basic_ostream<T> & str, const basic_nanoid val) {
            std::array<T, CharCount> buf;
            val.to_chars(buf);
            std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<T>(str));
            return str;
        }

        /// Reads nanoid from an istream
        template<impl::char_like T>
        friend std::basic_istream<T> & operator>>(std::basic_istream<T> & str, basic_nanoid & val) {
            std::array<T, CharCount> buf;
            auto * strbuf = str.rdbuf();
            for(T & c: buf) {
                auto res = strbuf->sbumpc();
                if (res == std::char_traits<T>::eof()) {
                    str.setstate(std::ios_base::eofbit | std::ios_base::failbit);
                    return str;
                }
                c = T(res);
            }
            if (auto maybe_val = basic_nanoid::from_chars(buf))
                val = *maybe_val;
            else
                str.setstate(std::ios_base::failbit);
            return str;
        }

        /// Returns hash code for the nanoid
        friend constexpr size_t hash_value(const basic_nanoid & val) noexcept {
            static_assert(sizeof(basic_nanoid) > sizeof(size_t) && sizeof(basic_nanoid) % sizeof(size_t) == 0);
            size_t temp;
            const uint8_t * data = val.bytes.data();
            size_t ret = 0;

            if constexpr (constexpr auto remainder = sizeof(val.bytes) % sizeof(size_t)) {
                temp = 0;
                memcpy(&temp, data, remainder);
                ret = impl::hash_combine(ret, temp);
                data += remainder;
            }
            for(unsigned i = 0; i < sizeof(basic_nanoid) / sizeof(size_t); ++i) {
                memcpy(&temp, data, sizeof(size_t));
                ret = impl::hash_combine(ret, temp);
                data += sizeof(size_t);
            }
            return ret;
        }
    };

    using nanoid = basic_nanoid<nanoid_default_alphabet, 21>;

    static_assert(sizeof(nanoid) == 16);

    namespace impl {
        template<class Derived, class Alphabet, size_t CharCount, class CharT>
        struct nanoid_formatter_base {
            template<class ParseContext>
            constexpr auto parse(ParseContext & ctx) -> typename ParseContext::iterator {
                using tr = nanoid_char_traits<CharT>;

                auto it = ctx.begin();
                while(it != ctx.end()) {
                    if (*it == tr::fmt_cl_br) {
                        break;
                    } else {
                        static_cast<Derived *>(this)->raise_exception("Invalid format args");
                    }
                }
                return it;
            }

            template <typename FormatContext>
            auto format(const basic_nanoid<Alphabet, CharCount> & val, FormatContext & ctx) const -> decltype(ctx.out())  {
                std::array<CharT, CharCount> buf;
                val.to_chars(buf);
                return std::copy(buf.begin(), buf.end(), ctx.out());
            }
        };
    }
}

/// std::hash specialization for nanoid
template<class Alphabet, size_t CharCount>
struct std::hash<muuid::basic_nanoid<Alphabet, CharCount>> {

    constexpr size_t operator()(const muuid::basic_nanoid<Alphabet, CharCount> & val) const noexcept {
        return hash_value(val);
    }
};


#if MUUID_SUPPORTS_STD_FORMAT

/// nanoid formatter for std::format
template<class Alphabet, size_t CharCount, class CharT>
struct std::formatter<::muuid::basic_nanoid<Alphabet, CharCount>, CharT> : 
    public ::muuid::impl::nanoid_formatter_base<std::formatter<::muuid::basic_nanoid<Alphabet, CharCount>, CharT>, Alphabet, CharCount, CharT>
{
    [[noreturn]] void raise_exception(const char * message) {
        MUUID_THROW(std::format_error(message));
    }
};

#endif

#if MUUID_SUPPORTS_FMT_FORMAT

/// nanoid formatter for fmt::format
template<class Alphabet, size_t CharCount, class CharT>
struct fmt::formatter<::muuid::basic_nanoid<Alphabet, CharCount>, CharT> : 
    public ::muuid::impl::nanoid_formatter_base<fmt::formatter<::muuid::basic_nanoid<Alphabet, CharCount>, CharT>, Alphabet, CharCount, CharT>
{
    void raise_exception(const char * message) {
        FMT_THROW(fmt::format_error(message));
    }
};

#endif


#endif 
