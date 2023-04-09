#!/bin/bash
set -e

shell_components="yambar swaybg synapse kgx"
miriway_config="$HOME/.config/miriway-shell.config"
yambar_config="$HOME/.config/yambar/config.yml"

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

mkdir "$(dirname "${yambar_config}")" -p -m 700

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

meta=Left:@dock-left
meta=Right:@dock-right
meta=Space:@toggle-maximized
meta=Home:@workspace-begin
meta=End:@workspace-end
meta=Page_Up:@workspace-up
meta=Page_Down:@workspace-down
ctrl-alt=BackSpace:@exit
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