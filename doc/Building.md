# Integrating and configuring modern-uuid

## Integration

### CMake via FetchContent

With modern CMake the easiest way to use this library is

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
  #this picks static or shared library based on your CMake defaults
  modern-uuid::modern-uuid
  #or to pick static explicitly
  #modern-uuid::modern-uuid-static
  #or to pick shared explicitly
  #modern-uuid::modern-uuid-shared
)
```
> ℹ&#xFE0F; _[What is FetchContent?](https://cmake.org/cmake/help/latest/module/FetchContent.html)_

See [CMake settings and targets](#cmake-settings-and-targets) for information about CMake settings that affect the above and available targets.

### CMake from download

Alternatively, you can clone this repository somewhere and do this:

```cmake
add_subdirectory(PATH_WHERE_YOU_DOWNALODED_IT_TO, modern-uuid)
...
target_link_libraries(mytarget
PRIVATE
  #this picks static or shared library based on your CMake defaults
  modern-uuid::modern-uuid
  #or to pick static explicitly
  #modern-uuid::modern-uuid-static
  #or to pick shared explicitly
  #modern-uuid::modern-uuid-shared
)
```

Note that you need to have your compiler to support at least C++20 in order for build to succeed.

See [CMake settings and targets](#cmake-settings-and-targets) for information about CMake settings that affect the above and available targets.

### Building and installing on your system

You can also build and install this library on your system using CMake.

1. Download or clone this repository into SOME_PATH
2. On command line:
```bash
cd SOME_PATH
cmake -S . -B out 
cmake --build out

#Optional
#cmake --build out --target run-tests

#install to /usr/local
sudo cmake --install out
#or for a different prefix
#cmake --install out --prefix /usr
```

See [CMake settings and targets](#cmake-settings-and-targets) for information about CMake settings you can pass to configuration step.

Once the library has been installed it can be used in the following ways:

#### Basic use 

Set the include directory to `<prefix>/include` where `<prefix>` is the install prefix from above.
Add `<prefix>/lib/libmodern-uuid.a` or `<prefix>/lib/libmodern-uuid.so` (`.dylib` on Mac) to your link libraries.

#### CMake package

```cmake
find_package(modern-uuid)

target_link_libraries(mytarget
PRIVATE
  #this picks static or shared library based on your CMake defaults
  modern-uuid::modern-uuid
  #or to pick static explicitly
  #modern-uuid::modern-uuid-static
  #or to pick shared explicitly
  #modern-uuid::modern-uuid-shared
)
```

#### Via `pkg-config`

Add the output of `pkg-config --cflags --libs modern-uuid` to your compiler flags.

Note that the default installation prefix `/usr/local` might not be in the list of places your
`pkg-config` looks into. If so you might need to do:
```bash
export PKG_CONFIG_PATH=/usr/local/share/pkgconfig
```
before running `pkg-config`

### CMake settings and targets
 
There are 3 variables that affect the type of library built:

* `MUUID_SHARED` - if set enables shared library target even if it otherwise wouldn't be enabled
* `MUUID_STATIC` - if set enables static library target even if it otherwise wouldn't be enabled
* [BUILD_SHARED_LIBS](https://cmake.org/cmake/help/latest/variable/BUILD_SHARED_LIBS.html) - see below
* [TARGET_SUPPORTS_SHARED_LIBS](https://cmake.org/cmake/help/latest/prop_gbl/TARGET_SUPPORTS_SHARED_LIBS.html) - see below

If you don't explicitly set either `MUUID_SHARED` or `MUUID_STATIC` the behavior is as follows:

* If `TARGET_SUPPORTS_SHARED_LIBS` is `FALSE` shared lib is not enabled regardless of the following. 
* If the modern-uuid project is a top level project then both variants are enabled.
* Otherwise the enabled variant depends on `BUILD_SHARED_LIBS`.
  If `BUILD_SHARED_LIBS` is `ON` then the shared library target will be enabled. Otherwise - the static one.


You can [set()](https://cmake.org/cmake/help/latest/command/set.html) `MUUID_SHARED`, `MUUID_STATIC` and `BUILD_SHARED_LIBS` in your CMake script prior to 
adding modern-uuid or specify them on CMake command line.

The exposed targets can be:

* `modern-uuid::modern-uuid-static` - the static library
* `modern-uuid::modern-uuid-shared` - the shared library
* `modern-uuid::modern-uuid` - the "default" one. If only one of static/shared variants is enabled, this one points to it. 
  If both variants are enabled then this alias points to `modern-uuid::modern-uuid-shared` if `BUILD_SHARED_LIBS` is `ON` or 
  `modern-uuid::modern-uuid-static` otherwise.  


### Other build systems

If you use a different build system then:

* Set your include path to `inc` 
* Set `MUUID_BUILDING_MUUID` preprocessor macro to 1.
* If building a shared library set `MUUID_SHARED` preprocessor macro to 1.
* Compile the sources under `src` into a static/shared library and add it to your link step
* On some platforms you will need to link with additional libraries:
  * Illumos: `socket`
  * MinGW: `iphlpapi.lib`

Note that you need to have your compiler to use at least C++20 mode in order for build to succeed.

## Configuration options

Whichever method you use you can set the following macros to control the library behavior. These must be set identically when using and building the library.

* `MUUID_SHARED` - set it to 1 if using/building a shared version of the library.
* `MUUID_MULTITHREADED` - set it to 1 or 0 to control support for multiple threads. Normally threading support is detected automatically but setting this macro manually allows you to override automatic detection.
* `MUUID_USE_EXCEPTIONS` - set it to 1 or 0 to control usage of exceptions. Normally exception support is detected automatically but setting this manually allows you to override automatic detection.
* `MUUID_USE_FMT` - set it to 1 to force support for `fmt` library. You must include `<fmt/format.h>` prior to `uuid.h` - otherwise the compilation will fail. (For backward compatibility, if this flag is not set, support for `fmt` will be automatically enabled if `<fmt/format.h>` was included prior to `uuid.h` and disabled otherwise.)
