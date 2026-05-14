# Repository Guidelines

## Project Structure & Module Organization
Application code resides in `src/`, split by responsibility: `main.c` coordinates runtime, `border.c`, `windows.c`, and `animation.c` draw and animate frames, while `events.c` and `mach.c` handle macOS event taps and system calls. Shared headers sit under `src/misc/` to isolate platform helpers such as `ax.h` and `connection.h`. Assets like screenshots are under `images/`, and additional prose lives in `docs/`.

## Build, Test, and Development Commands
- `make all` builds the optimized `bin/borders` binary with the AppKit, CoreVideo, and SkyLight frameworks.
- `make debug` compiles an unoptimized `bin/debug` with `-DDEBUG` for local tracing.
- `make asan` produces an AddressSanitizer build, runs it immediately, and is the quickest way to surface memory issues.
- `make clean` clears the `bin/` directory; run it before switching toolchains to avoid stale binaries.

## Coding Style & Naming Conventions
Code is written in C99 and formatted with four-space indentation and aligned initializers for readability. Keep file-local helpers `static`, prefer descriptive snake_case for functions and variables, and reserve ALL_CAPS for compile-time constants or macros. Include project headers before system headers to ensure local overrides compile first.

## Testing Guidelines
There is no dedicated test harness; rely on targeted binaries. Start with `make asan` to catch memory regressions, then launch `bin/borders` against a live desktop session to verify focus changes, shadow updates, and configuration reloads. Record reproduction steps for any graphical glitches and capture logs from the debug build if you add assertions.

## Commit & Pull Request Guidelines
Follow the existing short, imperative subjects (e.g., `use dlsym to link radii function`). Group related changes per commit, and bump version numbers separately to keep release management clear. Pull requests should summarize the visible change, reference impacted modules, note manual test scenarios (including sanitizer runs), and attach updated screenshots when window chrome changes.
