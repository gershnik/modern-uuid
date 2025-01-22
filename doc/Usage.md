
> [!WARNING]  
> This document is currently under construction. 

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
    - [Accessing UUID properties](#accessing-uuid-properties)
    - [Other features](#other-features)

<!-- /TOC -->

## Basics

### Headers and namespaces
There is a single include file in this library
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

`uuid` is the main class of this library. It is a [regular](https://en.cppreference.com/w/cpp/concepts/regular), 
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
for(auto byte: uuid.bytes) {
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
> The `generate_time_based()` and `generate_reordered_time_based()` will use one of the system MAC addresses, if available. Otherwise they
> will fall back on a randomly generated 6 bytes. If you want to avoid MAC address usage at all consider using UUID version 7 via 
> `uuid::generate_unix_time_based()` as recommended by RFC 9562. 

More details about various implementation details of UUID generation can be found in [Implementation Details](../README.md#implementation-details) section of the README.

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

### Accessing UUID properties

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

