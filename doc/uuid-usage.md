
# UUID Usage Guide

<!-- TOC -->

- [Basics](#basics)
    - [Headers and namespaces](#headers-and-namespaces)
    - [Exceptions and errors](#exceptions-and-errors)
    - [Thread safety](#thread-safety)
    - [Multiprocess safety](#multiprocess-safety)
- [Usage](#usage)
    - [uuid class](#uuid-class)
    - [Literals](#literals)
    - [Constructing from raw bytes](#constructing-from-raw-bytes)
    - [Accessing raw bytes](#accessing-raw-bytes)
    - [Generation](#generation)
        - [What about UUID versions 2 and 8?](#what-about-uuid-versions-2-and-8)
    - [Conversions from/to strings](#conversions-fromto-strings)
    - [Comparisons and hashing](#comparisons-and-hashing)
    - [Formatting and I/O](#formatting-and-io)
    - [Interoperability](#interoperability)
        - [macOS](#macos)
        - [Windows](#windows)
        - [__uuidof](#__uuidof)
    - [Accessing UUID properties](#accessing-uuid-properties)
    - [Other features](#other-features)
- [Advanced](#advanced)
    - [Controlling MAC address use for UUID version 1](#controlling-mac-address-use-for-uuid-version-1)
    - [Persisting/synchronizing the clock state](#persistingsynchronizing-the-clock-state)
- [Implementation details](#implementation-details)

<!-- /TOC -->

## Basics

### Headers and namespaces
Everything related to UUIDs is provided by a single include file:
```cpp
#include <modern-uuid/uuid.h>
```

Everything in the library is under `namespace muuid`. A declaration:
```cpp
using namespace muuid;
```
is assumed in all the examples below.

### Exceptions and errors

In general `modern-uuid` itself doesn't use exceptions. 

Most methods are `noexcept` and a few that aren't (notably UUID generation ones)
can only emit exceptions thrown by standard library facilities they use. Such methods are always strongly exception safe. 

A few methods that can fail (such as UUID parsing) report errors via either a boolean return or returning an `std::optional`. 

Specializations of `std::format` and `fmt::format` are normally required to throw which they do (`std::format_error` and 
`fmt::format_error` respectively) but _only if exceptions are not disabled_. If exceptions are disabled `throw` is replaced by 
printing the error message to `stderr` and `abort()`

### Thread safety

`modern-uuid` provides standard thread safety guarantees. Simultaneous "read" (e.g. non const) operations on `uuid` objects can be 
performed from multiple threads without synchronization. Simultaneous writes require mutual exclusion with other writes and reads.

UUID generation is thread safe and can be invoked simultaneously from multiple threads. It is guaranteed that UUIDs generated from different  
threads will not collide.

### Multiprocess safety

UUIDs generated from different processes have negligible probability of colliding.

UUID generation is also safe to use from a `fork()`-ed process. UUIDs generated in parent and child processes have the same
guarantees as UUIDs generated from completely unrelated processes - negligible probability of colliding.

## Usage

### `uuid` class

`uuid` is the class that represents a UUID. It is a [regular](https://en.cppreference.com/w/cpp/concepts/regular), 
[totally ordered](https://en.cppreference.com/w/cpp/concepts/totally_ordered) and 
[three way comparable](https://en.cppreference.com/w/cpp/utility/compare/three_way_comparable) class.
It is [trivially copyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable) and has a 
[standard layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType). Its size is 16 bytes and it has the same alignment as an `unsigned char`. 

Internally `uuid` stores UUID bytes in as an array in their natural order. It is layout compatible with `uuid_t` type of BSD or Linux `libuuid`.
It is **not** layout compatible with Microsoft's GUID/UUID and similar structures on little-endian machines.

### Literals

You can create UUID compile-time literals like this:

```cpp
constexpr uuid u1("e53d37db-e4e0-484f-996f-3ab1d4701abc"); 
//or
constexpr auto u2 = uuid("bc961bfb-b006-42f4-93ae-206f02658810");
```

> Wherever this guide uses `char` type and strings of chars you can also use any of
> `wchar_t`, `char16_t`, `char32_t` or `char8_t`. For example:
> ```cpp
> constexpr uuid u1(L"e53d37db-e4e0-484f-996f-3ab1d4701abc"); 
> ```
> For brevity this is usually will not be explicitly
> mentioned in the rest of the guide unless there is some difference or limitation.

Note that since UUIDs can be compile-time literals they can be used as template parameters:

```cpp
template<uuid U1> class some_class {...};

some_class<uuid("bc961bfb-b006-42f4-93ae-206f02658810")> some_object;
```

A default constructed `uuid` is a Nil UUID `00000000-0000-0000-0000-000000000000`.
```cpp
assert(uuid() == uuid("00000000-0000-0000-0000-000000000000"));
```

If you need it, there is also `uuid::max()` static method that returns Max UUID: `FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF`

### Constructing from raw bytes 

You can construct a `uuid` from `std::span</*byte-like*/, 16>` or anything convertible to such a span. A _byte_like_ is
any standard layout type of sizeof 1 that is convertible to `uint8_t`. This includes `uint8_t` itself, `std::byte`, `char`,
`signed char` and `unsigned char`. 

```cpp
std::array<std::byte, 16> arr1 = {...};
uuid u1(arr1);

uint8_t arr2[16] = {...};
uuid u2(arr2);

std::vector<uint8_t> vec = {...};
uuid u2(std::span{vec}.subspan<3, 19>());
```

This conversion allows for easy interoperability with `uuid_t` type common on Linux/BSD platforms. That type is simply
`unsigned char [16]` and can be passed directly to `uuid` constructor.

### Accessing raw bytes

You can access raw `uuid` bytes via its `bytes` public data member. Yes this data member is public, which is ok because
`uuid` class doesn't really have any invariants. *Any* 16 bytes could conceivably represent some kind of UUID and
`uuid` class isn't enforcing any constraints on that.
In addition having this member public is required in order to make `uuid` a [_structural type_](https://en.cppreference.com/w/cpp/language/template_parameters).

The `bytes` member is an `std:array<uint8_t, 16>` and can be manipulated using all the usual range machinery

```cpp
constexpr uuid u("7d444840-9dc0-11d1-b245-5ffdce74fad2");
assert(u.bytes[2] == 0x44);
for(auto byte: u.bytes) {
    ...
}
```

### Generation

This library supports generation of UUIDs of versions 1, 3, 4, 5 and 7 as defined by [RFC 9562](https://datatracker.ietf.org/doc/rfc9562/).

```cpp
uuid u_v1 = uuid::generate_time_based();
uuid u_v3 = uuid::generate_md5(uuid::namespaces::dns, "www.widgets.com");
uuid u_v4 = uuid::generate_random();
uuid u_v5 = uuid::generate_sha1(uuid::namespaces::url, "http://www.widgets.com");
uuid u_v6 = uuid::generate_reordered_time_based();
uuid u_v7 = uuid::generate_unix_time_based();
```

The `uuid::generate_md5` and `uuid::generate_sha1` methods take two arguments: a namespace and name. The namespace is a `uuid` itself.
The `uuid::namespaces` struct contains constants for well-known standard namespaces. These are:

```cpp
//name is a fully-qualified domain name
uuid::namespaces::dns;
//name is a URL
uuid::namespaces::url;
//name is an ISO OID 
uuid::namespaces::oid;
//name is an X.500 DN (in DER or a text output format) 
uuid::namespaces::x500;
```

The `name` argument is an `std::string_view`.

The `uuid::generate_md5`, `uuid::generate_random` and `uuid::generate_sha1` are all `noexcept`. 

The other methods, which produce so-called "time-based" UUIDs can, in principle, throw an exception if the
underlying arithmetic operations on `std::chrono::time_point` and `std::chrono::duration` throw. (See for example https://en.cppreference.com/w/cpp/chrono/time_point/operator_arith that claims that adding a `duration` to a `time_point` "\[m\]ay throw implementation-defined exceptions").
As far as I know no `std::chrono` implementation actually does throw anything, so for all practical purposes generation of time-based UUIDs is also `noexcept`. 

> [!NOTE]
> `generate_time_based()` will use one of the system MAC addresses, if available. Otherwise it
> will fall back on a randomly generated 6 bytes. If you want to avoid MAC address usage at all 
> consider using UUID versions 6 or 7 via 
> `generate_reordered_time_based()`/`uuid::generate_unix_time_based()` as recommended by RFC 9562. 

More details about various implementation details of UUID generation can be found in [Implementation Details](../README.md#implementation-details) section of the README.

Many aspects of UUID generation can be further controlled as explained in the [Advanced](#advanced) section.

#### What about UUID versions 2 and 8?

Version 8 of UUID is for custom UUIDs that are generated however _you_ wish. There isn't anything the library can help with to generate those.
If you want to use them you can generate them in a byte array and construct `uuid` from it or just directly generate into `uuid::bytes`.

Version 2 is so called DCE security UUIDs and their format is out of scope for RFC 9562. While a specification of how to generate them is available, their usage appears to be non-existent at this point in time (if they were ever used). 

If you have any need to generate them please [open an issue](https://github.com/gershnik/modern-uuid/issues) and it might be possible to add this functionality.


### Conversions from/to strings

A `uuid` can be parsed from any `std::span<char, /*any extent*/>` or anything convertible to such a span (`char` array, `std::string_view`,
`std::string` etc.) via `uuid::from_chars`. It returns `std::optional<uuid>` which is empty if the string is not a valid UUID representation.
Accepted format is `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` case insensitive. 

```cpp
if (auto maybe_uuid = uuid::from_chars("7d444840-9dc0-11d1-b245-5ffdce74fad2")) {
    // use *maybe_uuid
}
```

To convert in the opposite direction there is `uuid::to_chars` method that has two main form.

1. You can pass the destination buffer as an argument. It must be an `std::span<char, 36>` or anything convertible to it.

```cpp
uuid u = ...;
std::array<char, 36> buf;
//use lowercase
u.to_chars(buf);
//or explicitly
u.to_chars(buf, uuid::lowercase);
//or if you prefer uppercase
u.to_chars(buf, uuid::uppercase);
```

2. You can have it return an `std::array<char, 36>`

```cpp
uuid u = ...;
std::array<char, 36> chars;
//use lowercase
chars = u.to_chars();
//or explicitly
chars = u.to_chars(uuid::lowercase);
//or if you prefer uppercase
chars = u.to_chars(uuid::uppercase);
```

If you want to use another character type you need to specify it explicitly:

```cpp
std::array<wchar_t, 36> wchars = u.to_chars<wchar_t>();
std::array<char32_t, 36> wchars = u.to_chars<char32_t>();
//etc.
```

Finally there is also `uuid::to_string` that returns `std::string`

```cpp
uuid u = ...;
std::string str;
//use lowercase
str = u.to_string();
//or explicitly
str = u.to_string(uuid::lowercase);
//or if you prefer uppercase
str = u.to_string(uuid::uppercase);
```

> [!NOTE]
> C++ standard unfortunately and bizarrely does not allow us to specialize `std::to_string` for user types.

Similarly to `to_chars` if you want to use a different char type you need to specify it explicitly

```cpp
std::wstring str = u.to_string<wchar_t>();
```


### Comparisons and hashing

`uuid` objects can be compared in every possible way via `<`, `<=`, `==`, `!=`, `>=`, `>` and `<=>`. The ordering is lexicographical
by bytes from left to right. Thus:
```cpp
uuid u = ...;
//every uuid is greater or equal to a Nil UUID
assert(u >= uuid());
//every uuid is less or equal to a Max UUID
assert(u <= uuid::max());
```

`uuid` objects also support hashing via `std::hash` or by calling `hash_value()` function. Thus you can have:

```cpp
std::map<uuid, something> m;
std::unordered_map<uuid, something> um;
```

The `hash_value()` function allows you to easily adapt `uuid` to other hashing schemes (e.g. `boost::hash`).

### Formatting and I/O

`uuid` objects can be formatted using `std::format` (if your standard library has it) and `fmt::format` (if you include `fmt` headers _before_
`modern-uuid/uuid.h`). 
For either two formatting flags are defined: `l` which formats using lowercase (the default) and `u` which formats in uppercase.

```cpp
constexpr uuid u("F537C67E-09FF-41F9-8EDF-BD78489896F0");

std::string str = std::format("{}", u);
assert(str == "f537c67e-09ff-41f9-8edf-bd78489896f0");

str = std::format("{:u}", u);
assert(str == "F537C67E-09FF-41F9-8EDF-BD78489896F0");

str = std::format("{:l}", u);
assert(str == "f537c67e-09ff-41f9-8edf-bd78489896f0");
```

`uuid` objects can also be written to and read from iostreams. When reading case is ignored.

```cpp
std::istringstream istr("bC961bFb-B006-42f4-93ae-206f02658810");
uuid uuidr;
istr >> uuidr;
assert(uuidr = uuid("bc961bfb-b006-42f4-93ae-206f02658810"));
```

When writing, the standard `std::ios_base::uppercase` flag (which can be set using `std::uppercase`/`std::nouppercase` manipulators) 
controls the output format.

```cpp
std::ostringstream ostr;
ostr << uuid("bC961bFb-B006-42f4-93ae-206f02658810");
assert(ostr.str() == "bc961bfb-b006-42f4-93ae-206f02658810");

ostr.str("");
ostr << std::uppercase << uuid("7d444840-9dc0-11d1-b245-5ffdce74fad2");
assert(ostr.str() == "7D444840-9DC0-11D1-B245-5FFDCE74FAD2");
```

### Interoperability 

On some operating systems there are various system defined UUID types that are commonly used in many system APIs.
For ease of interoperability `modern-uuid` provides easy conversions to/from these types. 

#### macOS

`uuid` can be constructed from `CFUUIDBytes` and `CFUUIDRef` as well as `to_CFUUIDBytes` and `to_CFUUID` methods.

```cpp
CFUUIDRef uuidobj = CFUUIDCreate(nullptr);
auto bytes = CFUUIDGetUUIDBytes(uuidobj);

const uuid u_from_obj(uuidobj);
const uuid u_from_bytes(bytes);

assert(u_from_obj == u_from_bytes);

CFUUIDRef uuidobj1 = u_from_obj.to_CFUUID();
assert(CFEqual(uuidobj, uuidobj1));

auto bytes1 = u_from_obj.to_CFUUIDBytes();
CHECK(memcmp(&bytes, &bytes1, sizeof(CFUUIDBytes)) == 0);

```

#### Windows

`uuid` can be constructed from `GUID` structure (and its many aliases like `UUID`, `CLSID` etc.) and provides `to_GUID` method.

```cpp
GUID guid = { /* 0d1be41b-f035-4f89-b404-bc0641afae59 */
    0x0d1be41b,
    0xf035,
    0x4f89,
    {0xb4, 0x04, 0xbc, 0x06, 0x41, 0xaf, 0xae, 0x59}
};

constexpr uuid u = guid;

assert(u.to_string() == "0d1be41b-f035-4f89-b404-bc0641afae59");

GUID guid1 = u.to_GUID();
assert(IsEqualGUID(guid, guid1));
```

#### __uuidof 

On Windows MSVC and clang compilers provide a common extension: `__uuidof` that allows you to obtain UUID previously
associated with a type via `__declpec(uuid())`.

While you can certainly do `uuid(__uuidof(T))`, which is a constexpr, typing it is tedious so this library provides
a shorthand: `uuidof<T>` which is equivalent to the above.

```cpp
class __declspec(uuid("06f9ea87-da78-47aa-8a21-682260ed8b65")) foo;

constexpr auto uuid_of_foo = uuidof<foo>;

assert(uuid_of_foo.to_string() == "06f9ea87-da78-47aa-8a21-682260ed8b65");
```

For convenience and convenience this mechanism is also supported on other compilers and platforms that do not have
`__declspec(uuid)` mechanism. To use it portably you simply need to manually specialize `uuidof` for the desired type. 
You can do it either directly or using a provided macro `MUUID_ASSIGN_UUID`.

```cpp
class foo;
// this is equivalent to
// template<> constexpr muuid::uuid muuid::uuidof<foo>{"6cd966c0-1a04-486d-b642-7fda4cdcf1a4"}
// and must be in global namespace scope 
MUUID_ASSIGN_UUID(foo, "6cd966c0-1a04-486d-b642-7fda4cdcf1a4");

constexpr auto uuid_of_foo = uuidof<foo>;

assert(uuid_of_foo.to_string() == "6cd966c0-1a04-486d-b642-7fda4cdcf1a4");
```


### Accessing UUID properties

You can examine `uuid` variant via `get_variant` method. If the variant is `variant::standard` you can use `get_type` method to 
examine type/version.

```cpp
uuid u("cc318cec-baf4-4f7a-9272-fc6c47a7eab9");
assert(u.get_variant() == uuid::variant::standard);
assert(u.get_type() == uuid::type::time_based);
```

### Other features

For interoperability with older code this library also declares `uuid_parts` struct

```cpp
struct uuid_parts {
    uint32_t    time_low;
    uint16_t    time_mid;
    uint16_t    time_hi_and_version;
    uint16_t    clock_seq;
    uint8_t     node[6];
};
```

A `uuid` object can be created from `uuid_parts`:

```cpp
uuid u1(uuid_parts{0xbc961bfb, 0xb006, 0x42f4, 0x93ae, {0x20, 0x6f, 0x02, 0x65, 0x88, 0x10}});
```

A non const `uuid` object can be reset to Nil UUID via `clear` method

```cpp
uuid u("7d444840-9dc0-11d1-b245-5ffdce74fad2");
u.clear();
assert(u == uuid());
```

## Advanced

### Controlling MAC address use for UUID version 1

By default when generating UUIDs version 1 via `generate_time_based()` method this library uses one of the system's network cards MAC addresses,
if available, to provide "spatial uniqueness" as specified by the RFC. If MAC address cannot be detected a random value will be used instead.

In some cases, such as when generated UUID can be seen by a hostile party, MAC address usage might be undesirable because of the information 
leakage and privacy concerns. 

`modern-uuid` allows you to customize what data to use for the relevant UUID bits (known as the `node_id` field) via `set_node_id()` 
overloaded function family. 

You can use it to force random `node_id`:

```cpp
//this will generate a random node_id and use it in all subsequent generate_time_based() calls
//the generated value is returned in case you want to save it for later use
std::span<const uint8_t, 6> node_id = set_node_id(node_id::generate_random);
```

The generated random value is returned to you so you can save it to use again in later invocations of your program.
You can use a given `std::span<const uint8_t, 6>` as the `node_id` via:

```cpp
std::span<const uint8_t, 6> node_id = ...;
//this will use the passed value in all subsequent generate_time_based() calls
set_node_id(node_id);
```

Finally, you can specify the default behavior and obtain the detected `node_id` via:

```cpp
//this will attempt to detect a MAC address (falling back on generating a random value)
//for node_id and use it in all subsequent generate_time_based() calls
//the generated value is returned in case you want to save it for later use
std::span<const uint8_t, 6> node_id = set_node_id(node_id::detect_system);
```

### Persisting/synchronizing the clock state

For time-based UUID generation, it is often important to persist the last used clock state and/or synchronize its usage between
different threads or processes. It can be especially important for UUID version 1 to fully ensure uniqueness and handle potential
clock going backwards. It can also be important for UUID version 6 and 7 to ensure strict monotonicity between different threads
and/or processes.

To handle this `modern-uuid` specifies `uuid_clock_persistence` callback interface. You can implement this interface in your code and
pass it to the library to be used for time based UUID generation.

The `uuid_clock_persistence` interface itself contains only 3 methods:

```cpp
virtual void add_ref() noexcept = 0;
virtual void sub_ref() noexcept = 0;
virtual per_thread & get_for_current_thread() = 0;
```

The first 2 methods are the classical reference counting. Implement them as you need (e.g. for a static object they would be no-op) 
to ensure that your `uuid_clock_persistence` stays alive while it is being used by the library. 

The third method, `get_for_current_thread()` returns the _actual_ callback interface **for the thread that calls it**. The
`uuid_clock_persistence::per_thread` interface contains the following methods:

```cpp
virtual void close() noexcept = 0;
virtual void lock() = 0;
virtual void unlock() = 0;
virtual bool load(data & d) = 0;
virtual void store(const data & d) = 0;
```

The `close()` method is called when it is safe to destroy the `per_thread` instance. Note that `per_thread` is not reference
counted. This is by design since the library doesn't share its ownership. Its usage is bracketed between `get_for_current_thread()`
and `close()` calls.

The `lock()`/`unlock()` pair are called to lock access to persistent data against other threads or processes. Note that you can use these
methods to simply lock a mutex without having any persistent data - this will simply provide synchronization without persistence.

Finally the `load()`/`store()` are called to load the initial persistent data (it is called only once) and store and changes to it
(this is called every time data changes). All calls to `load()`/`store()` will be within `lock()`/`unlock()` bracket.

The `load()` method should return `false` if the persistent data is not available. 

The `uuid_clock_persistence::data` struct for `load()`/`store()` looks like this:

```cpp
struct data {
    using time_point_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
    
    time_point_t when; 
    uint16_t seq;
    int32_t adjustment;
};
```

The `seq` and `adjustment` fields are opaque values. If you persist data, save and restore them but do not otherwise depend
on their content. 

The `when` field is the last generation timestamp. You also need to save and restore it (if you persist data) but you can also examine
its value. This can be useful in optimizing access to persistent storage. You can actually save the data to storage only infrequently with the 
`when` field set to a future time. If your process is reloaded and the future time is read on load all UUID generators will handle this safely and correctly.


Once you implement `uuid_clock_persistence` and `uuid_clock_persistence::per_thread` you can assign a `uuid_clock_persistence` instance to a given generator via:

```cpp
void set_time_based_persistence(uuid_clock_persistence * persistence);
void set_reordered_time_based_persistence(uuid_clock_persistence * persistence);
void set_unix_time_based_persistence(uuid_clock_persistence * persistence);
```

The new instance will be used for all generations of the given type subsequent to these calls. Pass `nullptr` to remove the custom 
`uuid_clock_persistence`. 

> [!WARN]
> Do not use the **same** `uuid_clock_persistence` for different UUID types! The content and meaning of the `data` is different for each
> and mixing them will produce very bad results.


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
That is, different threads behave similar to how different processes would with regards to the UUIDs they generate. If full monotonicity for UUID versions 6 or 7 across different threads is desired the generation and clock usage can be made synchronous by providing custom `uuid_clock_persistence` callback implementation. 

By default, if available, one of the system's network cards MAC addresses is used for version 1 UUIDs. If not available it is replaced by a random number (initialized once per process) as described in RFC 9562. You can change this behavior via `set_node_id` APIs. Alternatively, you can simply use UUID versions 6, 7 or 4.
