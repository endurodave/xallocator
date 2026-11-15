# allocator

A copy of [Allocator](https://github.com/endurodave/Allocator) 1.2.0 (its `src/` folder), used by this repository's CMake build.

The PlatformIO and Arduino packages don't include it: they depend on Allocator instead, so a project can use xallocator and Allocator together without duplicate symbols.
