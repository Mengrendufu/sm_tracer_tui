#!/usr/bin/env bash
#============================================================================
# Copyright (C) 2026 Sunny Matato
#
# This program is free software. It comes without any warranty, to
# the extent permitted by applicable law. You can redistribute it
# and/or modify it under the terms of the Do What The Fuck You Want
# To Public License, Version 2, as published by Sam Hocevar.
# See http://www.wtfpl.net/ for more details.
#============================================================================
#============================================================================
# Build the native Windows (MSYS2 UCRT64) target from a WSL checkout.
#
# The Windows toolchain cannot build into the WSL filesystem: CMake wraps
# every link rule in cmd.exe, and cmd.exe refuses a UNC working directory,
# so the build tree must live on a Windows drive letter while the sources
# stay in WSL. The physical tree therefore stays on a drive, and the
# repository path build/Windows is a symlink to it, so
# build/Windows/<preset> resolves from WSL like the Linux tree does.
#
# Usage:
#   scripts/build_win.sh [debug|release] [--test] [<cmake --build args>]
#
#   scripts/build_win.sh                     # debug configure + build
#   scripts/build_win.sh release             # release configure + build
#   scripts/build_win.sh debug --target sm_tracer_tui
#   scripts/build_win.sh debug --test        # then run ctest
#
# Environment:
#   SM_WIN_BUILD_ROOT  physical build tree root on a Windows drive
#                      (default: /mnt/c/tmp/sm_tracer_tui_win)
#   SM_MSYS2_UCRT64    UCRT64 bin directory
#                      (default: /mnt/c/msys64/ucrt64/bin)
#
# Run the built application from Windows Terminal: the TUI needs a real
# ConPTY console and the copied protocols directory next to the binary.
#============================================================================
set -euo pipefail

preset=debug
run_tests=0
build_args=()
for arg in "$@"; do
    case "$arg" in
        debug|release) preset="$arg" ;;
        --test) run_tests=1 ;;
        *) build_args+=("$arg") ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ucrt64="${SM_MSYS2_UCRT64:-/mnt/c/msys64/ucrt64/bin}"
win_root="${SM_WIN_BUILD_ROOT:-/mnt/c/tmp/sm_tracer_tui_win}"
win_build="$win_root/$preset"
win_link="$root/build/Windows"

if [ ! -x "$ucrt64/cmake.exe" ]; then
    echo "build_win.sh: no cmake.exe under $ucrt64" >&2
    exit 1
fi
export PATH="$ucrt64:$PATH"

if /mnt/c/Windows/System32/tasklist.exe 2>/dev/null \
        | grep -qi 'sm_tracer_tui.exe'
then
    echo "build_win.sh: warning: sm_tracer_tui.exe is running and locks" \
         "the output binary" >&2
fi

mkdir -p "$win_build"
if [ -e "$win_link" ] && [ ! -L "$win_link" ]; then
    echo "build_win.sh: $win_link is a directory, not a symlink. A Windows" >&2
    echo "  build tree cannot live in the WSL filesystem, so remove the" >&2
    echo "  stale tree and rerun:  rm -rf $win_link" >&2
    exit 1
fi
ln -sfn "$win_root" "$win_link"

win_build_win="$(wslpath -w "$win_build")"

# Presets are read from the working directory; the drive-letter -B keeps
# the generated tree out of the UNC source path.
cd "$root"
cmake.exe --preset "$preset" -B "$win_build_win"
cmake.exe --build "$win_build_win" "${build_args[@]}"

if [ "$run_tests" = 1 ]; then
    ctest.exe --test-dir "$win_build_win" --output-on-failure
fi

echo "build tree : build/Windows/$preset -> $win_build"
echo "application: $(wslpath -w "$win_build/bin/sm_tracer_tui.exe")"
