#!/bin/bash
set -e

shell_components="yambar swaybg synapse kgx"
miriway_config="$HOME/.config/miriway-shell.config"
yambar_config="$HOME/.config/yambar/config.yml"

if [ -e "${miriway_config}" ]; then
  echo WARNING Overwriting "${miriway_config}"
fi

if [ -e "${yambar_config}" ]; then
  echo WARNING Overwriting "${yambar_config}"
fi

for component in $shell_components
do
  if ! command -v "$component" > /dev/null
  then
    echo Need to install "$component"
  fi
done

read -p"OK to proceed? [y/n] " yn

case $yn in
  [Yy] ) echo Proceeding...;;
  [Nn] ) exit 1;;
esac

for component in $shell_components
do
  if ! command -v "$component" > /dev/null
  then
    case "$component" in
      kgx ) sudo apt install gnome-console;;
      * )   sudo apt install "$component";;
    esac
  fi
done

mkdir -p "$(dirname "${yambar_config}")" -m 700

# Ensure we have a config file with the fixed options
cat <<EOT > "${miriway_config}"
x11-window-title=Miriway
idle-timeout=600
app-env-amend=XDG_SESSION_TYPE=wayland:GDK_USE_PORTAL=none:XDG_CURRENT_DESKTOP=Miriway:GTK_A11Y=none
shell-component=sh -c "dbus-update-activation-environment DISPLAY WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP"

shell-component=swaybg -i /usr/share/backgrounds/warty-final-ubuntu.png
shell-component=yambar

meta=a:synapse
ctrl-alt=t:kgx
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
            on-click: synapse
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

for wificard in $(ls -d /sys/class/net/wl*)
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

for battery in $(ls -d /sys/class/power_supply/BAT*)
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