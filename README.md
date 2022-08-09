# Miriway

Miriway is a simple Wayland desktop based on Mir

## In development

Miriway uses an updated version of the Mir API. This will be released in 
Mir 2.9, and, until then, is available from the mir-team "development" PPA:

```plain
sudo apt-add-repository ppa:mir-team/dev
```

## Dependencies

Build dependencies:
```plain
sudo apt install libmirclient-dev libxkb-common-dev
```

Runtime dependencies:
```plain
sudo apt install mir-graphics-drivers-desktop
```

In addition the default configuration expects the following programs:
```plain
sudo apt install wofi swaybg waybar
```
These provide a launcher, background and panel but alternatives can be used.

## Building

```plain
mkdir build
cd build
cmake ..
cmake --build .
```

## Running

Running (without installing):

```plain
./miriway
```

## Installing

```plain
sudo cmake --build . -- install
```

Now you can run with `miriway`, or select "Miriway" from the login screen.

## Keyboard shortcuts

Keys|Action
--|--
Ctrl-Alt-BkSp|Exit (long press to force if apps are open)
Meta-Left|Dock left
Meta-Right|Dock right
Meta-PkUp|Previous workspace
Meta-Shift-PkUp|Move window to previous workspace
Meta-PkDn|Next workspace
Meta-Shift-PkDn|Move window to next workspace
Meta-Home|First workspace
Meta-Home-PkUp|Move window to first workspace
Meta-End|Last workspace
Meta-End-PkDn|Move window to last workspace

### Keyboard shortcuts using `Meta` and `Ctrl-Alt` can be added or amended

These (and other) defaults are specified in `/etc/xdg/xdg-miriway/miriway-shell.config` but can be overridden 
in `~/.config/miriway-shell.config`. You should typically copy and edit the default config file.

Keys|Action
--|--
Ctrl-Alt-T|Terminal emulator
Meta-A|App launcher
