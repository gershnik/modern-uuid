# Differences from other UUID libraries

There are two well-known libraries commonly used to handle UUIDs: `libuuid` from [util-linux](https://github.com/util-linux/util-linux) and
[Boost.Uuid](http://boost.org/libs/uuid). Both are very good libraries, but have, <ins>**at the time of this writing**</ins> (03-2025), limitations 
and/or trade-offs that I found inconvenient or annoying and which `modern-uuid` was created to address. In particular:

## libuuid

Portability. `libuuid` is only really portable to Linux and BSD. It doesn't work on Windows. It hasn't even been [working on Mac](https://github.com/util-linux/util-linux/issues/2873) for almost a year now without patches. That's two major platforms out there. 

Second, while its time-based generation algorithms are very robust and correct they assume that the system clock ticks once a microsecond.
This assumption is not necessarily true. For example on WASM the clock ticks with a millisecond precision. This results in UUID generation 
being very slow on that platform and the results very predictable.

Lastly, `libuuid` is a C library. It doesn't really help C++ code to actually manipulate UUIDs as first class objects. For that one 
needs to either write a custom UUID class or use a different library.

## Boost.Uuid

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

