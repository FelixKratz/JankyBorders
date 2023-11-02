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
brew install FelixKratz/formulae/borders
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
