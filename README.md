## modern-uuid

[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-BSD-brightgreen.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Tests](https://github.com/gershnik/modern-uuid/actions/workflows/test.yml/badge.svg)](https://github.com/gershnik/modern-uuid/actions/workflows/test.yml)

A modern, no-dependencies, portable C++ library for manipulating UUIDs.

## Features

* Implements newer [RFC 9562](https://datatracker.ietf.org/doc/rfc9562/) (which supersedes older [RFC 4122](https://datatracker.ietf.org/doc/html/rfc4122)). Supports generation of UUID variants 1, 3, 5, 6 and 7.
* Self-contained with no dependencies beyond C++ standard library.
* Works on Mac, Linux, Windows, BSD, Wasm, and even Illumos. Might even work on some embedded systems given a suitable compiler and standard library support.
* Requires C++20 but does not require a very recent compiler (GCC is supported from version 10 and clang from version 13).
* Most operations (with an obvious exception of UUID generation and iostream I/O) are `constexpr` and can be done at compile time. Notably this enables:
  * Natural syntax for compile-time UUID literals
  * Using UUIDs as template parameters and in other compile-time contexts
* Supports `std::format` (if available) for formatting and parsing in addition to iostreams.
* Does not rely on C++ exceptions and can be used with C++ exceptions disabled.
* Uses "safe" constructs only in public interface (no raw pointers and such).

## Usage

A quick intro to the library is given below. For more details see [Usage Guide](/doc/Usage.md)

```cpp
#include <modern-uuid/uuid.h>

using namespace uuid;

//this is a compile time UUID literal
constexpr uuid u1("e53d37db-e4e0-484f-996f-3ab1d4701abc");

//default constructor creates Nil UUID 00000000-0000-0000-0000-000000000000
constexpr uuid nil_uuid;

//there is also uuid::max() to get Max UUID: FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF
constexpr uuid max_nil_uuid = uuid::max();

//if you want to you can use uuid as a template parameter
template<uuid U1> class some_class {...};
some_class<uuid("bc961bfb-b006-42f4-93ae-206f02658810")> some_object;

//you can generate all non-proprietary versions of UUID from RFC 9562:
uuid u_v1 = uuid::generate_time_based();
uuid u_v3 = uuid::generate_md5(uuid::namespaces::dns, "www.widgets.com");
uuid u_v4 = uuid::generate_random();
uuid u_v5 = uuid::generate_sha1(uuid::namespaces::dns, "www.widgets.com");
uuid u_v6 = uuid::generate_reordered_time_based();
uuid u_v7 = uuid::generate_unix_time_based();

//for non-literal strings you can parse uuids from strings using uuid::from_chars
//the argument to from_chars can be anything convertible to std::span<char, any extent>
//the call is constexpr
std::string some_uuid_str = "7D444840-9DC0-11D1-B245-5FFDCE74FAD2";
std::optional<uuid> maybe_uuid = uuid::from_chars(some_uuid_str);
if (maybe_uuid) {
    uuid parsed = *maybe_uuid;
}

//uuid objects can be compared in every possible way
assert(u_v1 > mil_uuid);
assert(u_v1 != u_v2);
std::strong_ordering res = (u_v6 <=> u_v7);
//etc.

//uuid objects can be hashed
std::unordered_map<uuid, transaction> transaction_map;

//they can be formatted

//this produces "e53d37db-e4e0-484f-996f-3ab1d4701abc"
std::string str = std::format("{}", u1);
//and this "E53D37DB-E4E0-484F-996F-3AB1D4701ABC"
str = std::format("{:u}", u1);
//and this "e53d37db-e4e0-484f-996f-3ab1d4701abc" again
str = std::format("{:l}", u1);

//uuids can be read/written from/to iostream (case doesn't matter)
std::istringstream istr("bc961bfb-b006-42f4-93ae-206f02658810");
uuid uuidr;
istr >> uuidr;
assert(uuidr = uuid("bc961bfb-b006-42f4-93ae-206f02658810"));

std::ostringstream ostr;
ostr1 << uuid("bc961bfb-b006-42f4-93ae-206f02658810");
assert(ostr.str() == "bc961bfb-b006-42f4-93ae-206f02658810");
ostr.str("");

//printing respects std::uppercase formatter
ostr << std::uppercase << uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2");
assert(ostr.str() == "7D444840-9DC0-11D1-B245-5FFDCE74FAD2");

//uuid objects can be created from raw bytes
//you need an std::span<anything byte-like, 16> or anything convertible to 
//such a span
std::array<std::byte, 16> arr1 = {...};
uuid u_from_std_array(arr1);

uint8_t arr2[16] = {...};
uuid u_from_c_array(arr2);

std::vector<uint8_t> vec = {...};
uuid u_from_bytes(std::span{vec}.subspan<3, 19>());

//finally you can access raw uuid bytes via bytes public member
constexpr uuid ua("7d444840-9dc0-11d1-b245-5ffdce74fad2");
assert(ua.bytes[3] == 0x48);

//bytes is an std::array<uint8_t, 16> so you can use all std::array
//functionality
for(auto b: ua.bytes) {
    ...use the byte...
}
```

## Building/Integrating

Quickest CMake method is given below. For more details and other method see [Integration Guide](doc/Building.md)

```cmake
include(FetchContent)
FetchContent_Declare(modern-uuid
    GIT_REPOSITORY git@github.com:gershnik/modern-uuid.git
    GIT_TAG        <desired tag like v0.1 or a sha>
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(modern-uuid)
...
target_link_libraries(mytarget
PRIVATE
  modern-uuid::modern-uuid
)
```

## Future work

The following functionality is currently not implemented but planned for future releases

* For UUID versions 1, 6 and 7 there is currently no way to persist the generation state. The state is initialized anew on each start.
  Persisting the state is recommended by RFC to ensure better global uniqueness.
* There is currently no way to create many UUIDs at once (batch creation). Due to the nature of time-based UUIDs creating a single UUID is 
  limited to the rate of at most 1 UUID per 0.1µs for versions 1 and 6 and to at most 1 UUID per 1µs for version 7.
* There are currently no helpers for easy conversion of various platform specific UUID representations (such as Windows `struct GUID`, Apple's 
  `struct CFUUIDBytes` etc.) to `muuid::uuid`


## Implementation details

There are many implementation choices for generating time-based UUIDs of versions 1, 6 and 7. This section documents some of them but these are
not contractual and can change in future releases

For UUID version 7 the `rand_a` field is used to store additional clock precision. This extends the precision of distinct representable times to 1µs. 

The actual granularity of the system clock is detected at runtime (unfortunately you cannot trust `time_point::period` on many systems). If the clock ticks slower than the maximum available precision for the desired UUID version then the unfilled rightmost decimal digits of the timestamp are filled in the following manner:
- Start with a random number `A` in the range `[0, max-1)` where `max` is the ratio of clock tick period to the desired precision (e.g. if the clock period is 1ms and desired precision is 1µs then `max` is 1,000). 
- For the first UUID generation for a given clock tick add `A` to the timestamp expressed in desired precision.
- For each subsequent generation within the same clock tick increment `A` by 1 until it reaches max. If it doesn't reach `max` add it to the timestamp.
- If it does reach `max`, wait until the clock changes. Then reset `A` to a new random number. 

Thus, if the clock ticks slower than the maximum available precision then, on average, you can generate at most 1 UUID per `2 * max precision` time units. For example if the clock ticks ever microsecond you can, on average, generate 1 UUID version 6 (precision 0.1µs) per 0.2µs. If the clocks ticks with same or greater precision then you can generate 1 UUID per max precision.

Currently there is no way to exceed these limit and no ability to have batch UUID generation. This might change in future releases.

The `clock_seq` field for UUIDs version 1 and 6 as well as the equivalent first 6 bits of `rand_b` field of UUID version 7 are used to handle system clock going backwards and/or distinguish between UUIDs generated by different processes. It is used in the following manner:
- On process startup (and upon `fork()` in child!) it is initialized to a random number
- If the system clock goes backwards from the last generation it is incremented by 1 modulo 2<sup>6</sup>

For time-based UUIDs (1, 6 and 7) in a multi-threaded environment there is a basic trade-off to be made between UUID monotonicity and speed/lack of contention. For version 1 monotonicity is not important and so `modern-uuid` allows each thread to generate them unsynchronized. Each thread gets a different `clock_seq` so the chance of collision is very low _as long as you don't run huge number of threads_. If you do it is advisable to use version 6 or 7 instead.

For versions 6 and 7 monotonicity is generally important - this is after all one of the main reasons for their existence. Thus `modern-uuid` synchronizes their generation between multiple threads. All threads have the same `clock_seq` and the values of UUIDs of each version are guaranteed to increase monotonically _as long as the system clock doesn't go back_. This of course increases thread contention.

Arguably these choices might be incorrect for a given application. Future versions might attempt to give the choice of what to prefer to the user.

Currently, if available, one of the system's network cards MAC addresses is used for UUIDs versions 1 and 6. If not available it is replaced by a random number (initialized once per process) as described in RFC 9562. If MAC address usage is not desired you should use UUID versions 7 or 4.
Future releases might add an option to always use random MAC.



