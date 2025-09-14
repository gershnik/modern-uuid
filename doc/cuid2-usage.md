
# CUID2 Usage Guide

<!-- TOC -->

- [Basics](#basics)
    - [Headers and namespaces](#headers-and-namespaces)
    - [Exceptions and errors](#exceptions-and-errors)
    - [Thread safety](#thread-safety)
    - [Multiprocess safety](#multiprocess-safety)
- [Usage](#usage)
    - [`cuid2` class](#cuid2-class)
    - [Literals](#literals)
    - [Generation](#generation)
    - [Conversions from/to strings](#conversions-fromto-strings)
    - [Conversion from raw bytes](#conversion-from-raw-bytes)
    - [Comparisons and hashing](#comparisons-and-hashing)
    - [Formatting and I/O](#formatting-and-io)
- [Advanced](#advanced)
    - [Accessing raw bytes](#accessing-raw-bytes)
    - [Controlling host fingerprint data](#controlling-host-fingerprint-data)

<!-- /TOC -->

## Basics

### Headers and namespaces
Everything related to CUID2s is provided by a single include file:
```cpp
#include <modern-uuid/cuid2.h>
```

Everything in the library is under `namespace muuid`. A declaration:
```cpp
using namespace muuid;
```
is assumed in all the examples below.

### Exceptions and errors

In general `modern-uuid` itself doesn't use exceptions. 

Most methods are `noexcept` and a few that aren't (notably CUID2 generation one)
can only emit exceptions thrown by standard library facilities they use. Such methods are always strongly exception safe. 

A few methods that can fail (such as CUID2 parsing) report errors via either a boolean return or returning an `std::optional`. 

Specializations of `std::format` and `fmt::format` are normally required to throw which they do (`std::format_error` and 
`fmt::format_error` respectively) but _only if exceptions are not disabled_. If exceptions are disabled `throw` is replaced by 
printing the error message to `stderr` and `abort()`

### Thread safety

`modern-uuid` provides standard thread safety guarantees. Simultaneous "read" (e.g. non const) operations on `cuid2` objects can be 
performed from multiple threads without synchronization. Simultaneous writes require mutual exclusion with other writes and reads.

CUID2 generation is thread safe and can be invoked simultaneously from multiple threads. It is guaranteed that CUID2s generated from different  
threads will not collide.

### Multiprocess safety

CUID2s generated from different processes have negligible probability of colliding.

CUID2 generation is also safe to use from a `fork()`-ed process. CUID2s generated in parent and child processes have the same
guarantees as CUID2s generated from completely unrelated processes - negligible probability of colliding.

## Usage

### `cuid2` class

`cuid2` is the class representing a CUID2. It is a [regular](https://en.cppreference.com/w/cpp/concepts/regular), 
[totally ordered](https://en.cppreference.com/w/cpp/concepts/totally_ordered) and 
[three way comparable](https://en.cppreference.com/w/cpp/utility/compare/three_way_comparable) class.
It is [trivially copyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable) and has a 
[standard layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType). Its size is 16 bytes and it has the same alignment as an `unsigned char`. 

Internally `cuid2` stores 16 binary packed CUID2 bytes. Currently, CUID2s of custom length are not supported.

### Literals

You can create CUID2 compile-time literals like this:

```cpp
constexpr cuid2 c1("tz4a98xxat96iws9zmbrgj3a"); 
//or
constexpr auto c2 = cuid2("tz4a98xxat96iws9zmbrgj3a");
```

> Wherever this guide uses `char` type and strings of chars you can also use any of
> `wchar_t`, `char16_t`, `char32_t` or `char8_t`. For example:
> ```cpp
> constexpr cuid2 c1(L"tz4a98xxat96iws9zmbrgj3a"); 
> ```
> For brevity this is usually will not be explicitly
> mentioned in the rest of the guide unless there is some difference or limitation.

Note that since CUID2s can be compile-time literals they can be used as template parameters:

```cpp
template<cuid2 C> class some_class {...};

some_class<cuid2("tz4a98xxat96iws9zmbrgj3a")> some_object;
```

A default constructed `cuid2` is a Nil CUID2 `a00000000000000000000000`.
```cpp
assert(cuid2() == cuid2("a00000000000000000000000"));
```

A non const `cuid2` object can be reset to Nil CUID2 via `clear` method

```cpp
cuid2 u = ...;
c.clear();
assert(c == cuid2());
```

If you need it, there is also `cuid2::max()` static method that returns Max CUID2: `zzzzzzzzzzzzzzzzzzzzzzzz`


### Generation

> [!NOTE]
> The Cuid2 format has no spec and its [canonical implementation](https://github.com/paralleldrive/cuid2/blob/main/src/index.js) 
> has many features only relevant to JavaScript and its engines. Accordingly the implementation in this 
> library is the best-effort attempt to implement its generation algorithm as applicable to native code and C++. 

To generate a CUID2 call its static `generate` method.

```cpp
cuid2 c = cuid2::generate();
```

This method can, in principle, throw an exception if the underlying operations on `std::chrono::time_point` throw. 
As far as I know no `std::chrono` implementation actually does throw anything for the operations used, so for all practical purposes 
CUID2 generation is `noexcept`. 

Some aspects of CUID2 generation can be further controlled as explained in the [Advanced](#advanced) section.

### Conversions from/to strings

A `cuid2` can be parsed from any `std::span<char, /*any extent*/>` or anything convertible to such a span (`char` array, `std::string_view`,
`std::string` etc.) via `cuid2::from_chars`. It returns `std::optional<cuid2>` which is empty if the string is not a valid CUID2 representation.
Accepted input is not case sensitive. 

```cpp
std::string str = "pfh0haxfpzowht3oi213cqos";
if (auto maybe_cuid2 = cuid2::from_chars(str)) {
    // use *maybe_cuid2
}
```

To convert in the opposite direction there is `cuid2::to_chars` method that has two main forms.

1. You can pass the destination buffer as an argument. It must be an `std::span<char, 24>` or anything convertible to it.

```cpp
cuid2 c = ...;
std::array<char, 24> buf;
//use lowercase
c.to_chars(buf);
//or explicitly
c.to_chars(buf, cuid2::lowercase);
//or if you prefer uppercase
c.to_chars(buf, cuid2::uppercase);
```

2. You can have it return an `std::array<char, 24>`

```cpp
cuid2 c = ...;
std::array<char, 24> chars;
//use lowercase
chars = c.to_chars();
//or explicitly
chars = c.to_chars(cuid2::lowercase);
//or if you prefer uppercase
chars = c.to_chars(cuid2::uppercase);
```

If you want to use another character type you need to specify it explicitly:

```cpp
std::array<wchar_t, 24> wchars = c.to_chars<wchar_t>();
std::array<char32_t, 24> wchars = c.to_chars<char32_t>();
//etc.
```

Finally there is also `cuid2::to_string` that returns `std::string`

```cpp
cuid2 c = ...;
std::string str;
//use lowercase
str = c.to_string();
//or explicitly
str = c.to_string(cuid2::lowercase);
//or if you prefer uppercase
str = c.to_string(cuid2::uppercase);
```

> [!NOTE]
> C++ standard unfortunately and bizarrely does not allow us to specialize `std::to_string` for user types.

Similarly to `to_chars` if you want to use a different char type you need to specify it explicitly

```cpp
std::wstring str = c.to_string<wchar_t>();
```

### Conversion from raw bytes 

You can create a `cuid2` from `std::span</*byte-like*/, 16>` or anything convertible to such a span using `cuid2::from_bytes`
method. 

A _byte_like_ is any standard layout type of sizeof 1 that is convertible to `uint8_t`. This includes `uint8_t` itself, 
`std::byte`, `char`, `signed char` and `unsigned char`. 

Unlike UUIDs and ULIDs for CUID2s not every byte combination is a valid representation so the conversion returns
`std::optional<cuid2>` which will be empty if the input is invalid.

```cpp
std::array<std::byte, 16> arr1 = {...};
std::optional<cuid2> maybe_c1 = cuid2::from_bytes(arr1);

uint8_t arr2[16] = {...};
std::optional<cuid2> maybe_c2 = cuid2::from_bytes(arr2);

std::vector<uint8_t> vec = {...};
std::optional<cuid2> maybe_c3 = cuid2::from_bytes(std::span{vec}.subspan<3, 19>());
```

> [!NOTE]
> Because `cuid2::from_bytes` validates its input it is not optimally efficient to populate `cuid2` with bytes
> that are known to be good (perhaps because they came from another `cuid2`). In such case you might prefer to use
> [raw bytes access](#accessing-raw-bytes). However, see warnings in that section.

### Comparisons and hashing

`cuid2` objects can be compared in every possible way via `<`, `<=`, `==`, `!=`, `>=`, `>` and `<=>`. The ordering is lexicographical
by bytes from left to right. Thus:
```cpp
cuid2 c = ...;
//every cuid2 is greater or equal to a Nil CUID2
assert(c >= cuid2());
//every cuid2 is less or equal to a Max CUID2
assert(c <= cuid2::max());
```

`cuid2` objects also support hashing via `std::hash` or by calling `hash_value()` function. Thus you can have:

```cpp
std::map<cuid2, something> m;
std::unordered_map<cuid2, something> um;
```

The `hash_value()` function allows you to easily adapt `cuid2` to other hashing schemes (e.g. `boost::hash`).

### Formatting and I/O

`cuid2` objects can be formatted using `std::format` (if your standard library has it) and `fmt::format` (if you include `fmt` headers _before_
`modern-uuid/cuid2.h`). 
For either two formatting flags are defined: `l` which formats using lowercase (the default) and `u` which formats in uppercase.

```cpp
constexpr cuid2 c("nc6bzmkmd014706rfda898to");

std::string str = std::format("{}", c);
assert(str == "nc6bzmkmd014706rfda898to");

str = std::format("{:u}", c);
assert(str == "NC6BZMKMD014706RFDA898TO");

str = std::format("{:l}", c);
assert(str == "nc6bzmkmd014706rfda898to");
```

`cuid2` objects can also be written to and read from iostreams. When reading case is ignored.

```cpp
std::istringstream istr("NC6BZMKMD014706RFDA898TO");
cuid2 cr;
istr >> cr;
assert(cr = cuid2("nc6bzmkmd014706rfda898to"));
```

When writing, the standard `std::ios_base::uppercase` flag (which can be set using `std::uppercase`/`std::nouppercase` manipulators) 
controls the output format.

```cpp
std::ostringstream ostr;
ostr << cuid2("nc6bzmkmd014706rfda898to");
assert(ostr.str() == "nc6bzmkmd014706rfda898to");

ostr.str("");
ostr << std::uppercase << cuid2("nc6bzmkmd014706rfda898to");
assert(ostr.str() == "NC6BZMKMD014706RFDA898TO");
```

## Advanced

### Accessing raw bytes

You can access raw `cuid2` bytes via its `bytes` public data member. 
Very unfortunately, this member has to be public in order to make `cuid2` a 
[_structural type_](https://en.cppreference.com/w/cpp/language/template_parameters).

The `bytes` member is an `std:array<uint8_t, 16>` and can be manipulated using all the usual range machinery

> [!CAUTION]
> Writing directly to `bytes` may result in an invalid `cuid2` object. This can result in undefined behavior. 
> Please use with extreme caution only if you are absolutely certain that the bytes are valid. 
> Otherwise use `from_bytes` method to perform safe conversion.

```cpp
constexpr cuid2 c("nc6bzmkmd014706rfda898to");
assert(c.bytes[3] == 23);
for(auto byte: c.bytes) {
    ...
}
```

### Controlling host fingerprint data

One part of data that is mixed into the final CUID2 value is the host fingerprint - something that
differentiates current CUID2 producer host from other, potentially very identical ones. 
More details on why this is a good idea can be found in [the official CUID2 documentation](https://github.com/paralleldrive/cuid2).

Currently, by default, `modern-uuid` uses host's MAC address, if available, as part of the host fingerprint. (The rest is made
of random bytes.) This is the same data as used for generation of UUIDv1 and it can be controlled via the same mechanism.

> [!NOTE]
> Unlike with UUIDv1 there is no security/privacy concern with using a MAC address in CUID2 generation. It is not uses
> as-is but is rather mixed with other data and then cryptographically hashed. Its purpose is to provide additional entropy
> only and it is not visible to or recoverable by an attacker.

If the default behavior is insufficient or suboptimal, `modern-uuid` allows you to customize what data to use for the 
host fingerprint (known as the `node_id` field) via `set_node_id()` overloaded function family. 

You can use it to force random `node_id`:

```cpp
//this will generate a random node_id and use it in all subsequent generate_time_based() calls
//the generated value is returned in case you want to save it for later use
std::array<uint8_t, 6> node_id = set_node_id(node_id::generate_random);
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
std::array<uint8_t, 6> node_id = set_node_id(node_id::detect_system);
```

