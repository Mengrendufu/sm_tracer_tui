# =============================================================================
# GCC desktop C toolchain template
# Purpose: define compiler and common compile flags for native GCC projects.
# =============================================================================

# =============================================================================
# ===Compiler.
set(CMAKE_C_COMPILER gcc)

# =============================================================================
# ===Final symbols.
set(CMAKE_C_FLAGS
    "-Wall -Wextra -Wpedantic"
    CACHE STRING "Common C flags"
    FORCE
)
