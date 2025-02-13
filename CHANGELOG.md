# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

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
