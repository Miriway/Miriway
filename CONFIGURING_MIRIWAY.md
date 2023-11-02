# Configuring Miriway

The Miriway shell is intentionally provided with a minimal configuration
that can be built upon. The most obvious lack is the absence of "wallpaper",
"docks" and "panels". These are covered under "Shell Components".

Another lack is a way to launch applications. There's a very basic solution
in `Ctrl-Alt-T` which launches a terminal emulator from which other apps
can be started. But it is possible to integrate specialised app launchers
(and other applications) into the shell. See "Shell Apps" (below).

In addition to these Miriway specific configuration options, all the standard
Mir Server configuration options are supported. These include such things as
display layout.

## Supplying Configuration Options

Configuration options are set using one of three mechanisms:

1. As an entry in `~/.config/miriway-shell.config` or `[/usr/local]/etc/xdg/xdg-miriway/miriway-shell.config`;
2. As a `MIR_SERVER_*` environment variable; or,
3. as a command line option.

The following discussion describes adding entries to `~/.config/miriway-shell.config`
but the same options can be supplied via all three options. In particular, 
the command-line can be useful for trying things out. For example, the following
are equivalent:

Where|Format
--|--
In `.config`| `shell-component=swaybg --mode fill --output '*' --image /usr/share/backgrounds/warty-final-ubuntu.png`
In env| `MIR_SERVER_SHELL_COMPONENT="swaybg --mode fill --output '*' --image /usr/share/backgrounds/warty-final-ubuntu.png"`
Command line| `--shell-component="swaybg --mode fill --output '*' --image /usr/share/backgrounds/warty-final-ubuntu.png"`

Where there's a clash, the command-line overrides the environment variable which overrides the `.config`.

A full list of options can be obtained with:
```plain
miriway --help
```

## Wayland Extension Protocols

The Wayland ecosystem is built of a collection of Wayland Extension protocols
that allow apps and compositors to negotiate how things are to be displayed.

Many of these extension protocols are generally useful and can be made available
to all applications. However, some of these extension protocols, while useful
for trusted applications, may have security concerns. Miriway restricts the use
of these protocols to apps explicitly configured as "Shell Components" or 
"Shell Apps".

## Shell Components

These are applications that are configured to be run when Miriway starts and
to be given access to "trusted" Wayland extension protocols (for example,
`layer-shell` which allows placement of wallpaper, panels, etc).

