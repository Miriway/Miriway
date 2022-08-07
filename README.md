# Miriway

Miriway is a simple Wayland desktop based on Mir

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
These provide a launcher, background and panel but alernatives can be used.

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

These are specified in `~/.config/miriway.config`

This is the default:
```plain
shell-meta=a:wofi --show drun --location top_left
ctrl-alt=t:miriway-terminal
```

There are four similar options that can be specified zero or more times: 
`shell-meta`, `shell-ctrl-alt`, `meta` and `ctrl-alt`. The modifier keys
`meta` and `ctrl-alt` should be self-explanatory. These options starting 
with a `shell` prefix will start the application with access to "shell"
related Wayland extensions.

There is a similar `shell-component` option for programs to be started 
with access to "shell" related Wayland extensions.