#!/bin/bash
set -e

if [ ! -e ~/.config ]; then mkdir ~/.config; fi

if  [ -e "/usr/libexec/mate-polkit/polkit-mate-authentication-agent-1" ]; then
  polkit_agent="/usr/libexec/mate-polkit/polkit-mate-authentication-agent-1"
else
  polkit_agent="/usr/libexec/polkit-mate-authentication-agent-1"
fi

shell_components="waybar synapse swaync swaybg swaylock kgx grim gnome-keyring-daemon ${polkit_agent}"
shell_packages="waybar synapse sway-notification-center swaybg swaylock gnome-console grim gnome-keyring mate-polkit"

miriway_config="${XDG_CONFIG_HOME:-$HOME/.config}/miriway-shell.config"
miriway_settings="${XDG_CONFIG_HOME:-$HOME/.config}/miriway-shell.settings"
waybar_config="${XDG_CONFIG_HOME:-$HOME/.config}/waybar/config"
waybar_style="${XDG_CONFIG_HOME:-$HOME/.config}/waybar/style.css"

unset need_install

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

if [ -e "${miriway_settings}" ]; then
  echo WARNING Overwriting "${miriway_settings}"
fi

if [ -e "${waybar_config}" ]; then
  echo WARNING Overwriting "${waybar_config}"
fi

if [ -e "${waybar_style}" ]; then
  echo WARNING Overwriting "${waybar_style}"
fi

read -p"OK to proceed/configure only? [y/n/c] " yn

case $yn in
  [Yy] ) echo Proceeding...;;
  [Cc] ) echo Skippling install...; unset need_install;;
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

mkdir "$(dirname "${waybar_config}")" -p -m 700

if  [ -e "/usr/libexec/mate-polkit/polkit-mate-authentication-agent-1" ]; then
  polkit_agent="/usr/libexec/mate-polkit/polkit-mate-authentication-agent-1"
elif  [ -e "/usr/libexec/polkit-mate-authentication-agent-1" ]; then
  polkit_agent="/usr/libexec/polkit-mate-authentication-agent-1"
else
  polkit_agent="$(find /usr/lib* -name polkit-*-agent-1 | tail -n 1)"
fi

miriway_display="${miriway_config/%config/display}"
if  [ -e "/home/${USER}/Pictures/202109-Norfolk/IMG_5949.JPG" ]; then
  background="/home/${USER}/Pictures/202109-Norfolk/IMG_5949.JPG"
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
app-env-amend=XDG_SESSION_TYPE=wayland:GTK_USE_PORTAL=0:XDG_CURRENT_DESKTOP=Miriway:GTK_A11Y=none:-GTK_IM_MODULE:SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh
display-config=static=${miriway_display}
lockscreen-app=miriway-unsnap swaylock -i ${background}

shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice synapse --startup
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice swaybg --mode fill --output '*' --image ${background}
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice swaync
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice waybar
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice gnome-keyring-daemon --foreground
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice ${polkit_agent}
EOT

cat <<EOT > "${miriway_settings}"
touchpad_tap_to_click=true
command_ctrl_alt=t:miriway-unsnap kgx
command_shell_meta=a:miriway-unsnap synapse
command_meta=Print:miriway-unsnap sh -c "grim ~/Pictures/screenshot-\$(date --iso-8601=seconds).png"

command_shell_ctrl_alt=l:miriway-unsnap loginctl lock-session

command_ctrl_alt=d:cp ${miriway_display}~docked ${miriway_display}
command_ctrl_alt=u:cp ${miriway_display}~undocked ${miriway_display}
command_ctrl_alt=s:miriway-swap
command_ctrl_alt=k:miriway-unsnap lock-and-suspend.sh
command_ctrl_alt=Up:@toggle-always-on-top
command_shell_ctrl_alt=y:systemd-run --user --scope --slice=background.slice waybar

command_meta=Left:@dock-left
command_meta=Right:@dock-right
command_meta=Space:@toggle-maximized
command_meta=Home:@workspace-begin
command_meta=End:@workspace-end
command_meta=Page_Up:@workspace-up
command_meta=Page_Down:@workspace-down
command_ctrl_alt=BackSpace:@exit
EOT

# Install the waybar config files shipped alongside this script,
# expanding @CPU_ICONS@ to one {icon<n>} per thread on this machine
waybar_source="$(dirname "$0")/../waybar"
threads=$(grep -c '^cpu[0-9]' /proc/stat)
cpu_icons=$(printf '{icon%s}' $(seq 0 $((threads - 1))))
sed "s|@CPU_ICONS@|${cpu_icons}|" "${waybar_source}/config" > "${waybar_config}"
cp "${waybar_source}/style.css" "${waybar_style}"
