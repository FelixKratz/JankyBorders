# JankyBorders

<img align="right" width="50%" src="images/screenshot.png" alt="Screenshot">

*JankyBorders* is a lightweight tool designed to add colored borders to
user windows on macOS 14.0+. It enhances the user experience by visually
highlighting the currently focused window without relying on the accessibility
API, thereby being faster than comparable tools.

## Usage
### Install
The binary can be made available by installing it through Homebrew:
```bash
brew tap FelixKratz/formulae
brew install borders
```

For a comprehensive overview of all avaliable options and commands, consult the
man page: `man borders`. A rendered version of the man page is available in the
[Wiki](https://github.com/FelixKratz/JankyBorders/wiki/Man-Page).

### Bootstrap with yabai
For example, if you are using `yabai`, you could add:
```bash
borders active_color=0xffe1e3e4 inactive_color=0xff494d64 width=5.0 &
```
to the very end of your `yabairc`. This will start the borders with the
specified options.

### Bootstrap with brew
If you want to run this as a separate service, you could use:
```bash
brew services start borders
```

### Configuring the appearance
You can either configure the appearance directly when starting the borders
process (as shown in "Bootstrap with yabai") or use a configuration file.
The appearance can be adapted at any point in time.

#### Using a configuration file (Optional)
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
	active_color=0xffe2e2e3
	inactive_color=0xff414550
)

borders "${options[@]}"
```

#### Updating the border properties during runtime
If a `borders` process is already running, invoking a new `borders` instance
with any combination of the available arguments will update the properties of
the already running instance.

## Documentation
Local documentation is available as `man borders` and as a rendered verion in
the [Wiki](https://github.com/FelixKratz/JankyBorders/wiki/Man-Page).
