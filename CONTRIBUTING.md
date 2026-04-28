# Contributing to Jeode

Jeode is a small project so the process is pretty simple, just open an issue or pull request on [GitHub](https://github.com/Paficent/jeode). The [Codeberg](https://codeberg.org/Paficent/jeode) mirror is read-only.

## Building

You need `i686-w64-mingw32` (MinGW-w64 targeting 32-bit Windows) and standard build tools (`cmake`, `bash`).

```sh
./build.sh         # Development build
./build.sh X.X.X.X # Production build
```

## Code Style

The project uses `clang-format` run it before committing:

```sh
clang-format -i src/**/*.cpp src/**/*.h
```

The [.clang-format](.clang-format) uses modified LLVM style:

- Tabs for indentation, 4-wide
- 120 column limit
- Attached braces (`if (x) {`, not `if (x)\n{`)
- `snake_case` for functions and variables, `PascalCase` for classes and structs
- `#pragma once` for header guards

## Logging

Use spdlog levels correctly:

- **`info`** — What a user should see in production
- **`warn`** — Non-fatal problems (missing files, override conflicts)
- **`error`** — Failures that prevent something from working (hook failures, load errors)
- **`debug`** — Diagnostic detail useful when troubleshooting (addresses, paths)

Always include a `[tag]` prefix: `spdlog::info("[loader] loaded '{}'", id);`

## Pull Requests

- Keep changes focused (one fix or feature per PR)
- Test with the current Steam version of MSM before submitting
- Describe what your change does and why
