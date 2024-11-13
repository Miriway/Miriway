# Miriway

## Introducing Miriway
Miriway is a starting point for creating a Wayland based desktop environment using Mir.

Miriway has been tested with shell components from several desktop environments 
and comes with some scripts to help set up a variety of example configurations.
(See the `example-configs` directory.) 

### Keyboard shortcuts using `Meta` and `Ctrl-Alt` can be added or amended

These shortcuts are the defaults provided:

Modifiers|Key|Function|Action
--|--|--|--
Ctrl-Alt|T|miriway-terminal|Terminal emulator
meta|Left|@dock-left|Dock app left
meta|Right|@dock-right|Dock app right
meta|Space|@toggle-maximized|Toggle app maximized
meta|Home|@workspace-begin|First workspace ("Shift" to bring app)
meta|End|@workspace-end|Last workspace ("Shift" to bring app)
meta|Page_Up|@workspace-up|Previous workspace ("Shift" to bring app)
meta|Page_Down|@workspace-down|Next workspace ("Shift" to bring app)
ctrl-alt|BackSpace|@exit|Exit ("Shift" to force if apps are open)

These (and other) defaults are specified in `/usr/local/etc/xdg/xdg-miriway/miriway-shell.config` but can be
overridden for an individual user in `~/.config/miriway-shell.config`. For more
details see [Configuring Miriway](CONFIGURING_MIRIWAY.md).

The "@" commands are internal to miriway-shell, others are commands that could be executed from a terminal.

## Miriway internals

At the core of Miriway is `miriway-shell`, a Mir based Wayland compositor that provides:
* A "floating windows" window managament policy;
* Support for Wayland (and via `Xwayland`) X11 applications;
* Dynamic workspaces;
* Additional Wayland support for "shell components" such as panels and docs; and,
* Configurable shortcuts for launching standard apps such as launcher and terminal emulator.

In addition to `miriway-shell`, Miriway has:

* A "terminal emulator finder" script `miriway-terminal`, that works with most terminal emulators;
* A launch script `miriway` to simplify starting Miriway;
* A default configuration file `miriway-shell.config`; and,
* A greeter configuration `miriway.desktop` so Miriway can be selected at login

## Pre-packaged Miriway

Miriway is available as a Snap, which provides a basic default configuration 
and also includes the example configuration scripts to help set up a variety
of options.

[![miriway](https://snapcraft.io/miriway/badge.svg)](https://snapcraft.io/miriway)

## Building Miriway from source

Miriway can be built and installed from source (without packaging as a Snap).
The source is available from GitHub:

```plain
git clone https://github.com/Miriway/Miriway.git && cd Miriway
```

### Dependencies

Miriway uses the latest version of Mir available from the mir-team "release" PPA:

```plain
sudo apt-add-repository ppa:mir-team/release
```

Build dependencies:
```plain
sudo apt install libmiral-dev
```

Additional runtime dependencies:
```plain
sudo apt install mir-graphics-drivers-desktop
```

The default install is minimal and provides a basic shell and a default 
`Ctrl-Alt-T` command that tries to find a terminal emulator (by using 
`miriway-terminal`). If you don't already have a terminal emulator 
installed, then `sudo apt install xfce4-terminal` is a simple option.

### Building and Installing

```plain
mkdir build
cd build
cmake ..
cmake --build .
```

Installing

```plain
sudo cmake --build . -- install
```

Now you can run with `miriway`, or select "Miriway" from the login screen.

## Community

* [GitHub Discussions](https://github.com/Miriway/Miriway/discussions)
* [Matrix chat room](https://matrix.to/#/#miriway:matrix.org)
