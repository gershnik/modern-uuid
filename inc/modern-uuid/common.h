// Copyright (c) 2024, Eugene Gershnik
// SPDX-License-Identifier: BSD-3-Clause

#ifndef HEADER_MODERN_UUID_COMMON_H_INCLUDED
#define HEADER_MODERN_UUID_COMMON_H_INCLUDED

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
        constexpr size_t hash_combine(S prev, S next) {
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

        template<impl::byte_like Byte, class T>
        constexpr const uint8_t * read_bytes(const Byte * bytes, T & val) noexcept {
            T tmp = uint8_t(*bytes++);
            for(unsigned i = 0; i < sizeof(T) - 1; ++i)
                tmp = (tmp << 8) | uint8_t(*bytes++);
            val = tmp;
            return bytes;
        }

        template<impl::byte_like Byte, class T>
        constexpr uint8_t * write_bytes(T val, Byte * bytes) noexcept {
            bytes[sizeof(T) - 1] = Byte(static_cast<uint8_t>(val));
            if constexpr (sizeof(T) > 1) {
                for(unsigned i = 1; i != sizeof(T); ++i) {
                    val >>= 8;
                    bytes[sizeof(T) - i - 1] = Byte(static_cast<uint8_t>(val));
                }
            }
            return bytes + sizeof(T);
        }
    }

    /// Callback interface to handle persistence of clock data
    template<class Data>
    class generic_clock_persistence {
    public:
        /// Clock persistence data
        using data = Data;

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
        generic_clock_persistence() noexcept = default;
        ~generic_clock_persistence() noexcept = default;
        generic_clock_persistence(const generic_clock_persistence &) noexcept = default;
        generic_clock_persistence & operator=(const generic_clock_persistence &) noexcept = default;
    };
}

#endif
