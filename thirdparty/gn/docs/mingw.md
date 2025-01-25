# Building and using `gn` with MinGW on Windows

At the time of writing: 2024-04-13
Test environment: Windows 11, intel x86_64 CPU

## Requirements

1.  Install [MSYS2](https://www.msys2.org/)
1.  Start a Terminal windows for MSYS2's MinGW development by opening one of `clang32.exe`, `clang64.exe`, `clangarm64.exe`,`mingw32.exe`, `mingw64.exe`, `ucrt64.exe`.
1.  To download clang toolchain, Ninja and git programs, run command: `pacman -S mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-ninja git`

## Build `gn`

1.  To clone `gn` source code, run command: `git clone https://gn.googlesource.com/gn`
1.  Run command: `cd gn`
1.  Run command: `build/gen.py --platform mingw`  
    This configuration generates a static executable that does not depend on MSYS2, and can be run in any development environment.  
    Use `--help` flag to check more configuration options.  
    Use `--allow-warning` flag to build with warnings.
1.  Run command: `ninja -C out`

## Testing

1.  Run command: `out/gn --version`
1.  Run command: `out/gn_unittests`

> Notes:
>
> For "mingw-w64-clang-x86_64-toolchain" in the clang64 environment, g++ is a copy of clang++.
>
> The toolchain that builds `gn` does not use vendor lock-in compiler command flags,
> so `gn` can be built with these clang++, g++.
>
> However the build rules in [examples/simple_build](../examples/simple_build/) require GCC-specific compiler macros, and thus only work in the `ucrt64` MSYS2 development environment.
