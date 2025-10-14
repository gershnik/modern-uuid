# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)

## Unreleased

## [2.1] - 2025-10-14

### Added
- Code portability improvements
- This library now builds and works properly on Haiku OS

### Fixed
- Suppressed bogus GCC warning in SHA1 code (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=106709)

## [2.0] - 2025-09-13

### Changed
- Small incompatible changes to `set_node_id` API
- Removal of all deprecated APIs
- Speed improvements to UUID/ULID generation
- CUID2 is out of experimental mode
- CUID2 generation now mixes system MAC address (if available) to the
  host fingerprint hash.

### Fixed
- A few rare/corner case bugs in UUID/ULID clock persistence handlers
- Potential thread safety issues in `set_node_id`

## [1.10] - 2025-09-10

### Added
- Experimental support for [Cuid2](https://github.com/paralleldrive/cuid2)
- Performance improvements for ULID and Nano ID conversion from/to strings
- Added `char_length` static constant to all ID types that indicates how long they
  are in string form

## [1.9] - 2025-09-05

### Added
- Support for [ULID](https://github.com/ulid/spec)
- Support for [Nano ID](https://github.com/ai/nanoid)
- Significant performance improvements in UUID conversion from/to strings.

### Changed
- Name of `clock_persistence` interface is deprecated. Use identical `uuid_clock_persistence` instead.
  This is to support ULID which has `ulid_clock_persistence`

### Fixed
- The library now properly supports [reproducible builds](https://reproducible-builds.org):
  - CMake build respects SOURCE_DATE_EPOCH environment variable
  - For non-CMake build define MUUID_BUILD_DATE_TIME macro on command line to a string
    containing something resembling concatenation of `__DATE__` `__TIME__`

## [1.8] - 2025-06-27

### Added
- All character/string-related `uuid` operations now work with `wchar_t`, `char16_t`, 
  `char32_t` and `char8_t` in addition to `char`.
- `uuidof<T>` that wraps `__uuidof` on Windows and can also be used portably

### Fixed
- Addressed false positive compiler warnings in code that could appear in certain CMake build configurations.
- `clang-cl` is now fully supported by CMake build and does not generate deluge of warnings

## [1.7] - 2025-05-11

### Fixed
- Added missing includes reported by Clang in C++23 mode

## [1.6] - 2025-04-09

### Added
- New `MUUID_USE_FMT` macro. Set to 1 to force usage of `fmt` library. When set the compilation 
  will error out if `fmt` is not present. 

### Changed
- Detection of `fmt` library presence is now more robust. 

## [1.5] - 2025-03-27

### Changed
- v6 generation now follows "SHOULD" rather than "MAY" approach of RFC 9562. The `node_id` field
  is filled with random data and `clock_seq` is used to handle monotonicity within the same clock tick.
  This results in significant performance gains
- v6 and v7 generation is no longer synchronized between different threads by default. 

## [1.4] - 2025-03-07

### Added
- Significant performance improvements for time based UUID generation
- Significant performance improvements for random (v4) UUID generation

### Changed
- The random number generator using in the library is now cryptographically secure ChaCha20 instead of mt19937

### Fixed
- Clock sequence of v1 UUIDs always starting from 0
- Possible race condition in internal random generator usage

## [1.3] - 2025-02-27

### Added
- Ability to supply clock persistence and/or cross process coordination for time based (v1, v6 and v7) UUID generation
- Ability to control node id (e.g. Mac address) for time_based and reordered_time_based UUID generation
- Conversions to/from Apple's CFUUID/CFUUIDBytes
- Conversions to/from Windows GUID

### Fixed
- Incorrect behavior of per-thread singletons in case of nested process forks
- Multithreading support is now better detected on older compilers

## [1.2] - 2025-02-12

### Fixed
- `MUUID_NO_TESTS` CMake option no longer incorrectly prevents CMake install
- CMake installed exports now actually work

### Changed
- Tests are no longer included in `all` CMake target and so aren't built unless specifically requested

## [1.1] - 2025-01-23

### Fixed
- `fmt::formatter` no longer relies on `std::formatter` to function

## [1.0] - 2025-01-22

### Added
- Initial version

[1.0]: https://github.com/gershnik/modern-uuid/releases/v1.0
[1.1]: https://github.com/gershnik/modern-uuid/releases/v1.1
[1.2]: https://github.com/gershnik/modern-uuid/releases/v1.2
[1.3]: https://github.com/gershnik/modern-uuid/releases/v1.3
[1.4]: https://github.com/gershnik/modern-uuid/releases/v1.4
[1.5]: https://github.com/gershnik/modern-uuid/releases/v1.5
[1.6]: https://github.com/gershnik/modern-uuid/releases/v1.6
[1.7]: https://github.com/gershnik/modern-uuid/releases/v1.7
[1.8]: https://github.com/gershnik/modern-uuid/releases/v1.8
[1.9]: https://github.com/gershnik/modern-uuid/releases/v1.9
[1.10]: https://github.com/gershnik/modern-uuid/releases/v1.10
[2.0]: https://github.com/gershnik/modern-uuid/releases/v2.0
[2.1]: https://github.com/gershnik/modern-uuid/releases/v2.1
