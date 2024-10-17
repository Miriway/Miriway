#!/bin/bash
set -e

if [ ! -e ~/.config ]; then mkdir ~/.config; fi

shell_components="mate-panel mate-terminal /usr/libexec/mate-notification-daemon/mate-notification-daemon swaybg"
shell_packages="mate-panel mate-terminal mate-notification-daemon mate-backgrounds swaybg"

miriway_config="${XDG_CONFIG_HOME:-$HOME/.config}/miriway-shell.config"

for component in $shell_components
do
  if ! command -v "$component" > /dev/null
  then
    echo Need to install "$component"
    need_install=1
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

if [ -n "$need_install" ]
then
  if command -v apt > /dev/null
  then
    sudo apt install $shell_packages fonts-font-awesome
  elif command -v dnf > /dev/null
  then
    sudo dnf install $shell_packages fontawesome-fonts
  elif command -v apk > /dev/null
  then
    sudo apk add $shell_packages font-awesome
  else
    echo ERROR: I cannot find an install tool for this system
  fi
fi

if [ -e "/usr/share/backgrounds/ubuntu-mate-common/Green-Wall-Logo.png" ]; then
  # Try Ubuntu MATE wallpaper (from ubuntu-mate-wallpapers-common)
  background="/usr/share/backgrounds/ubuntu-mate-common/Green-Wall-Logo.png"
elif [ -e "/usr/share/backgrounds/mate/desktop/GreenTraditional.jpg" ]; then
  # Try Ubuntu MATE wallpaper (from mate-backgrounds)
  background="/usr/share/backgrounds/mate/desktop/GreenTraditional.jpg"
elif  [ -e "/usr/share/backgrounds/warty-final-ubuntu.png" ]; then
  # fall back to Ubuntu default
  background="/usr/share/backgrounds/warty-final-ubuntu.png"
else
  # fall back to anything we can find
  background="$(find /usr/share/backgrounds/ -type f | tail -n 1)"
fi

# Ensure we have a config file with the fixed options
cat <<EOT > "${miriway_config}"
x11-window-title=Miriway
idle-timeout=600
app-env-amend=XDG_SESSION_TYPE=wayland:GTK_USE_PORTAL=0:XDG_CURRENT_DESKTOP=Miriway:GTK_A11Y=none
shell-component=miriway-unsnap dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY XDG_DATA_HOME XDG_CONFIG_HOME XDG_CONFIG_DIRS XDG_DATA_DIRS XDG_CACHE_HOME XDG_DESKTOP_DIR XDG_SESSION_TYPE XDG_SESSION_DESKTOP XDG_CURRENT_DESKTOP
shell-component=miriway-unsnap /usr/libexec/mate-notification-daemon/mate-notification-daemon
shell-component=systemd-run --user --scope --slice=background.slice swaybg --mode fill --output '*' --image '${background}'
shell-component=miriway-unsnap mate-panel

shell-meta=a:miriway-unsnap mate-panel --run-dialog
ctrl-alt=t:miriway-unsnap mate-terminal
# This hack to work with X11
#shell-meta=a:miriway-unsnap sh -c "exec mate-panel --run-dialog --display \$DISPLAY"
#ctrl-alt=t:miriway-unsnap sh -c "exec mate-terminal --display \$DISPLAY"

meta=Left:@dock-left
meta=Right:@dock-right
meta=Space:@toggle-maximized
meta=Home:@workspace-begin
meta=End:@workspace-end
meta=Page_Up:@workspace-up
meta=Page_Down:@workspace-down
ctrl-alt=BackSpace:@exit
EOT
