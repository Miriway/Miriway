#!/bin/bash
set -e

if [ ! -e ~/.config ]; then mkdir ~/.config; fi

shell_components="xfce4-terminal xfce4-appfinder xfce4-panel xfdesktop /usr/lib/x86_64-linux-gnu/xfce4/notifyd/xfce4-notifyd"
shell_packages="xfce4-terminal xfce4-appfinder xfce4-panel swaybg xfdesktop4 xfce4-notifyd"
miriway_config="${XDG_CONFIG_HOME:-$HOME/.config}/miriway-shell.config"

unset need_install

for component in $shell_components
do
  if ! command -v "$component" > /dev/null
  then
    echo May need to install "$component"
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

# Ensure we have a config file with the fixed options
cat <<EOT > "${miriway_config}"
x11-window-title=XFCE/Miriway
idle-timeout=600
app-env-amend=XDG_SESSION_TYPE=wayland:GTK_USE_PORTAL=0:XDG_CURRENT_DESKTOP=XFCE:GTK_A11Y=none

shell-component=miriway-unsnap xfdesktop
shell-component=miriway-unsnap /usr/lib/$(uname -i)-linux-gnu/xfce4/notifyd/xfce4-notifyd
shell-component=miriway-unsnap xfce4-panel
shell-meta=a:miriway-unsnap xfce4-appfinder --disable-server
ctrl-alt=t:miriway-unsnap xfce4-terminal

meta=Left:@dock-left
meta=Right:@dock-right
meta=Space:@toggle-maximized
meta=Home:@workspace-begin
meta=End:@workspace-end
meta=Page_Up:@workspace-up
meta=Page_Down:@workspace-down
ctrl-alt=BackSpace:@exit
EOT
