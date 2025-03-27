## modern-uuid

[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-BSD-brightgreen.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Tests](https://github.com/gershnik/modern-uuid/actions/workflows/test.yml/badge.svg)](https://github.com/gershnik/modern-uuid/actions/workflows/test.yml)

A modern, no-dependencies, portable C++ library for manipulating UUIDs.

## Features

* Implements newer [RFC 9562](https://www.rfc-editor.org/rfc/rfc9562.html) (which supersedes older [RFC 4122](https://www.rfc-editor.org/rfc/rfc4122.html)). Supports generation of UUID variants 1, 3, 5, 6 and 7.
* Self-contained with no dependencies beyond C++ standard library.
* Works on Mac, Linux, Windows, BSD, Wasm, and even Illumos. Might even work on some embedded systems given a suitable compiler and standard library support.
* Requires C++20 but does not require a very recent compiler (GCC is supported from version 10 and clang from version 13).
* Most operations (with an obvious exception of UUID generation and iostream I/O) are `constexpr` and can be done at compile time. Notably this enables:
  * Natural syntax for compile-time UUID literals
  * Using UUIDs as template parameters and in other compile-time contexts
* Supports `std::format` (if available) for formatting and parsing in addition to iostreams.
* Does not rely on C++ exceptions and can be used with C++ exceptions disabled.
* Uses "safe" constructs only in public interface (no raw pointers and such).
* Properly handles `fork` with no `exec` on Unix systems. UUIDs generated by the child process will not collide with parent's.

See also [differences from other libraries](#differences-from-other-libraries) below.

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

//they can be formatted. u and l stand for uppercase and lowercase

std::string str = std::format("{}", u1);
assert(str == "e53d37db-e4e0-484f-996f-3ab1d4701abc");

str = std::format("{:u}", u1);
assert(str == "E53D37DB-E4E0-484F-996F-3AB1D4701ABC")

str = std::format("{:l}", u1);
assert(str == "e53d37db-e4e0-484f-996f-3ab1d4701abc")

//uuids can be read/written from/to iostream 

//when reading case doesn't matter
std::istringstream istr("bc961bfb-b006-42f4-93ae-206f02658810");
uuid uuidr;
istr >> uuidr;
assert(uuidr = uuid("bc961bfb-b006-42f4-93ae-206f02658810"));

std::ostringstream ostr;
ostr << uuid("bc961bfb-b006-42f4-93ae-206f02658810");
assert(ostr.str() == "bc961bfb-b006-42f4-93ae-206f02658810");
ostr.str("");

//writing respects std::ios_base::uppercase stream flag
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
    GIT_TAG        <desired tag like v1.2 or a sha>
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(modern-uuid)
...
target_link_libraries(mytarget
PRIVATE
  modern-uuid::modern-uuid
)
```

## Differences from other libraries

There are two well-known libraries commonly used to handle UUIDs: `libuuid` from [util-linux](https://github.com/util-linux/util-linux) and
[Boost.Uuid](http://boost.org/libs/uuid). Both are very good libraries, but have, <ins>**at the time of this writing**</ins> (03-2025), limitations 
and/or trade-offs that I found inconvenient or annoying and which `modern-uuid` was created to address. In particular:

### libuuid

Portability. `libuuid` is only really portable to Linux and BSD. It doesn't work on Windows. It hasn't even been [working on Mac](https://github.com/util-linux/util-linux/issues/2873) for almost a year now without patches. That's two major platforms out there. 

Second, while its time-based generation algorithms are very robust and correct they assume that the system clock ticks once a microsecond.
This assumption is not necessarily true. For example on WASM the clock ticks with a millisecond precision. This results in UUID generation 
being very slow on that platform and the results very predictable.

Lastly, `libuuid` is a C library. It doesn't really help C++ code to actually manipulate UUIDs as first class objects. For that one 
needs to either write a custom UUID class or use a different library.

### Boost.Uuid

Boost.Uuid makes two design trade-offs that might not be the right ones for many users

It prioritizes speed above RFC compliance and, in some cases, correctness. For example it allows the sequential UUID counters to wrap around without
waiting for the clock to change. This makes UUID v7 (and possibly v6 too) non always monotonically increasing on platforms with a slower clock 
(e.g. WASM again). The ways it populates v7 fields also contradicts RFC recommendations. Whether this results in some increased guessability or not is hard to tell. On the flip side this results in much faster UUID generation so YMMV.

It pushes management of UUID generator objects to the library user. Unfortunately, managing the generators is not a trivial task for most of them. You need to be aware of various intricacies that a casual user without deep understanding of how UUIDs work will likely miss. For example, you need to be aware that you must reset any inherited parent process generator in a forked child process or risk duplicate UUIDs being generated.

This approach makes the library simpler and synthetic benchmarks faster. But it also makes it easy for the user to misuse the library and the speed gains would be negated by external generator management anyway.

Boost.Uuid is a header only library, which is great, but to actually use it you still need to get the entire 130+MB Boost-zilla download. If your
project already contains Boost this is not a problem. But, for things that don't, it is a big annoyance.

`modern-uuid` tries to address these perceived shortcomings:

- It strives to be widely portable to any reasonable system out there.
- It does not require you to know how to manage generators. Just call generate_xxx and it will do the right thing.
- It handles slow clocks correctly (hopefully)
- It is standalone with no dependencies. 

On the negative side:
- It is slower than Boost.Uuid for UUID generation (but is faster than `libuuid`!).
- It currently is not header-only. (This might, or might not, be addressed in future releases)


## Implementation details

There are many implementation choices for generating time-based UUIDs of versions 1, 6 and 7. This section documents some of them but these are
not contractual and can change in future releases

For UUID version 7 the `rand_a` field is used to store additional clock precision using Method 3 of the [section 6.2](https://www.rfc-editor.org/rfc/rfc9562.html#name-monotonicity-and-counters) of the RFC. This extends the precision of distinct representable times to 1µs. The first 14 bit of `rand_b` field are filled with a randomly seeded counter using Method 1 of the same section.

For UUID version 6 the `node` field is populated with a random value on each generation. The `clock_seq` field is filled with a randomly seeded counter using Method 1 of the [section 6.2](https://www.rfc-editor.org/rfc/rfc9562.html#name-monotonicity-and-counters) of the RFC. 

The actual granularity of the system clock is detected at runtime (unfortunately you cannot trust `time_point::period` on many systems). If the clock ticks slower than the maximum available precision for the desired UUID version then the unfilled rightmost decimal digits of the timestamp are filled by incrementing a counter for each generation. The counter is in the range `[0, max-1)` where `max` is the ratio of clock tick period to the desired precision (e.g. if the clock period is 1ms and desired precision is 1µs then `max` is 1,000). 
When the counter reaches `max`:
- for version 1 the generation waits until the clock changes
- for versions 6 and 7 the `clock_seq`/`rand_b` field is used to provide further monotonicity as described above. When the these counters are exhausted the generation also waits for a clock change.

If the system clock goes backwards:
- for version 1 the `clock_seq` is incremented by 1 modulo 2<sup>14</sup>
- for versions 6 and 7 the monotonicity has been lost - there is nothing that can be done about it - so the `clock_seq`/first 14 bits of `rand_b` are initialized to a random number.

On Unix-like systems upon `fork()` without `exec()` in the child process all the "static" state
for all generators is reinitialized anew as-if on a new process start. Specifically this affects:
- random number generator(s)
- all `clock_seq`/first 14 bits of `rand_b` fields

In a multithreaded environment generation of UUIDs is completely independent on different threads.
That is, different threads behave similar to how different processes would with regards to the UUIDs they generate. If full monotonicity for UUID versions 6 or 7 across different threads is desired the generation and clock usage can be made synchronous by providing custom `clock_persistence` callback implementation. 

By default, if available, one of the system's network cards MAC addresses is used for version 1 UUIDs. If not available it is replaced by a random number (initialized once per process) as described in RFC 9562. You can change this behavior via `set_node_id` APIs. Alternatively, you can simply use UUID versions 6, 7 or 4.



