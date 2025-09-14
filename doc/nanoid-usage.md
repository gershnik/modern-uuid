
# NanoID Usage Guide

<!-- TOC -->

- [Basics](#basics)
    - [Headers and namespaces](#headers-and-namespaces)
    - [Exceptions and errors](#exceptions-and-errors)
    - [Thread safety](#thread-safety)
    - [Multiprocess safety](#multiprocess-safety)
- [Usage](#usage)
    - [`nanoid` class and `basic_nanoid` class template](#nanoid-class-and-basic_nanoid-class-template)
    - [Literals](#literals)
    - [Generation](#generation)
    - [Conversions from/to strings](#conversions-fromto-strings)
    - [Conversion from raw bytes](#conversion-from-raw-bytes)
    - [Comparisons and hashing](#comparisons-and-hashing)
    - [Formatting and I/O](#formatting-and-io)
- [Advanced](#advanced)
    - [Accessing raw bytes](#accessing-raw-bytes)
- [Custom Alphabets](#custom-alphabets)

<!-- /TOC -->

## Basics

### Headers and namespaces
Everything related to NanoIDs is provided by a single include file:
```cpp
#include <modern-uuid/nanoid.h>
```

Everything in the library is under `namespace muuid`. A declaration:
```cpp
using namespace muuid;
```
is assumed in all the examples below.

### Exceptions and errors

In general `modern-uuid` itself doesn't use exceptions. 

Most methods are `noexcept` and a few that aren't can only emit exceptions thrown by standard library 
facilities they use. Such methods are always strongly exception safe. 

A few methods that can fail (such as NanoID parsing) report errors via either a boolean return or returning an `std::optional`. 

Specializations of `std::format` and `fmt::format` are normally required to throw which they do (`std::format_error` and 
`fmt::format_error` respectively) but _only if exceptions are not disabled_. If exceptions are disabled `throw` is replaced by 
printing the error message to `stderr` and `abort()`

### Thread safety

`modern-uuid` provides standard thread safety guarantees. Simultaneous "read" (e.g. non const) operations on `nanoid` objects can be 
performed from multiple threads without synchronization. Simultaneous writes require mutual exclusion with other writes and reads.

NanoID generation is thread safe and can be invoked simultaneously from multiple threads. It is guaranteed that NanoIDs generated from different  
threads will not collide.

### Multiprocess safety

NanoIDs generated from different processes have negligible probability of colliding.

NanoID generation is also safe to use from a `fork()`-ed process. NanoIDs generated in parent and child processes have the same
guarantees as NanoIDs generated from completely unrelated processes - negligible probability of colliding.

## Usage

### `nanoid` class and `basic_nanoid` class template

General NanoIDs are represented by a `basic_nanoid<Alphabet, Size>` class template. Note that the `Size`
template parameter is the size of _textual representation of the NanoID in characters_, not the size of
the class instances in bytes.

The `nanoid` class is simply a typedef to `basic_nanoid<nanoid_default_alphabet, 21>` and represents the default
NanoID variant.

`basic_nanoid` is a [regular](https://en.cppreference.com/w/cpp/concepts/regular), 
[totally ordered](https://en.cppreference.com/w/cpp/concepts/totally_ordered) and 
[three way comparable](https://en.cppreference.com/w/cpp/utility/compare/three_way_comparable) class.
It is [trivially copyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable) and has a 
[standard layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType). 

Its size depends on the Alphabet and Size template parameters. For default `nanoid` it is 16 bytes.
It has the same alignment as an `unsigned char`. 

Internally `nanoid` stores NanoID data in an array of bytes that packs the NanoID bits. Since default NanoID uses 6 bits per character and
21 character this results in 6*21=126 total bits which require 16 bytes to store.

Custom alphabets and sizes are described in the [Custom Alphabets](#custom_alphabets) section. The following guide describes the
default `nanoid` behavior which is mostly the same for other `basic_nanoid` types with obvious exceptions of string representation
and byte size.

### Literals

You can create NanoID compile-time literals like this:

```cpp
constexpr nanoid n1("Uakgb_J5m9g-0JDMbcJqL"); 
//or
constexpr auto n2 = nanoid("Uakgb_J5m9g-0JDMbcJqL");
```

> Wherever this guide uses `char` type and strings of chars you can also use any of
> `wchar_t`, `char16_t`, `char32_t` or `char8_t`. For example:
> ```cpp
> constexpr nanoid n1(L"Uakgb_J5m9g-0JDMbcJqL"); 
> ```
> For brevity this is usually will not be explicitly
> mentioned in the rest of the guide unless there is some difference or limitation.

Note that since NanoIDs can be compile-time literals they can be used as template parameters:

```cpp
template<nanoid NID> class some_class {...};

some_class<nanoid("Uakgb_J5m9g-0JDMbcJqL")> some_object;
```

A default constructed `nanoid` is a Nil NanoID `uuuuuuuuuuuuuuuuuuuuu`.
```cpp
assert(nanoid() == nanoid("uuuuuuuuuuuuuuuuuuuuu"));
```

A non const `nanoid` object can be reset to Nil NanoID via `clear` method

```cpp
nanoid n = ...;
n.clear();
assert(n == nanoid());
```

If you need it, there is also `nanoid::max()` static method that returns Max NanoID: `ttttttttttttttttttttt`


### Generation

To generate a NanoID call its static `generate` method.

```cpp
nanoid n = nanoid::generate();
```

This method is `noexcept`. 

### Conversions from/to strings

A `nanoid` can be parsed from any `std::span<char, /*any extent*/>` or anything convertible to such a span (`char` array, `std::string_view`,
`std::string` etc.) via `nanoid::from_chars`. It returns `std::optional<nanoid>` which is empty if the string is not a valid NanoID representation.

```cpp
std::string str = "HPTI4DZV5qieFysheU5_f";
if (auto maybe_nanoid = nanoid::from_chars(str)) {
    // use *maybe_nanoid
}
```

To convert in the opposite direction there is `nanoid::to_chars` method that has two main forms.

1. You can pass the destination buffer as an argument. It must be an `std::span<char, 21>` or anything convertible to it.

```cpp
nanoid n = ...;
std::array<char, 21> buf;
n.to_chars(buf);
```

2. You can have it return an `std::array<char, 21>`

```cpp
nanoid n = ...;
std::array<char, 21> chars = n.to_chars();
```

If you want to use another character type you need to specify it explicitly:

```cpp
std::array<wchar_t, 21> wchars = n.to_chars<wchar_t>();
std::array<char32_t, 21> wchars = n.to_chars<char32_t>();
//etc.
```

Finally there is also `nanoid::to_string` that returns `std::string`

```cpp
nanoid n = ...;
std::string str = n.to_string();
```

> [!NOTE]
> C++ standard unfortunately and bizarrely does not allow us to specialize `std::to_string` for user types.

Similarly to `to_chars` if you want to use a different char type you need to specify it explicitly

```cpp
std::wstring str = n.to_string<wchar_t>();
```

### Conversion from raw bytes 

You can create a `nanoid` from `std::span</*byte-like*/, 16>` or anything convertible to such a span using `nanoid::from_bytes`
method. 

A _byte_like_ is any standard layout type of sizeof 1 that is convertible to `uint8_t`. This includes `uint8_t` itself, 
`std::byte`, `char`, `signed char` and `unsigned char`. 

Unlike UUIDs and ULIDs for NanoIDs not every byte combination is a valid representation so the conversion returns
`std::optional<nanoid>` which will be empty if the input is invalid.

```cpp
std::array<std::byte, 16> arr1 = {...};
std::optional<nanoid> maybe_n1 = nanoid::from_bytes(arr1);

uint8_t arr2[16] = {...};
std::optional<nanoid> maybe_n2 = nanoid::from_bytes(arr2);

std::vector<uint8_t> vec = {...};
std::optional<nanoid> maybe_n3 = nanoid::from_bytes(std::span{vec}.subspan<3, 19>());
```

> [!NOTE]
> Because `nanoid::from_bytes` validates its input it is not optimally efficient to populate `nanoid` with bytes
> that are known to be good (perhaps because they came from another `nanoid`). In such case you might prefer to use
> [raw bytes access](#accessing-raw-bytes). However, see warnings in that section.

### Comparisons and hashing

`nanoid` objects can be compared in every possible way via `<`, `<=`, `==`, `!=`, `>=`, `>` and `<=>`. The ordering is lexicographical
by bytes from left to right. Thus:
```cpp
nanoid n = ...;
//every nanoid is greater or equal to a Nil NanoID
assert(n >= nanoid());
//every nanoid is less or equal to a Max NanoID
assert(u <= nanoid::max());
```

`nanoid` objects also support hashing via `std::hash` or by calling `hash_value()` function. Thus you can have:

```cpp
std::map<nanoid, something> m;
std::unordered_map<nanoid, something> um;
```

The `hash_value()` function allows you to easily adapt `nanoid` to other hashing schemes (e.g. `boost::hash`).

### Formatting and I/O

`nanoid` objects can be formatted using `std::format` (if your standard library has it) and `fmt::format` (if you include `fmt` headers _before_
`modern-uuid/nanoid.h`).

There are currently no formatting flags defined for `nanoid` objects.

```cpp
std::string str = std::format("{}", nanoid("V1StGXR8_Z5jdHi6B-myT"));
assert(str == "V1StGXR8_Z5jdHi6B-myT");
```

`nanoid` objects can also be written to and read from iostreams.

```cpp
std::istringstream istr("V1StGXR8_Z5jdHi6B-myT");
nanoid nr;
istr >> nr;
assert(nr = nanoid("V1StGXR8_Z5jdHi6B-myT"));

std::ostringstream ostr;
ostr << nanoid("V1StGXR8_Z5jdHi6B-myT");
assert(ostr.str() == "V1StGXR8_Z5jdHi6B-myT");
```

## Advanced

### Accessing raw bytes

You can access raw `nanoid` bytes via its `bytes` public data member. 
Very unfortunately, this member has to be public in order to make `nanoid` a 
[_structural type_](https://en.cppreference.com/w/cpp/language/template_parameters).

The `bytes` member is an `std:array<uint8_t, 16>` and can be manipulated using all the usual range machinery

> [!CAUTION]
> Writing directly to `bytes` may result in an invalid `nanoid` object. This can result in undefined behavior. 
> Please use with extreme caution only if you are absolutely certain that the bytes are valid. 
> Otherwise use `from_bytes` method to perform safe conversion.

```cpp
constexpr nanoid n("V1StGXR8_Z5jdHi6B-myT");
assert(n.bytes[3] == 237);
for(auto byte: n.bytes) {
    ...
}
```

## Custom Alphabets

The `basic_nanoid` class template can be used to create NanoID types with custom alphabets and string sizes.

To declare a custom alphabet you must use `MUUID_DECLARE_NANOID_ALPHABET` macro. For example

```cpp
MUUID_DECLARE_NANOID_ALPHABET(my_alphabet, "1234567890abcdef")
```

Once declared, the alphabet can be used in `basic_nanoid` instantiation:

```cpp
using my_id = basic_nanoid<my_alphabet, 10>;
```

This creates a NanoID type which uses `1234567890abcdef` alphabet and is 10 characters long in its string representation.

The size of my_id object will be the minimum byte size needed to pack bits that can be represented by the alphabet times
the number of characters. In the example above the alphabet has 16 characters which require 4 bits each. This means the total
size is 40 bits which requires 5 bytes to store

```cpp
assert(sizeof(my_id) == 5);
```

Once declared a custom `basic_nanoid` type can be used just like default `nanoid` as described above:

```cpp
constexpr my_id mid1("4f90d13a42");
my_id mid2 = my_id::generate();
auto str = mid1.to_string();
std::unordered_map<my_id, string> some_map = ...;
//etc.
```

The rules for the custom alphabets that can be used with `MUUID_DECLARE_NANOID_ALPHABET` macro are as follows:

* The size of the alphabet must be between 2 and 128 inclusive. 
* All alphabet characters must be smaller than 128 in UTF-8 encoding. In other words they are limited to ASCII
* If your `char` encoding is not ASCII-compatible (very rare nowadays but can happen on EBCDIC mainframes) all
  the alphabet characters must be representable as single byte `char`s
* Similarly all the characters must be representable as `wchar_t` smaller than 256.

All these rules are checked via `static_assert`s in the library code and your alphabet will not compile if they are violated.

For reference the `nanoid_default_alphabet` used by `nanoid` class is:

```
useandom-26T198340PX75pxJACKVERYMINDBUSHWOLF_GQZbfghjklqvwyzrict
```

You can find more interesting alphabets at [nanoid-dictionary](https://github.com/CyberAP/nanoid-dictionary) project.

If you use custom alphabets please check the safety of your custom alphabet and ID size using 
[Nano ID Collision Calculator](https://zelark.github.io/nano-id-cc/)


