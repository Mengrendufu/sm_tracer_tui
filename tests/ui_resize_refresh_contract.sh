#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_file="$root/application/ui/thread/ui.c"

grep -q 'r == NCKEY_RESIZE' "$source_file"
grep -q 'UI_resizePending_ = true;' "$source_file"
grep -q 'if (UI_resizePending_)' "$source_file"
grep -q 'notcurses_refresh(UI_nc_' "$source_file"

refresh_line=$(grep -n 'notcurses_refresh(UI_nc_' "$source_file" |
               head -n 1 | cut -d: -f1)
flush_line=$(grep -n 'SM_UI_flush();' "$source_file" |
             head -n 1 | cut -d: -f1)
test "$refresh_line" -lt "$flush_line"
