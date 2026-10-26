# libserialport notice and corresponding source

sm_tracer_tui uses libserialport through `libserialport-0.dll`.
The library and its use are covered by the GNU Lesser General Public License,
version 3 or (at your option) any later version (LGPL-3.0-or-later).
The accompanying `COPYING.LESSER` contains LGPLv3; `COPYING` contains GPLv3,
whose terms LGPLv3 incorporates. These licenses apply to libserialport, not
automatically to the application's original code.

In the application distribution, these files and the source archive are under
`third_party/libserialport/` beside the executable. In the repository they are
under `3rd_party/libserialport/windows/distribution/`.

## Copyright and attribution

Copyright notices from the Windows library sources and public header:

```text
Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
Copyright (C) 2010-2015 Uwe Hermann <uwe@hermann-uwe.de>
Copyright (C) 2013-2015, 2019 Martin Ling <martin-libserialport@earth.li>
Copyright (C) 2013 Matthias Heidbrink <m-sigrok@heidbrink.biz>
Copyright (C) 2014 Aurelien Jacobs <aurel@gnuage.org>
Copyright (C) 2026 Sunny Matato (Windows RX fix and regression test)
```

The unmodified upstream `AUTHORS` file is included. Individual files in the
source archive retain their original copyright and license notices.

## Exact source for the supplied Windows DLL

- Upstream: https://github.com/sigrokproject/libserialport
- Fork: https://github.com/Mengrendufu/libserialport
- Commit: `29937e8d0cd4a83d691cee9de55c18de0f89f683`
- Local archive: `libserialport-29937e8-source.tar.gz`
- Modification date: 2026-10-27.
- Modification: rearm completed Windows RX notifications after empty reads,
  with a regression test and build instructions added. This prevents repeated
  immediate wakeups after reception stops.

The archive contains every tracked file at that commit, including the modified
library, headers, build inputs, tests, and `WINDOWS_RX_FIX.md`. It is supplied
locally so obtaining the corresponding source does not require GitHub access.
Extract it and follow `WINDOWS_RX_FIX.md` to build with MSYS2 UCRT64 GCC.

SHA-256:

```text
cedee9245b86da2a4e98443584188d869e8e3a8840cf475b329839c6c247720b  libserialport-29937e8-source.tar.gz
6a73ea88bd2eb1ba41beb7fdffeba943deabf55ce729f88e23a335b1b95f573e  libserialport-0.dll
```

## Replacing the library

You may modify this library under its license and use an interface-compatible
64-bit DLL. Close the application, back up `libserialport-0.dll` beside the
executable, replace it with your build, then restart the application. No
application source code or application relinking is needed for this mechanism.
Modification of the library and reverse engineering for debugging such
modifications are permitted under LGPLv3; application terms must not restrict
those rights. See the full licenses for the applicable conditions and warranty
disclaimer.

When redistributing this Windows DLL, retain this notice, both license texts,
and the corresponding source archive with the application. This source archive
identifies the Windows DLL above; it is not a provenance claim for the
separately vendored Linux shared library or an audit of other dependencies.
