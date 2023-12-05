# JankyBorders

<img align="right" width="50%" src="images/screenshot.png" alt="Screenshot">

*JankyBorders* is a lightweight tool designed to add colored borders to user
windows on macOS 14.0+. It enhances the user experience by visually highlighting
the currently focused window without relying on the accessibility API, thereby
being faster than comparable tools.

## Usage
Compile the source code to a binary using `make` in the cloned repository or
install the binary via brew:
```bash
brew tap FelixKratz/formulae
brew install borders
```

The binary can be started via `borders` and takes
any of the arguments (in arbitrary order and count):

* `active_color=<color>`
* `inactive_color=<color>`
* `width=<float>`
* `style=<round/square>`
* `hidpi=<off/on>`

Those determine the color of the currently focused window, the inactive window
and the width, style and resolution of the border respectively. The color hex
shall be given in the format: `0xAARRGGBB`, where `A` is the alpha channel, `R`
the red channel, `G` the green channel and `B` the blue channel.


The color argument can take the special values:

* `gradient(top_left=0xAARRGGBB,bottom_right=0xAARRGGBB)`
* `gradient(top_right=0xAARRGGBB,bottom_left=0xAARRGGBB)`

(Note: Depending on your shell, you might need to quote these arguments)

If a `borders` process is already running, invoking a new `borders` instance
with any combination of the above arguments will update the properties of the
already running instance (just like in yabai and sketchybar).

### Bootstrap with yabai
For example, if you are using `yabai`, you could add:
```bash
borders active_color=0xffe1e3e4 inactive_color=0xff494d64 width=5.0 &
```
to the very end of your `yabairc`.

### Bootstrap with brew
If you want to run this as a separate service, you could also use:
```bash
brew services start borders
```
If the primary `borders` process is started without any arguments (or launched
as a service by brew), it will search for a file at
`~/.config/borders/bordersrc` and execute it on launch if found.

An example configuration file could look like this:
`~/.config/borders/bordersrc`
```bash
#!/bin/bash

options=(
	style=round
	width=6.0
	hidpi=off
	#active_color=0xffe2e2e3
	active_color=0xfff39660
	inactive_color=0xff414550
)

borders "${options[@]}"
```

## Documentation
Local documentation is available as `man borders`.
