#!/bin/bash
set -e

if [ ! -e ~/.config ]; then mkdir ~/.config; fi

shell_components="budgie-panel budgie-run-dialog /usr/libexec/budgie-desktop/budgie-polkit-dialog tilix grim swaylock swaync swaybg"
shell_packages="budgie-desktop-environment budgie-wallpapers tilix grim swaylock sway-notification-center swaybg"
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
    sudo apt install $shell_packages
  elif command -v dnf > /dev/null
  then
    sudo dnf install $shell_packages
  elif command -v apk > /dev/null
  then
    sudo apk add $shell_packages
  else
    echo ERROR: I cannot find an install tool for this system
  fi
fi

# Ensure we have a config file with the fixed options
cat <<EOT > "${miriway_config}"
x11-window-title=Budgie/Miriway
idle-timeout=600
app-env-amend=XDG_SESSION_TYPE=wayland:GTK_USE_PORTAL=0:XDG_CURRENT_DESKTOP=Budgie:GTK_A11Y=none:-GTK_IM_MODULE:SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh
shell-component=systemd-run --user --scope --slice=background.slice swaybg --mode fill --output '*' --image /usr/share/backgrounds/budgie/ubuntu_budgie_wallpaper1.jpg
shell-component=systemd-run --user --scope --slice=background.slice swaync
shell-component=systemd-run --user --scope --slice=background.slice budgie-panel
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice gnome-keyring-daemon --foreground
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice /usr/libexec/budgie-desktop/budgie-polkit-dialog

ctrl-alt=t:miriway-unsnap tilix
shell-meta=a:miriway-unsnap budgie-run-dialog
meta=Print:miriway-unsnap sh -c "grim ~/Pictures/screenshot-\$(date --iso-8601=seconds).png"

shell-ctrl-alt=l:miriway-unsnap loginctl lock-session
lockscreen-app=miriway-unsnap swaylock -i /usr/share/backgrounds/budgie/ubuntu_budgie_wallpaper1.jpg

ctrl-alt=Up:@toggle-always-on-top
meta=Left:@dock-left
meta=Right:@dock-right
meta=Space:@toggle-maximized
meta=Home:@workspace-begin
meta=End:@workspace-end
meta=Page_Up:@workspace-up
meta=Page_Down:@workspace-down
ctrl-alt=BackSpace:@exit
EOT
