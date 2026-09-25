# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

JankyBorders (`borders`) is a small C99 daemon for macOS 14.0+ that draws colored borders around windows. It does **not** use the Accessibility API for tracking windows. Instead it talks directly to the WindowServer through the private **SkyLight** framework (`SLS*` / `SL*` functions), creating its own overlay windows positioned relative to each target window. The user-facing option reference is the man page source in `docs/borders.1.scd` (scdoc), rendered to `docs/borders.1`.

## Build

```sh
make          # optimized build -> bin/borders
make debug    # -O0 -DDEBUG -> bin/debug (enables debug() logging and a raw SkyLight event dump)
make asan     # ASan/UBSan build -> bin/debug, then runs it right away
make run      # stop any local build, rebuild debug, run bin/debug in the foreground (Ctrl+C stops it)
make stop     # kill any running bin/borders or bin/debug (never the Homebrew /opt/homebrew/bin/borders)
make test     # build and run tests/color_style_test.c (color parsing and glow blending)
make clean    # deletes bin/ only; it does not stop a running instance
```

`make test` is the only automated test. It `#include`s `src/parse.c` directly, so it covers `parse_settings` and the color helpers in `misc/drawing.h`. There's no linter. For anything that draws or tracks windows, use `make run` and look at real windows. For a debugger, use `lldb ./bin/debug`, then `run`.

Only one instance can own the server port. If one is already running, a new binary forwards its arguments to it and exits ("A borders instance is already running..." when given no args). The debug build's process is named `debug`, not `borders`, so `pkill borders`, Activity Monitor and Raycast searches for "borders" won't find it. Use `make stop` or `pgrep -fl bin/debug`. A Homebrew `borders` started from yabairc or `brew services` also holds the port and must be stopped separately.

New `.c` files must be added to `FILES` in the `makefile`. The headers under `src/misc/` are header-only (`static inline` helpers and `extern` declarations for private APIs), so they don't need makefile entries.

When bumping the version, update `MAJOR`/`MINOR`/`PATCH` in `src/main.c`.

## Architecture

### Single-instance client/server over Mach
`main.c` looks up the bootstrap service `git.felix.borders` (`BS_NAME` in `mach.h`):
- **Found and args are valid:** it acts as a client. It packs argv as NUL-separated strings, sends them with `mach_send_message`, and exits. This is how `borders key=value ...` updates a running instance.
- **Not found:** it becomes the server. It registers the port (`mach_server_begin`), enumerates existing windows, and runs the CFRunLoop. If it got no args, it runs `~/.config/borders/bordersrc` (`execute_config_file`). That file usually calls `borders ...` again, which then goes down the client path.

`message_handler` in `main.c` parses incoming settings into a copy of `g_settings` and then applies them. With `apply-to=<wid>`, the settings go into that border's `setting_override` only. Otherwise they replace `g_settings`, and are also re-parsed into any borders that already have overrides.

### Settings and update masks
`parse_settings` (`parse.c`) is shared by the CLI and runtime paths. It returns a bitmask (`BORDER_UPDATE_MASK_*` in `parse.h`) that picks how much work to do: redraw active only, inactive only, all, or `RECREATE_ALL`. `RECREATE_ALL` destroys and re-creates every border window, and is needed for `hidpi` and `blacklist`/`whitelist` changes. A new option needs a parse branch and the right mask bit, and should be documented in `docs/borders.1.scd`.

A color is a `struct color_style` (`border.h`): `stype` is `COLOR_STYLE_SOLID` or `COLOR_STYLE_GRADIENT`, and `glow` is a separate flag, so `glow(gradient(...))` works. `parse_color` requires the whole token to match (`%n` check) and leaves the old value untouched on failure. For a gradient glow, `border_draw` uses one shadow color, the alpha-weighted blend of the two endpoints (`colors_mix` in `misc/drawing.h`). `background_color` rejects gradients.

`border_get_settings()` returns either the per-border override or `g_settings`. It asserts it's on the main thread.

### Event flow
- `events.c` registers `SLSRegisterNotifyProc` callbacks for window create, destroy, move, resize, reorder, hide, unhide, title, and level events, plus space changes and front-app changes. Per-window notifications only fire for windows listed in `SLSRequestNotificationsForWindows`. That's why `windows_update_notifications` must run whenever the window table changes.
- Focus is resolved lazily. Handlers schedule `windows_determine_and_focus_active_window` with `DELAY_ASYNC_EXEC_ON_MAIN_THREAD` (a usleep on a background queue, then dispatch to main), because SkyLight state isn't settled yet when the event arrives. The front window comes from SkyLight (`get_front_window`) by default, or from AX when `ax_focus=on`.
- On a space change, `windows_draw_borders_on_current_spaces` re-checks every visible window and creates any borders that are missing.

### Windows and borders
- `g_windows` is a `struct table` (`hashtable.c`, a simple chained hashtable) that maps target window IDs (`uint32_t`) to `struct border*`. The same table type also stores the black/white lists, keyed by process name.
- `window_suitable()` in `misc/window.h` decides which windows get a border, based on tags and attributes: top-level, document-like, not attached, and not ignoring window cycling. Blacklist/whitelist filtering happens in `windows_window_create`.
- Each `struct border` (`border.h`) owns its own SkyLight connection (`SLSNewConnection`), an overlay window (`wid`), and a CGContext. It has a recursive mutex, because border operations can run from dispatch queues (`border_move`, and the teardown in `border_destroy`).
- `border_update_internal` does the work: it computes bounds (window frame plus `border_width` plus `BORDER_PADDING`), reshapes the window if the frame changed, redraws if `needs_redraw` is set, and then moves, orders, and sets the level in one `SLSTransaction`. Drawing primitives live in `misc/drawing.h`.
- Corner radius comes from `SLSWindowIteratorGetCornerRadii`, resolved via `dlsym` only on macOS 26+ (`load_symbols` in `main.c`). It falls back to 9. Use this pattern for any private symbol that isn't available on every supported macOS version. Linking it directly breaks older systems.
- `border_update` currently always runs synchronously. The code after its early `return` (the async pthread path) can't be reached.

### yabai integration (`misc/yabai.h`)
This is compiled in through the `_YABAI_INTEGRATION` define. It registers a second Mach port, `git.felix.jbevent`, that yabai uses to announce window animations with proxy windows. During an animation the border is moved to track the proxy (`external_proxy_wid`, `is_proxy`, `proxy`). A `CVDisplayLink` (`animation.c`) follows the proxy's transform every frame. Normal updates skip borders while `external_proxy_wid` is set.

### Mission Control (`misc/mission_control.h`)
Since macOS 27, WindowServer no longer hides border windows during Mission Control, and Mission Control is drawn by the `WindowManager` process, not the Dock. While it's open, WindowManager creates one level-19 backdrop window per display, sized exactly to that display. `window_spawn_handler` (`events.c`) passes every created and destroyed window to `mission_control_window_created`/`_destroyed`. The first backdrop sets `g_mission_control_active` and hides all borders. When the last one no longer exists, borders are redrawn. Existence is checked because a destroy event also fires when a backdrop moves between spaces. `front_app_handler` also prunes stale backdrops, so borders can't get stuck hidden. `border_update_internal`, `border_unhide` and the yabai proxy ordering all check the flag.

Signals that do *not* work on macOS 27: SkyLight events 1327/1328 (space create/destroy fire for many unrelated spaces), and the Dock's `AXExpose*` accessibility notifications (never sent).

### Private API declarations
All SkyLight and other private symbols are declared by hand in `src/misc/extern.h` (plus a few in other `misc/` headers). When you use a new private function, add its prototype there. The framework is linked with `-F/System/Library/PrivateFrameworks -framework SkyLight`.

## Code style

Two-space indentation, and snake_case with module prefixes (`border_*`, `windows_*`, `table_*`, `mach_*`). Long argument lists are aligned in columns, with the closing paren padded to line up (see `border.c`). Globals use the `g_` prefix and are defined in `main.c`.
