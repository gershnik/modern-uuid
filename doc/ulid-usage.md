
# ULID Usage Guide

<!-- TOC -->

- [Basics](#basics)
    - [Headers and namespaces](#headers-and-namespaces)
    - [Exceptions and errors](#exceptions-and-errors)
    - [Thread safety](#thread-safety)
    - [Multiprocess safety](#multiprocess-safety)
- [Usage](#usage)
    - [ulid class](#ulid-class)
    - [Literals](#literals)
    - [Constructing from raw bytes](#constructing-from-raw-bytes)
    - [Accessing raw bytes](#accessing-raw-bytes)
    - [Generation](#generation)
    - [Conversions from/to strings](#conversions-fromto-strings)
    - [Comparisons and hashing](#comparisons-and-hashing)
    - [Formatting and I/O](#formatting-and-io)
- [Advanced](#advanced)
    - [Persisting/synchronizing the clock state](#persistingsynchronizing-the-clock-state)

<!-- /TOC -->

## Basics

### Headers and namespaces
Everything related to ULIDs is provided by a single include file:
```cpp
#include <modern-uuid/ulid.h>
```

Everything in the library is under `namespace muuid`. A declaration:
```cpp
using namespace muuid;
```
is assumed in all the examples below.

### Exceptions and errors

In general `modern-uuid` itself doesn't use exceptions. 

Most methods are `noexcept` and a few that aren't (notably ULID generation ones)
can only emit exceptions thrown by standard library facilities they use. Such methods are always strongly exception safe. 

A few methods that can fail (such as ULID parsing) report errors via either a boolean return or returning an `std::optional`. 

Specializations of `std::format` and `fmt::format` are normally required to throw which they do (`std::format_error` and 
`fmt::format_error` respectively) but _only if exceptions are not disabled_. If exceptions are disabled `throw` is replaced by 
printing the error message to `stderr` and `abort()`

### Thread safety

`modern-uuid` provides standard thread safety guarantees. Simultaneous "read" (e.g. non const) operations on `ulid` objects can be 
performed from multiple threads without synchronization. Simultaneous writes require mutual exclusion with other writes and reads.

ULID generation is thread safe and can be invoked simultaneously from multiple threads. It is guaranteed that ULIDs generated from different  
threads will not collide.

### Multiprocess safety

ULIDs generated from different processes have negligible probability of colliding.

ULID generation is also safe to use from a `fork()`-ed process. ULIDs generated in parent and child processes have the same
guarantees as ULIDs generated from completely unrelated processes - negligible probability of colliding.

## Usage

### `ulid` class

`ulid` is the class representing a ULID. It is a [regular](https://en.cppreference.com/w/cpp/concepts/regular), 
[totally ordered](https://en.cppreference.com/w/cpp/concepts/totally_ordered) and 
[three way comparable](https://en.cppreference.com/w/cpp/utility/compare/three_way_comparable) class.
It is [trivially copyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable) and has a 
[standard layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType). Its size is 16 bytes and it has the same alignment as an `unsigned char`. 

Internally `ulid` stores ULID bytes in as an array in their natural order. 

### Literals

You can create ULID compile-time literals like this:

```cpp
constexpr ulid u1("01k4c0saqszycn0jwbrb3kan9n"); 
//or
constexpr auto u2 = ulid("01k4c0saqszycn0jwbrb3kan9n");
```

> Wherever this guide uses `char` type and strings of chars you can also use any of
> `wchar_t`, `char16_t`, `char32_t` or `char8_t`. For example:
> ```cpp
> constexpr ulid u1(L"01k4c0saqt63ntznp3xah01eb3"); 
> ```
> For brevity this is usually will not be explicitly
> mentioned in the rest of the guide unless there is some difference or limitation.

Note that since ULIDs can be compile-time literals they can be used as template parameters:

```cpp
template<ulid U1> class some_class {...};

some_class<ulid("01k4c0saqt63ntznp3xah01eb3")> some_object;
```

A default constructed `ulid` is a Nil ULID `00000000000000000000000000`.
```cpp
assert(ulid() == ulid("00000000000000000000000000"));
```

A non const `ulid` object can be reset to Nil ULID via `clear` method

```cpp
ulid u = ...;
u.clear();
assert(u == ulid());
```

If you need it, there is also `ulid::max()` static method that returns Max ULID: `7zzzzzzzzzzzzzzzzzzzzzzzzz`

### Constructing from raw bytes 

You can construct a `ulid` from `std::span</*byte-like*/, 16>` or anything convertible to such a span. A _byte_like_ is
any standard layout type of sizeof 1 that is convertible to `uint8_t`. This includes `uint8_t` itself, `std::byte`, `char`,
`signed char` and `unsigned char`. 

```cpp
std::array<std::byte, 16> arr1 = {...};
ulid u1(arr1);

uint8_t arr2[16] = {...};
ulid u2(arr2);

std::vector<uint8_t> vec = {...};
ulid u2(std::span{vec}.subspan<3, 19>());
```

### Accessing raw bytes

You can access raw `ulid` bytes via its `bytes` public data member. Yes this data member is public, which is ok because
`ulid` class doesn't really have any invariants. *Any* 16 bytes could conceivably represent some kind of ULID and
`ulid` class isn't enforcing any constraints on that.
In addition having this member public is required in order to make `ulid` a [_structural type_](https://en.cppreference.com/w/cpp/language/template_parameters).

The `bytes` member is an `std:array<uint8_t, 16>` and can be manipulated using all the usual range machinery

```cpp
constexpr ulid u("01bx5zzkbkactav9wevgemmvry");
assert(u.bytes[6] == 83);
for(auto byte: u.bytes) {
    ...
}
```

### Generation

To generate a ULID call its static `generate` method.

```cpp
ulid u = ulid::generate();
```

This method can, in principle, throw an exception if the underlying arithmetic operations on `std::chrono::time_point` 
and `std::chrono::duration` throw. (See for example https://en.cppreference.com/w/cpp/chrono/time_point/operator_arith 
that claims that adding a `duration` to a `time_point` "\[m\]ay throw implementation-defined exceptions").
As far as I know no `std::chrono` implementation actually does throw anything, so for all practical purposes ULID generation is `noexcept`. 

Some aspects of ULID generation can be further controlled as explained in the [Advanced](#advanced) section.

### Conversions from/to strings

A `ulid` can be parsed from any `std::span<char, /*any extent*/>` or anything convertible to such a span (`char` array, `std::string_view`,
`std::string` etc.) via `ulid::from_chars`. It returns `std::optional<ulid>` which is empty if the string is not a valid ULID representation.
Accepted input is not case insensitive. 

```cpp
if (auto maybe_ulid = ulid::from_chars("01k4c0saqt63ntznp3xah01eb3")) {
    // use *maybe_ulid
}
```

To convert in the opposite direction there is `ulid::to_chars` method that has two main form.

1. You can pass the destination buffer as an argument. It must be an `std::span<char, 26>` or anything convertible to it.

```cpp
ulid u = ...;
std::array<char, 26> buf;
//use lowercase
u.to_chars(buf);
//or explicitly
u.to_chars(buf, ulid::lowercase);
//or if you prefer uppercase
u.to_chars(buf, ulid::uppercase);
```

2. You can have it return an `std::array<char, 26>`

```cpp
ulid u = ...;
std::array<char, 36> chars;
//use lowercase
chars = u.to_chars();
//or explicitly
chars = u.to_chars(ulid::lowercase);
//or if you prefer uppercase
chars = u.to_chars(ulid::uppercase);
```

If you want to use another character type you need to specify it explicitly:

```cpp
std::array<wchar_t, 26> wchars = u.to_chars<wchar_t>();
std::array<char32_t, 26> wchars = u.to_chars<char32_t>();
//etc.
```

Finally there is also `ulid::to_string` that returns `std::string`

```cpp
ulid u = ...;
std::string str;
//use lowercase
str = u.to_string();
//or explicitly
str = u.to_string(ulid::lowercase);
//or if you prefer uppercase
str = u.to_string(ulid::uppercase);
```

> [!NOTE]
> C++ standard unfortunately and bizarrely does not allow us to specialize `std::to_string` for user types.

Similarly to `to_chars` if you want to use a different char type you need to specify it explicitly

```cpp
std::wstring str = u.to_string<wchar_t>();
```


### Comparisons and hashing

`ulid` objects can be compared in every possible way via `<`, `<=`, `==`, `!=`, `>=`, `>` and `<=>`. The ordering is lexicographical
by bytes from left to right. Thus:
```cpp
ulid u = ...;
//every ulid is greater or equal to a Nil ULID
assert(u >= ulid());
//every ulid is less or equal to a Max ULID
assert(u <= ulid::max());
```

`ulid` objects also support hashing via `std::hash` or by calling `hash_value()` function. Thus you can have:

```cpp
std::map<ulid, something> m;
std::unordered_map<ulid, something> um;
```

The `hash_value()` function allows you to easily adapt `ulid` to other hashing schemes (e.g. `boost::hash`).

### Formatting and I/O

`ulid` objects can be formatted using `std::format` (if your standard library has it) and `fmt::format` (if you include `fmt` headers _before_
`modern-uuid/ulid.h`). 
For either two formatting flags are defined: `l` which formats using lowercase (the default) and `u` which formats in uppercase.

```cpp
constexpr ulid u("01k4c3fp5fcfr2syyb3kqd37yk");

std::string str = std::format("{}", u);
assert(str == "01k4c3fp5fcfr2syyb3kqd37yk");

str = std::format("{:u}", u);
assert(str == "01K4C3FP5FCFR2SYYB3KQD37YK");

str = std::format("{:l}", u);
assert(str == "01k4c3fp5fcfr2syyb3kqd37yk");
```

`ulid` objects can also be written to and read from iostreams. When reading case is ignored.

```cpp
std::istringstream istr("01K4C3FP5FCFR2SYYB3KQD37YK");
ulid ur;
istr >> ur;
assert(ur = ulid("01k4c3fp5fcfr2syyb3kqd37yk"));
```

When writing, the standard `std::ios_base::uppercase` flag (which can be set using `std::uppercase`/`std::nouppercase` manipulators) 
controls the output format.

```cpp
std::ostringstream ostr;
ostr << ulid("01k4c3fp5fcfr2syyb3kqd37yk");
assert(ostr.str() == "01k4c3fp5fcfr2syyb3kqd37yk");

ostr.str("");
ostr << std::uppercase << ulid("01k4c3fp5fcfr2syyb3kqd37yk");
assert(ostr.str() == "01K4C3FP5FCFR2SYYB3KQD37YK");
```

## Advanced

### Persisting/synchronizing the clock state

For time-based ULID generation, it might occasionally be desired to persist the last used clock state and/or synchronize its usage between
different threads or processes. For example you might want to ensure strict monotonicity for ULIDs generated by different threads
and/or processes.

To handle this `modern-uuid` specifies `ulid_clock_persistence` callback interface. You can implement this interface in your code and
pass it to the library to be used for ULID generation.

The `ulid_clock_persistence` interface itself contains only 3 methods:

```cpp
virtual void add_ref() noexcept = 0;
virtual void sub_ref() noexcept = 0;
virtual per_thread & get_for_current_thread() = 0;
```

The first 2 methods are the classical reference counting. Implement them as you need (e.g. for a static object they would be no-op) 
to ensure that your `ulid_clock_persistence` stays alive while it is being used by the library. 

The third method, `get_for_current_thread()` returns the _actual_ callback interface **for the thread that calls it**. The
`ulid_clock_persistence::per_thread` interface contains the following methods:

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

The `ulid_clock_persistence::data` struct for `load()`/`store()` looks like this:

```cpp
struct data {
    using time_point_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
    
    time_point_t when; 
    int32_t adjustment;
    uint8_t random[80];
};
```

The `adjustment` and `random` fields are opaque values. If you persist data, save and restore them but do not otherwise depend
on their content. 

The `when` field is the last generation timestamp. You also need to save and restore it (if you persist data) but you can also examine
its value. This can be useful in optimizing access to persistent storage. You can actually save the data to storage only infrequently with the 
`when` field set to a future time. If your process is reloaded and the future time is read on load all ULID generators will handle this safely and correctly.


Once you implement `ulid_clock_persistence` and `ulid_clock_persistence::per_thread` you can assign a `ulid_clock_persistence` instance to a given generator via:

```cpp
void set_time_based_persistence(ulid_clock_persistence * persistence);
void set_reordered_time_based_persistence(ulid_clock_persistence * persistence);
void set_unix_time_based_persistence(ulid_clock_persistence * persistence);
```

The new instance will be used for all generations of the given type subsequent to these calls. Pass `nullptr` to remove the custom 
`ulid_clock_persistence`. 

