#include "terminal_input_geometry_priv.h"

int main(void) {
    int failed = 0;
    TerminalInputGeometry geometry = {.rows = 40U, .cols = 120U};

    failed += TerminalInputGeometry_update(
        &geometry,
        (TerminalInputGeometry){.rows = 40U, .cols = 120U}) ? 1 : 0;
    failed += TerminalInputGeometry_update(
        &geometry,
        (TerminalInputGeometry){.rows = 41U, .cols = 120U}) ? 0 : 1;
    failed += (geometry.rows == 41U) && (geometry.cols == 120U) ? 0 : 1;
    failed += TerminalInputGeometry_update(
        &geometry,
        (TerminalInputGeometry){.rows = 41U, .cols = 121U}) ? 0 : 1;
    failed += (geometry.rows == 41U) && (geometry.cols == 121U) ? 0 : 1;
    failed += TerminalInputGeometry_update(
        &geometry,
        (TerminalInputGeometry){.rows = 41U, .cols = 121U}) ? 1 : 0;

    return failed == 0 ? 0 : 1;
}
