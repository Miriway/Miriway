#!/bin/bash
set -e

shell_components="swaybg xfce4-terminal xfce4-appfinder"
miriway_config="$HOME/.config/miriway-shell.config"

for component in $shell_components
do
  if ! command -v "$component" > /dev/null
  then
    echo Need to install "$component"
  fi
done

if [ -e "${miriway_config}" ]; then
  echo WARNING Overwriting "${miriway_config}"
fi

read -p"OK to proceed? [y/n] " yn

case $yn in
  [Yy] ) echo Proceeding...;;
  [Nn] ) exit 1;;
esac

install()
{
if command -v apt > /dev/null
then
    case "$1" in
      waybar ) sudo apt install "$1" fonts-font-awesome;;
      yambar ) sudo apt install "$1" fonts-font-awesome;;
      kgx ) sudo apt install gnome-console;;
      * )   sudo apt install "$1";;
    esac
elif command -v dnf > /dev/null
then
    case "$1" in
      waybar ) sudo dnf install "$1" fontawesome-fonts;;
      yambar ) sudo dnf install "$1" fontawesome-fonts;;
      kgx ) sudo dnf install gnome-console;;
      * )   sudo dnf install "$1";;
    esac
elif command -v apk > /dev/null
then
    case "$1" in
      waybar ) sudo apk install "$1" font-awesome;;
      yambar ) sudo apk install "$1" font-awesome;;
      kgx ) sudo apk add gnome-console;;
      * )   sudo apk add "$1";;
    esac
else
  echo ERROR: I cannot find an install tool for this system
fi
}

for component in $shell_components
do
  if ! command -v "$component" > /dev/null
  then
    install "$component"
  fi
done

# Ensure we have a config file with the fixed options
cat <<EOT > "${miriway_config}"
x11-window-title=Miriway
idle-timeout=600
app-env-amend=XDG_SESSION_TYPE=wayland:GDK_USE_PORTAL=none:XDG_CURRENT_DESKTOP=Miriway:GTK_A11Y=none
shell-component=sh -c "dbus-update-activation-environment DISPLAY WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP"
ctrl-alt=t:miriway-terminal # Default "terminal emulator finder"

shell-component=swaybg -i /usr/share/backgrounds/warty-final-ubuntu.png # Wallpaper/background
meta=a:xfce4-appfinder --disable-server # Launcher
ctrl-alt=t:xfce4-terminal               # Terminal emulator

meta=Left:@dock-left
meta=Right:@dock-right
meta=Space:@toggle-maximized
meta=Home:@workspace-begin
meta=End:@workspace-end
meta=Page_Up:@workspace-up
meta=Page_Down:@workspace-down
ctrl-alt=BackSpace:@exit
EOT