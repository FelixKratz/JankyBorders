borders(1)

# NAME

JankyBorders - A window border system for macOS

# SYNOPSIS

*borders [options]*

# DESCRIPTION

*JankyBorders* is a lightweight tool designed to add colored borders to
user windows on macOS 14.0+. It enhances the user experience by visually
highlighting the currently focused window without relying on the accessibility
API, thereby being faster than comparable tools.

# OPTIONS

*style=<style>*
	Specifies the style of the border (either _round_ or _square_)

*active_color=<color>*
	Sets the border color for the focused window (format: _0xAARRGGBB_). For
	instance, active_color=0xff00ff00 creates a green border.

*inactive_color=<color>*
	Sets the border color for all windows not in focus.

*background_color=<color>*
	Sets the background fill color for all windows (only _0xAARRGGBB_ arguments supported).

*width=<float>*
	Determines the width of the border. For example, width=5.0 creates a border
	5.0 points wide.

*hidpi=<boolean>*
	If set to _on_, the border will be drawn with retina resolution.

*ax_focus=<boolean>*
	If set to _on_, the (slower) accessibility API is used to resolve the
	focused window. Enabled automatically if the (parent) process has
	accessibility permissions. Improves compatibility with other tools
	modifying window properties on a system level (e.g. yabai).

*blacklist=<application_list>*
	The applications specified here are excluded from being bordered. For
	example, blacklist="Safari,kitty" excludes Safari and kitty from being
	bordered.

*whitelist=<application_list>*
	Once this list is populated, only applications listed here are considered
	for receiving a border. If the whitelist is empty (default) it is inactive.

If an instance of *borders* is already running, subsequent invocations will
update the existing process with the new arguments.

If no instance of *borders* is running and no arguments are supplied, we try
to execute a file at ~/.config/borders/bordersrc where a configuration command
could be issued.

# NOMENCLATURE

*<float>*
	A floating point number.

*<boolean>*
	Either _on_ or _off_.

*<style>*
	Either _round_ or _square_.

*<color>*
	A color formatted as _0xAARRGGBB_, where _A_ is the alpha channel, _R_ the
	red channel, _G_ the green channel and _B_ the blue channel. All
	constituents take a base 16 number, i.e. _0-F_.

	The color argument can take the special values: ++
	_gradient(top_left=0xAARRGGBB,bottom_right=0xAARRGGBB)_ ++
	_gradient(top_right=0xAARRGGBB,bottom_left=0xAARRGGBB)_ ++
	_glow(0xAARRGGBB)_ ++
(You might need to quote these arguments depending on your shell)

*<application_list>*
	A comma separated list of application names. This argument should be
	quoted.

# BUGS

Report them at https://github.com/FelixKratz/JankyBorders/issues.

# MAINTAINERS

Felix Kratz <felix.kratz@tu-dortmund.de>
