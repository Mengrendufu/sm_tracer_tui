# sm_tracer_tui

A C99 terminal serial tracer built with notcurses, `sm_hsm`, and the `sm_sst`
Active Object kernel. Linux and native Windows UCRT64 builds use the same
pinned patched notcurses source.

## Clone

The project pins third-party source repositories as Git submodules:

```sh
git clone --recurse-submodules <repository-url>
```

For an existing checkout:

```sh
git submodule update --init --recursive
```

## Platform prerequisites

The repository supplies the patched notcurses source. Platform compilers and
notcurses' lower-level development dependencies remain part of the build
environment.

### Linux

On Ubuntu or Debian:

```sh
sudo apt install \
    build-essential cmake ninja-build pkg-config \
    libncurses-dev libunistring-dev libdeflate-dev
```

### Windows UCRT64

Run the following from an MSYS2 UCRT64 shell:

```sh
pacman -S \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-ncurses \
    mingw-w64-ucrt-x86_64-libunistring \
    mingw-w64-ucrt-x86_64-libdeflate
```

Add the MSYS2 `ucrt64/bin` directory to the Windows `PATH`. The shared CMake
presets discover `gcc` and `windres` from that environment. Run the project
presets from PowerShell.

## Build

Linux or Windows debug build:

```sh
cmake --preset debug
cmake --build --preset build-debug
```

Run the application on either platform with:

```sh
cmake --build --preset run-debug
```

Build trees are separated automatically under `build/Linux/debug` and
`build/Windows/debug`. Run tests from the corresponding directory:

```sh
ctest --test-dir build/Linux/debug --output-on-failure
ctest --test-dir build/Windows/debug --output-on-failure
```

## notcurses dependency

`3rd_party/notcurses` is pinned to the project fork of notcurses 3.0.17. Its
Windows terminal-mode and binary-stdin fixes are guarded by the platform
implementation; Linux and Windows therefore build one authoritative source
tree without maintaining divergent copies.

Only the notcurses core static target is linked into the application. Demo
programs, multimedia support, C++, FFI, documentation, and the upstream test
suite are disabled in this build.
