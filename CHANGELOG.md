# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

## [1.3] - 2025-02-27

## Added
- Ability to supply clock persistence and/or cross process coordination for time based (v1, v6 and v7) UUID generation
- Ability to control node id (e.g. Mac address) for time_based and reordered_time_based UUID generation
- Conversions to/from Apple's CFUUID/CFUUIDBytes
- Conversions to/from Windows GUID

## Fixed
- Incorrect behavior of per-thread singletons in case of nested process forks
- Multithreading support is now better detected on older compilers

## [1.2] - 2025-02-12

## Fixed
- `MUUID_NO_TESTS` CMake option no longer incorrectly prevents CMake install
- CMake installed exports now actually work

## Changed
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
