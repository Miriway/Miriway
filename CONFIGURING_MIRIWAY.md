# Configuring Miriway

The Miriway shell is intentionally provided with a minimal configuration
that can be built upon. The most obvious lack is the absence of "wallpaper",
"docks" and "panels". These are covered under "Shell Components".

Another lack is a way to launch applications. There's a very basic solution
in `Ctrl-Alt-T` which launches a terminal emulator from which other apps
can be started. But it is possible to integrate specialised app launchers
(and other applications) into the shell. See "Shell Apps" (below).

## Basic configuration

In addition to these Miriway specific configuration options, all the standard
Mir Server configuration options are supported. These include such things as
display layout.

### Supplying Configuration Options

Configuration options are set using one of three mechanisms:

1. As a command line option,
2. As a `MIR_SERVER_*` environment variable; or,
3. As an entry in `miriway-shell.config` (this will be found in `~/.config)` or from `XDG_CONFIG_DIRS`.

The same options can be supplied via all three options, but each option will
only be taken from the first place it is encountered. For example, the following
are equivalent:

Where|Format
--|--
In `.config`| `shell-component=swaybg --mode fill --output '*' --image /usr/share/backgrounds/warty-final-ubuntu.png`
In environment| `MIR_SERVER_SHELL_COMPONENT="swaybg --mode fill --output '*' --image /usr/share/backgrounds/warty-final-ubuntu.png"`
Command line| `--shell-component="swaybg --mode fill --output '*' --image /usr/share/backgrounds/warty-final-ubuntu.png"`

It is recommended to use entries in `miriway-shell.config` and the following
discussion ues this. But the command-line can be useful for trying things out. 

A full list of options can be obtained with:
```plain
miriway --help
```

### Shell Components

These are applications that are configured to be given access to privileged 
Wayland extension protocols (for example, `layer-shell` which allows placement
of wallpaper, panels, etc). There are two types of shell component:

Shell components that should be launched when the shell starts and restarted if
they fail are specified with `shell-component`. For example,

    shell-component=swaybg --mode fill --output '*' --image /usr/share/lubuntu/wallpapers/lubuntu-default-wallpaper.jpg

Shell components that are launched by the user are specified by either
`shell-meta` or `shell-ctrl-alt`. For example:

    shell-meta=a:wofi --show drun --location top_left

### Wayland Protocols Extension

The Wayland ecosystem is built of a collection of Wayland protocol extensions
that allow apps and compositors to negotiate how things are to be displayed.

Many of these extension protocols are generally useful and can be made available
to all applications. However, some of these extension protocols, while useful
for trusted applications, may have security concerns. Miriway restricts the use
of these protocols to apps explicitly configured as "Shell Components".

Additional Wayland extensions can be made available to all applications with:

    add-wayland-extensions=zwp_idle_inhibit_manager_v1

Additional Wayland extensions can be made available to shell components only
with:

    shell-add-wayland-extension=zwp_virtual_keyboard_manager_v1
    shell-add-wayland-extension=zwlr_virtual_pointer_manager_v1

### Working with the Miriway snap

If you are using the Miriway snap, or might be, then there can be problems
launching applications installed on the host system. This happens because
snaps run in a slightly modified environment. These can be avoided by
prefixing these commands with `miriway-unsnap` which removes these 
modifications. It is safe to use with unsnapped Miriway as it does nothing when
not using the snap. For example:

    ctrl-alt=t:miriway-unsnap mate-terminal

## Advanced configuration of `miriway-shell`

For simple use of Miriway the defaults provided by the `miriway` and
`miriway-session` scripts. But to enable bespoke customization for different
systems and destop environments there are the following environment variables.

* `MIRIWAY_SESSION_STARTUP` a script run when `miriway-shell` starts
* `MIRIWAY_SESSION_SHUTDOWN` a script run when `miriway-shell` shuts down
* `MIRIWAY_CONFIG_DIR` a config subdirectory used to find a bespoke `miriway-shell.config`

The `MIRIWAY_SESSION_STARTUP` and `MIRIWAY_SESSION_SHUTDOWN` variables are used
by `miriway-session` for `systemd` integration.

The `MIRIWAY_CONFIG_DIR` variable is used by desktop environments. For example,
`MIRIWAY_CONFIG_DIR=lxqt` will cause `miriway-shell` to look for 
`lxqt/miriway-shell.config`.