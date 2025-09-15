# mimalloc-lite

This version is simplified from Daan Leijen and the Microsoft Research team's [mimalloc](https://github.com/microsoft/mimalloc/).

- Linux-only support (removed macOS, Windows, Apple, WASI)
- Assumes GCC/Clang (removed MSVC and other compiler branches)
- Removed `MI_PADDING`, `MI_SECURE`, `MI_ENCODE_FREELIST`, and `MI_DEBUG>2`
- Renamed obsoleted malloc family functions (e.g. `pvalloc`, `cfree`)
- Removed additional interceptors like strdup and realpath. Their libc implementations typically call the interceptable malloc.
- Removed C++ new/delete interceptors
- Removed `mi_register_deferred_free`
