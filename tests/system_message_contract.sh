#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if grep -R -q \
    'SerialPortThread initialized\.\|BlinkyTOPInit\.\|SpMngr initialized\.' \
    "$root/application"
then
    exit 1
fi

grep -q '\[SYS_INFO\]> Requesting initial protocol catalog refresh\.' \
    "$root/application/aos/sp_mngr/sp_mngr.c"
grep -q '\[SYS_INFO\]> Requesting initial serial port refresh\.' \
    "$root/application/aos/sp_mngr/sp_mngr.c"
if grep -q 'Discovered .*protocol file' \
    "$root/application/aos/sp_mngr/sp_mngr.c"
then
    exit 1
fi
if grep -q '"SpMngr:' \
    "$root/application/aos/sp_mngr/sp_mngr.c"
then
    exit 1
fi
grep -q '\[SYS_INFO\]> Serial ports:' \
    "$root/application/ui/hsm/sm_ui.c"
grep -q '\[SYS_INFO\]> Protocol files:' \
    "$root/application/ui/hsm/sm_ui.c"
if grep -q 'SM_UI_mainBuffer_pushSysInfo_\|pushText_(me, "\\n"' \
    "$root/application/ui/hsm/sm_ui.c"
then
    exit 1
fi
