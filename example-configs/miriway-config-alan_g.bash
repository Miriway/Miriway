#!/bin/bash
set -e

if [ ! -e ~/.config ]; then mkdir ~/.config; fi

shell_components="yambar swaybg synapse kgx swaync grim swaylock gnome-keyring-daemon"
miriway_config="${XDG_CONFIG_HOME:-$HOME/.config}/miriway-shell.config"
yambar_config="${XDG_CONFIG_HOME:-$HOME/.config}/yambar/config.yml"

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

if [ -e "${yambar_config}" ]; then
  echo WARNING Overwriting "${yambar_config}"
fi

read -p"OK to proceed/configure only? [y/n/c] " yn

case $yn in
  [Yy] ) echo Proceeding...;;
  [Cc] ) echo Skippling install...; skip_install=1;;
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
      swaync ) sudo apt install sway-notification-center;;
      gnome-keyring-daemon ) sudo apt install gnome-keyring;;
      * )   sudo apt install "$1";;
    esac
elif command -v dnf > /dev/null
then
    case "$1" in
      waybar ) sudo dnf install "$1" fontawesome-fonts;;
      yambar ) sudo dnf install "$1" fontawesome-fonts;;
      kgx ) sudo dnf install gnome-console;;
      gnome-keyring-daemon ) sudo dnf install gnome-keyring;;
      * )   sudo dnf install "$1";;
    esac
elif command -v apk > /dev/null
then
    case "$1" in
      waybar ) sudo apk add "$1" font-awesome;;
      yambar ) sudo apk add "$1" font-awesome;;
      kgx ) sudo apk add gnome-console;;
      gnome-keyring-daemon ) sudo apk add install gnome-keyring;;
      * )   sudo apk add "$1";;
    esac
else
  echo ERROR: I cannot find an install tool for this system
fi
}

if [ "${skip_install}" != "1" ]; then
for component in $shell_components
do
  if ! command -v "$component" > /dev/null
  then
    install "$component"
  fi
done
fi

mkdir "$(dirname "${yambar_config}")" -p -m 700

# Ensure we have a config file with the fixed options
cat <<EOT > "${miriway_config}"
x11-window-title=Miriway
idle-timeout=600
app-env-amend=XDG_SESSION_TYPE=wayland:GTK_USE_PORTAL=0:XDG_CURRENT_DESKTOP=Miriway:GTK_A11Y=none:-GTK_IM_MODULE:SSH_AUTH_SOCK=/run/user/1000/keyring/ssh
shell-component=miriway-unsnap dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP SSH_AUTH_SOCK
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice swaync
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice swaybg --mode fill --output '*' --image /usr/share/backgrounds/warty-final-ubuntu.png
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice synapse --startup
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice gnome-keyring-daemon --foreground
shell-component=yambar

ctrl-alt=t:miriway-unsnap kgx
shell-meta=a:miriway-unsnap synapse
meta=Print:miriway-unsnap sh -c "grim ~/Pictures/screenshot-\$(date --iso-8601=seconds).png"

shell-add-wayland-extension=ext_session_lock_manager_v1
shell-ctrl-alt=l:miriway-unsnap swaylock -i /usr/share/backgrounds/warty-final-ubuntu.png

meta=Left:@dock-left
meta=Right:@dock-right
meta=Space:@toggle-maximized
meta=Home:@workspace-begin
meta=End:@workspace-end
meta=Page_Up:@workspace-up
meta=Page_Down:@workspace-down
ctrl-alt=BackSpace:@exit

display-config=static=${miriway_config/%config/display}
EOT

# Ensure we have a config file with the fixed options
cat <<EOT > "${yambar_config}"
awesome: &awesome Font Awesome 6 Free:style=solid:pixelsize=14
ubuntu: &ubuntu Font Ubuntu 6 Free:style=solid:pixelsize=14

bar:
  location: top
  height: 21
  background: 282828ff
  font: *ubuntu

  left:
    - label:
        content:
          string:
            text: " start"
            margin: 5
            on-click: miriway-unsnap synapse
            deco: &greybg
              background:
                color: 3f3f3fff

  center:
    - clock:
        content:
          - string:
              margin: 5
              text: "{date} {time}"
              deco: *greybg

  right:
EOT

for wificard in /sys/class/net/wl*
do cat <<EOT >> "${yambar_config}"
    - network:
        name: $(basename "$wificard")
        content:
          map:
            deco: *greybg
            margin: 5
            default: {string: {text: , font: *awesome}}
            conditions:
              state == down: {string: {text: , font: *awesome}}
              state == up:
                map:
                  default:
                    - string: {text: , font: *awesome, deco: *greybg}
#                    - string: {text: "{ssid} {dl-speed:mb}/{ul-speed:mb} Mb/s" }

                  conditions:
                    ipv4 == "":
                      - string: {text: , font: *awesome}
#                      - string: {text: "{ssid} {dl-speed:mb}/{ul-speed:mb} Mb/s"}
EOT
done

for battery in /sys/class/power_supply/BAT*
do cat <<EOT >> "${yambar_config}"
    - battery:
        name: $(basename "$battery")
        anchors:
          discharging: &discharging
            list:
              items:
                - ramp:
                    tag: capacity
                    items:
                      - string: {text: , foreground: ff0000ff, font: *awesome}
                      - string: {text: , foreground: ffa600ff, font: *awesome}
                      - string: {text: , font: *awesome}
                      - string: {text: , font: *awesome}
                      - string: {text: , font: *awesome}
                      - string: {text: , font: *awesome}
                      - string: {text: , font: *awesome}
                      - string: {text: , font: *awesome}
                      - string: {text: , font: *awesome}
                      - string: {text: , foreground: 00ff00ff, font: *awesome}
                - string: {text: "{capacity}% {estimate}"}
        content:
          map:
            deco: *greybg
            margin: 5
            conditions:
              state == unknown:
                <<: *discharging
              state == discharging:
                <<: *discharging
              state == charging:
                - string: {text: , foreground: 00ff00ff, font: *awesome}
                - string: {text: "{capacity}% {estimate}"}
              state == full:
                - string: {text: , foreground: 00ff00ff, font: *awesome}
                - string: {text: "{capacity}% full"}
              state == "not charging":
                - ramp:
                    tag: capacity
                    items:
                      - string: {text:  , foreground: ff0000ff, font: *awesome}
                      - string: {text:  , foreground: ffa600ff, font: *awesome}
                      - string: {text:  , foreground: 00ff00ff, font: *awesome}
                      - string: {text:  , foreground: 00ff00ff, font: *awesome}
                      - string: {text:  , foreground: 00ff00ff, font: *awesome}
                      - string: {text:  , foreground: 00ff00ff, font: *awesome}
                      - string: {text:  , foreground: 00ff00ff, font: *awesome}
                      - string: {text:  , foreground: 00ff00ff, font: *awesome}
                      - string: {text:  , foreground: 00ff00ff, font: *awesome}
                      - string: {text:  , foreground: 00ff00ff, font: *awesome}
                - string: {text: "{capacity}%"}
EOT
done
