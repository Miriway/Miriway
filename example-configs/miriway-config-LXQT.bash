#!/bin/bash
set -e

if [ ! -e ~/.config ]; then mkdir ~/.config; fi

shell_components="lxqt-policykit-agent qterminal lxqt-runner lxqt-panel swaybg"
shell_packages="lxqt-policykit qterminal lxqt-runner lxqt-panel lubuntu-artwork swaybg"
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
    sudo apt install $shell_packages fonts-font-awesome lxqt-branding-debian
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
x11-window-title=Miriway
idle-timeout=600
app-env-amend=XDG_SESSION_TYPE=wayland:GTK_USE_PORTAL=0:XDG_CURRENT_DESKTOP=Miriway:GTK_A11Y=none

shell-component=swaybg --mode fill --output '*' --image /usr/share/lubuntu/wallpapers/lubuntu-default-wallpaper.jpg
shell-component=miriway-unsnap lxqt-policykit-agent
shell-component=miriway-unsnap lxqt-panel
ctrl-alt=t:miriway-unsnap qterminal
shell-meta=a:miriway-unsnap lxqt-runner

meta=Left:@dock-left
meta=Right:@dock-right
meta=Space:@toggle-maximized
meta=Home:@workspace-begin
meta=End:@workspace-end
meta=Page_Up:@workspace-up
meta=Page_Down:@workspace-down
ctrl-alt=BackSpace:@exit
EOT
