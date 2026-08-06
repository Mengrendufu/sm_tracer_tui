# =============================================================================
# Native GCC toolchain for Linux and Windows UCRT64.
# The selected compiler and its platform root come from the caller's PATH.
# =============================================================================

# =============================================================================
# ===Compiler.
find_program(SM_GCC_EXECUTABLE NAMES gcc REQUIRED)
set(CMAKE_C_COMPILER "${SM_GCC_EXECUTABLE}")

if(CMAKE_HOST_WIN32)
    set(CMAKE_SYSTEM_NAME Windows)

    find_program(SM_WINDRES_EXECUTABLE NAMES windres REQUIRED)
    set(CMAKE_RC_COMPILER "${SM_WINDRES_EXECUTABLE}")

    get_filename_component(UCRT64_BIN_DIR
        "${SM_GCC_EXECUTABLE}" DIRECTORY)
    get_filename_component(UCRT64_ROOT
        "${UCRT64_BIN_DIR}" DIRECTORY)
    set(UCRT64_ROOT "${UCRT64_ROOT}" CACHE PATH
        "Root of the active MSYS2 UCRT64 environment" FORCE)

    set(CMAKE_FIND_ROOT_PATH "${UCRT64_ROOT}")
    list(PREPEND CMAKE_PREFIX_PATH "${UCRT64_ROOT}")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
elseif(NOT CMAKE_HOST_UNIX)
    message(FATAL_ERROR
        "The native GCC toolchain supports only Linux and Windows hosts")
endif()

# =============================================================================
# ===Final symbols.
set(CMAKE_C_FLAGS
    "-Wall -Wextra -Wpedantic"
    CACHE STRING "Common C flags"
    FORCE
)
