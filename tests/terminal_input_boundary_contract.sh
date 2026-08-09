#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
app_source="$root/application/ui/thread/ui_terminal_input.c"
port_header="$root/ports/terminal_input/terminal_input_platform.h"

test -f "$port_header"
test -f "$root/ports/terminal_input/terminal_input_platform_posix.c"
test -f "$root/ports/terminal_input/terminal_input_platform_win32.c"

if grep -q '_WIN32\|GetStdHandle\|GetConsoleScreenBufferInfo' \
    "$app_source"
then
    exit 1
fi

grep -q 'Component: TerminalInputPlatform' "$port_header"
grep -q 'TerminalInputPlatform_init' "$port_header"
grep -q 'TerminalInputPlatform_deinit' "$port_header"
grep -q 'TerminalInputPlatform_waitObject' "$port_header"
grep -q 'TerminalInputPlatform_read' "$port_header"

grep -q 'TerminalInputPlatform_init' "$app_source"
grep -q 'TerminalInputPlatform_deinit' "$app_source"
grep -q 'TerminalInputPlatform_waitObject' "$app_source"
grep -q 'TerminalInputPlatform_read' "$app_source"
