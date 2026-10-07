# TommyDS Development Guide

This document provides guidance for agentic coding agents working on the TommyDS codebase.

## Project Structure

### Core Source Files (`tommyds/`)

TommyDS is a C library of data structures. Public APIs and their Doxygen documentation are defined in the headers, with implementations in the corresponding C files.

| Modules | Purpose |
|---|---|
| `tommy.h` / `tommy.c` | All-in-one header and compilation unit including all library modules |
| `tommytypes.h` | Shared types, node definitions, callbacks, and portability helpers |
| `tommyalloc.h` / `tommyalloc.c` | Allocator of fixed-size blocks, used by the trie |
| `tommyarray.h` / `tommyarray.c`, `tommyarrayof.h` / `tommyarrayof.c` | Dynamic arrays using exponentially growing segments, storing pointers or arbitrary-size elements |
| `tommyarrayblk.h` / `tommyarrayblk.c`, `tommyarrayblkof.h` / `tommyarrayblkof.c` | Dynamic arrays using fixed-size blocks, storing pointers or arbitrary-size elements |
| `tommylist.h` / `tommylist.c`, `tommychain.h` | Doubly linked lists and sorting, with internal chain helpers |
| `tommyhashtbl.h` / `tommyhashtbl.c` | Fixed-size chained hash table |
| `tommyhashdyn.h` / `tommyhashdyn.c` | Dynamic chained hash table |
| `tommyhashlin.h` / `tommyhashlin.c` | Linear chained hash table with incremental resizing |
| `tommyhash.h` / `tommyhash.c` | Hash functions for integers, strings, and byte buffers |
| `tommytree.h` / `tommytree.c` | AVL tree ordered by a comparison callback |
| `tommytrie.h` / `tommytrie.c` | Cache-optimized trie using an external allocator |
| `tommytrieinp.h` / `tommytrieinp.c` | In-place trie requiring no external allocation |

### Tests, Benchmarks, and Documentation

- `check.c`: Correctness tests for the library, compiled into `tommycheck`.
- `benchmark.cc`: C++ benchmark comparing TommyDS with other data structure implementations, compiled into `tommybench`.
- `benchmark/lib/`: Third-party libraries used by the benchmark.
- `benchmark/data/`, `benchmark/gr_*.gnu`, `benchmark/gr_all.sh`, and `benchmark/gr_all.bat`: Benchmark datasets and graph generation scripts.
- `tommy.doxygen`, `tommyweb.doxygen`, `tommy.css`, and the HTML header/footer files: Doxygen configuration and presentation assets; generated documentation goes into `doc/` or `web/`.
- `README`, `INSTALL`, `HISTORY`, `COPYING`, and `LICENSES/`: Project overview, build instructions, release history, and licensing.

### Build System

- `Makefile`: Builds `tommyds/tommy.c` into `tommy.o` and links `tommycheck`; the `bench` target builds `tommybench` from `benchmark.cc`.
- Run `make -j$(nproc) check` to build and execute the correctness tests, and `make -j$(nproc) bench` to build the benchmark.
- The `coverage`, `valgrind`, `callgrind`, and `cachegrind` targets provide coverage and runtime analysis; `graph` regenerates benchmark graphs.
- Run `make -j$(nproc) doc` or `make -j$(nproc) web` to regenerate HTML documentation with Doxygen.
- `benchmark.sln` / `benchmark.vcxproj`: Visual Studio solution and project for the benchmark.
- `makebench.sh`, `makecov.sh`, and `makescan.sh`: Helper scripts for benchmarks, coverage, and Coverity analysis.
- `uncrustify.cfg` and `makeuncrustify.sh`: Formatting configuration and helper script for `tommyds/*.c` and `tommyds/*.h`.
- Always use parallel compilation with `make -j$(nproc)` instead of plain `make`
- Create temporary files and directories under `/tmp/tommyds/`.

## Code Style Guidelines

#### Design Simplicity

- **Simplicity First**: Always prefer the simplest and cleanest implementation that fully satisfies the demonstrated requirements. Do not introduce extra state, abstractions, conditions, or defensive handling unless a concrete requirement or verified failure mode needs them. When replacing existing logic, preserve its narrow established semantics instead of broadening it through inferred state from unrelated subsystems.
- **Smallest Necessary Change** Harmless existing behavior or artifacts do not justify refactoring. Always make the smallest change necessary to satisfy the requirement.
- **Binary Compatibility**: Compatibility with previously compiled clients is not required. Do not add compatibility shims solely to preserve old exported symbols.

#### Code Style

- **Language**: The codebase uses **C99 standard** (not C11/C++)
- **Format**: Enforce via `uncrustify -c uncrustify.cfg --no-backup *.c *.h`
- **Naming**: Snake_case for functions, UPPER_CASE for macros/constants
- **Indentation**: Tabs for indentation, no alignment (existing codebase style)
- **Comments**: Use `/** ... */` for multiline top comments preceding declarations and `/* ... */` for multiline comments within code. Use `/* first letter lowercase */` for single-line inline notes.
- **Critical Comments**: Always add a comment at non-obvious critical points, especially around data-integrity invariants, crash recovery, fallback behavior, concurrency, and fatal versus best-effort error handling. Explain why the logic is required, not merely what the code does.
- **Headers**: All `.h` files have include guards (`#ifndef __NAME_H`)
- **Preferences**: Use 0 instead of NULL and '\0'
- **Preferences**: Use prefix ++variable and --variable instead of postfix variable++ and variable-- where both are equivalent
- **Safety Checks**: Avoid adding safety checks for conditions that never happen
- **Type Casts**: Avoid type casts unless strictly necessary for compilation with -Werror and unavoidable by other mean
- **Stack Over Heap**: Prefer stack-allocated fixed-size buffers over dynamic memory allocation (`malloc`/`free`) whenever the upper bound is small, fixed, and known at compile time.
- **Commit Messages**: Every time a change is done, a single line commit description should be provided for that change
- **Git Commits**: Never commit changes to git.
