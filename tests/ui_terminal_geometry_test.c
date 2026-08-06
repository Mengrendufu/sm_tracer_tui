#include "thread/ui_terminal_geometry_priv.h"

int main(void) {
    int failed = 0;
    UI_TerminalGeometry geometry = {.rows = 40U, .cols = 120U};

    failed += UI_TerminalGeometry_update(
        &geometry,
        (UI_TerminalGeometry){.rows = 40U, .cols = 120U}) ? 1 : 0;
    failed += UI_TerminalGeometry_update(
        &geometry,
        (UI_TerminalGeometry){.rows = 41U, .cols = 120U}) ? 0 : 1;
    failed += (geometry.rows == 41U) && (geometry.cols == 120U) ? 0 : 1;
    failed += UI_TerminalGeometry_update(
        &geometry,
        (UI_TerminalGeometry){.rows = 41U, .cols = 121U}) ? 0 : 1;
    failed += (geometry.rows == 41U) && (geometry.cols == 121U) ? 0 : 1;
    failed += UI_TerminalGeometry_update(
        &geometry,
        (UI_TerminalGeometry){.rows = 41U, .cols = 121U}) ? 1 : 0;

    return failed == 0 ? 0 : 1;
}
