#!/bin/bash
set -e

shell_components="mate-panel mate-terminal swaybg"
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
      kgx ) sudo apt install gnome-console;;
      * )   sudo apt install "$1";;
    esac
elif command -v dnf > /dev/null
then
    case "$1" in
      kgx ) sudo dnf install gnome-console;;
      * )   sudo dnf install "$1";;
    esac
elif command -v apk > /dev/null
then
    case "$1" in
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

shell-component=swaybg -i /usr/share/backgrounds/warty-final-ubuntu.png
shell-component=mate-panel

meta=a:sh -c "exec mate-panel --run-dialog --display $DISPLAY"
ctrl-alt=t:sh -c "exec mate-terminal --display $DISPLAY"
EOT
