#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

for widget in title_bar connection_status_bar input_composer keybar menu; do
    test -f "$root/application/ui/widgets/$widget.h"
    test -f "$root/application/ui/widgets/$widget.c"
done

grep -q 'struct TitleBar' \
    "$root/application/ui/widgets/title_bar.h"
grep -q 'struct ConnectionStatusBar' \
    "$root/application/ui/widgets/connection_status_bar.h"
grep -q 'struct InputComposer' \
    "$root/application/ui/widgets/input_composer.h"
grep -q 'struct Keybar' \
    "$root/application/ui/widgets/keybar.h"
grep -q 'struct Menu' \
    "$root/application/ui/widgets/menu.h"
grep -q 'bool[[:space:]]*dirty;' \
    "$root/application/ui/widgets/text_buffer_view.h"

if grep -R -q 'NcDisp\|mainBufferDirty' \
    "$root/application/ui/hsm" "$root/application/ui/widgets"
then
    exit 1
fi
