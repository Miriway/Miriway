# Miriway

Miriway is a simple Wayland desktop based on Mir

## Dependencies

Build dependencies:
```plain
sudo apt install libmirclient-dev libxkb-common-dev
```

Runtime dependencies:
```plain
sudo apt install wofi swaybg waybar
```

## Building

```plain
mkdir build
cd build
cmake ..
cmake --build
```

## Installing

```plain
sudo cmake --build install
```

## Keyboard shortcuts

Keys|Action
--|--
Ctrl-Alt-BkSp|Exit (long press to force if apps are open)
Ctrl-Alt-T|Open terminal
Meta-A|Open launcher
Meta-PkUp|Previous workspace
Meta-PkDn|Next workspace