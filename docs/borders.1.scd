borders(1)

# NAME

JankyBorders - A window border system for macOS

# SYNOPSIS

*borders [options]*

# DESCRIPTION

*JankyBorders* is a lightweight tool designed to add colored borders to user
windows on macOS 14.0+. It enhances the user experience by visually highlighting
the currently focused window without relying on the accessibility API, thereby
ensuring performance and responsiveness.

# OPTIONS

*style=<style>*
	Specifies the style of the border (either _round_ or _square_)

*active_color=<color>*
	Sets the border color for the focused window (format: _0xAARRGGBB_). For
	instance, active_color=0xff00ff00 creates a green border.

*inactive_color=<color>*
	Sets the border color for all windows not in focus.

*width=<float>*
	Determines the width of the border. For example, width=5.0 creates a border
	5.0 points wide.

If an instance of *borders* is already running, subsequent invocations will
update the existing process with the new arguments.

# NOMENCLATURE

*<style>*
	Either _round_ or _square_.

*<color>*
	A color formatted as _0xAARRGGBB_, where _A_ is the alpha channel, _R_ the
	red channel, _G_ the green channel and _B_ the blue channel. All constituents
	take a base 16 number, i.e. _0-F_.

*<float>*
	A floating point number.

# BUGS

Report them at https://github.com/FelixKratz/JankyBorders/issues.

# MAINTAINERS

Felix Kratz <felix.kratz@tu-dortmund.de>
