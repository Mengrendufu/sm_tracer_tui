# Windows libserialport build

Source: https://github.com/Mengrendufu/libserialport

Branch: `fix/windows-rx-empty-wakeup`

Commit: `29937e8d0cd4a83d691cee9de55c18de0f89f683`

Build instructions and regression coverage:
https://github.com/Mengrendufu/libserialport/blob/29937e8d0cd4a83d691cee9de55c18de0f89f683/WINDOWS_RX_FIX.md

Built with MSYS2 UCRT64 GCC 16.1.0, `-O2 -g`, shared library enabled and
static library disabled. The fix rearms completed RX notifications after an
empty read, preventing repeated immediate wakeups. The application API and
all 79 DLL exports are unchanged. The checked-in header is byte-identical to
the source header at this commit.

SHA-256:

```text
6a73ea88bd2eb1ba41beb7fdffeba943deabf55ce729f88e23a335b1b95f573e  libserialport-0.dll
50f041f398e73fa3ac83fb5dc089ac17d9e376d0b353fef64d2a66029bda5eea  libserialport.dll.a
```

The DLL uses Windows UCRT. `make check` passed both upstream timing and
Windows RX wait regression tests; the regression uses deterministic Win32
I/O mocks. The Windows application passed all 17 CTest tests. The user
confirmed that the reported CPU issue was resolved in their Windows setup.
