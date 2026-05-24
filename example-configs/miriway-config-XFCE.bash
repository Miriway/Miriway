#!/bin/bash
set -e

if [ ! -e ~/.config ]; then mkdir ~/.config; fi

shell_components="xfce4-terminal xfce4-appfinder xfce4-panel xfdesktop /usr/lib/x86_64-linux-gnu/xfce4/notifyd/xfce4-notifyd /usr/libexec/xfce-polkit"
shell_packages="xfce4-terminal xfce4-appfinder xfce4-panel swaybg xfdesktop4 xfce4-notifyd xfce-polkit"
miriway_config="${XDG_CONFIG_HOME:-$HOME/.config}/miriway-shell.config"
miriway_settings="${XDG_CONFIG_HOME:-$HOME/.config}/miriway-shell.settings"

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

if [ -e "${miriway_settings}" ]; then
  echo WARNING Overwriting "${miriway_settings}"
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
shell-component=miriway-unsnap /usr/lib/$(uname -m)-linux-gnu/xfce4/notifyd/xfce4-notifyd
shell-component=miriway-unsnap /usr/libexec/xfce-polkit
shell-component=miriway-unsnap xfce4-panel
EOT

cat <<EOT > "${miriway_settings}"
command_shell_meta=a:miriway-unsnap xfce4-appfinder --disable-server
command_ctrl_alt=t:miriway-unsnap xfce4-terminal

command_meta=Left:@dock-left
command_meta=Right:@dock-right
command_meta=Space:@toggle-maximized
command_meta=Home:@workspace-begin
command_meta=End:@workspace-end
command_meta=Page_Up:@workspace-up
command_meta=Page_Down:@workspace-down
command_ctrl_alt=BackSpace:@exit
EOT
