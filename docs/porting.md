# Port rules for the engine code

`engine/` is the Descent 1 source release (Watcom C 9.5, Microsoft assembler, DOS).
These rules apply to each change that makes the code build on a modern system.

## Build

- The build system is CMake with Ninja. The compilers are GCC and Clang. Keep the code correct for MSVC where the cost is low.
- The language is C99 with GNU extensions. Do not use C++.
- The targets are 64-bit. Do not store a pointer in an `int` or a `fix`.
- Each library directory has its own `CMakeLists.txt`. The library target name is `d_<directory>`, for example `d_fix`.
- Each library links `d_common` and the libraries that it uses. `d_common` supplies the compiler flags and `engine/includes`.
- `engine/CMakeLists.txt` adds a directory when it contains a `CMakeLists.txt` file. Do not change that file to add a library.
- A test program has the file name `test_<name>.c`. Register it with `add_test`.

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

## Types

- `fix` is `int32_t`. `fixang` is `int16_t`. Use `int64_t` for a 64-bit intermediate result.
- `long` is 32 bits on Windows and 64 bits on Linux. Replace `long` with a fixed size type where the size matters.
- Plain `char` is unsigned (`-funsigned-char`), as in Watcom C.
- Signed overflow wraps (`-fwrapv`).
- A structure that the code reads from a file or writes to a file must not depend on compiler padding. Read and write each field, or prove the layout with a `_Static_assert` on `sizeof` and `offsetof`.

## Assembly

- Replace each `.asm` file with a `.c` file of the same name. Keep the `.asm` file in the tree as the reference. Do not build it.
- Replace each `#pragma aux` function with a C function or a `static inline` function.
- The C code must give the same results as the assembly, bit for bit, for the fixed point math. Physics and level geometry depend on it.
- Where the assembly depends on a processor exception (divide overflow), state the new behavior in a comment.
- Do not copy code from other Descent ports. Write the C code from the assembly in this tree.

## Hardware and DOS

- Only `engine/platform/` calls SDL. The engine code includes `platform.h` and does not include SDL headers.
- Remove the use of `dos.h`, `conio.h`, `i86.h`, `bios.h`, DPMI calls, port I/O, and interrupt handlers. Call the platform layer.
- The game draws into an 8-bit frame in memory. `plat_present` shows the frame with the current palette.

## Edits

- Keep the original code and comments where they still apply. Make the smallest change that is correct.
- Mark a changed block with a comment that starts with `ASCENT:` and gives the reason.
- Keep the Parallax license header at the top of each original file.
- A new file starts with a short comment that says what it replaces.
- The original files use CRLF line ends and tabs. Keep them.
- `engine/lib/` is an old copy of the headers. It is not in the include path. Do not use it and do not edit it.
- The header in the directory of a library is the correct header for that library.
