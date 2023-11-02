# JankyBorders
This small project adds colored borders to all user windows on macOS Sonoma and
later. This tool can be used to better identify the currently focused window.
It does not use the accessibility API and should thus be faster than comparable
tools.

Current status: Early development.

## Usage
Compile the source code to a binary using `make` in the cloned repository or
install the binary via brew:
```bash
brew tap FelixKratz/formulae
brew update
brew install borders
```

The binary can be started via `borders` and takes either no arguments or
the arguments (TODO: make this configurable during runtime):
```bash
borders active_color=<color_hex> inactive_color=<color_hex> width=<float>
```
which determine the color of the currently focused window, the inactive window 
and the width of the border respectively. The color hex shall be given in the
format: `0xAARRGGBB`, where `A` is the alpha channel, `R` the red channel,
`B` the blue channel and `B` the blue channel.

Any new user window, spawned after the launch of the `borders` process will
receive a border (TODO: Add borders to all existing windows).

### Bootstrap with yabai
For example, if you are using `yabai`, you could add:
```bash
borders active_color=0xffe1e3e4 inactive_color=0xff494d64 width=5.0 2>/dev/null 1>&2 &
```
to the very end of your `yabairc`.

### Bootstrap with brew
If you want to run this as a separate service, you could also use:
```bash
brew services start borders
```
