# =============================================================================
# Native Windows GCC toolchain using the MSYS2 UCRT64 environment.
# Set MSYS2_ROOT to override the default installation directory.
# =============================================================================

if(DEFINED ENV{MSYS2_ROOT})
    file(TO_CMAKE_PATH "$ENV{MSYS2_ROOT}" MSYS2_ROOT)
else()
    set(MSYS2_ROOT "C:/msys64")
endif()

set(UCRT64_ROOT "${MSYS2_ROOT}/ucrt64")

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER "${UCRT64_ROOT}/bin/gcc.exe")
set(CMAKE_RC_COMPILER "${UCRT64_ROOT}/bin/windres.exe")

set(CMAKE_FIND_ROOT_PATH "${UCRT64_ROOT}")
list(PREPEND CMAKE_PREFIX_PATH "${UCRT64_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS
    "-Wall -Wextra -Wpedantic"
    CACHE STRING "Common C flags"
    FORCE
)
