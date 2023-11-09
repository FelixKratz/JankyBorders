borders(1)

# NAME

JankyBorders - A window border system for macOS

# SYNOPSIS

*borders*

# OPTIONS

*style=<style>*
	Specify the border style.

*active_color=<color>*
	Specify the border color of the window which is currently focused.

*inactive_color=<color>*
	Specify the border color of all windows which are not currently focused.

*width=<float>*
	Specify the border width.

If an instance of *borders* is already running, another invocation will send
the arguments to the already running process and update them accordingly.

# NOMENCLATURE

*<style>*
	Either _round_ or _square_.

*<color>*
	A color formatted as _0xAARRGGBB_, where _A_ is the alpha channel, _R_ the
	red channel, _G_ the green channel and _B_ the blue channel. All constituents
	take a base 16 number, i.e. _0-F_. ++
	E.g. active_color=0xff00ff00 creates a green border.

*<float>*
	A floating point number
	E.g. width=5.0 creates a 5.0 points wide border.

# DESCRIPTION

This small project adds colored borders to all user windows on macOS Sonoma and
later. It can be used to better identify the currently focused window. It does
not use the accessibility API and should thus be faster than comparable tools.

# BUGS

Report them at https://github.com/FelixKratz/JankyBorders/issues.

# MAINTAINERS

Felix Kratz <felix.kratz@tu-dortmund.de>
