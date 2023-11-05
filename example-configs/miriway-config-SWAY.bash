#!/bin/bash
set -e

shell_components="swaybg waybar wofi swaync kgx swaylock"
miriway_config="${XDG_CONFIG_HOME:-$HOME/.config}/miriway-shell.config"
waybar_config="${XDG_CONFIG_HOME:-$HOME/.config}/waybar/config"
waybar_style="${XDG_CONFIG_HOME:-$HOME/.config}/waybar/style.css"

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

if [ -e "${waybar_config}" ]; then
  echo WARNING Overwriting "${waybar_config}"
fi

if [ -e "${waybar_style}" ]; then
  echo WARNING Overwriting "${waybar_style}"
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
      waybar ) sudo apt install "$1" fonts-font-awesome;;
      yambar ) sudo apt install "$1" fonts-font-awesome;;
      wofi ) sudo apt install "$1" fonts-font-awesome;;
      kgx ) sudo apt install gnome-console;;
      swaync ) sudo apt install sway-notification-center;;
      * )   sudo apt install "$1";;
    esac
elif command -v dnf > /dev/null
then
    case "$1" in
      waybar ) sudo dnf install "$1" fontawesome-fonts;;
      yambar ) sudo dnf install "$1" fontawesome-fonts;;
      kgx ) sudo dnf install gnome-console;;
      * )   sudo dnf install "$1";;
    esac
elif command -v apk > /dev/null
then
    case "$1" in
      waybar ) sudo apk add "$1" font-awesome;;
      yambar ) sudo apk add "$1" font-awesome;;
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

if [ -e "/usr/share/backgrounds/sway/Sway_Wallpaper_Blue_2048x1536.png" ]; then
  # Try SWAY wallpaper (from sway-backgrounds)
  background="/usr/share/backgrounds/sway/Sway_Wallpaper_Blue_2048x1536.png"
elif  [ -e "/usr/share/backgrounds/sway/Sway_Wallpaper_Blue_1920x1080.png" ]; then
  # Try SWAY wallpaper (from sway-backgrounds)
  background="/usr/share/backgrounds/sway/Sway_Wallpaper_Blue_1920x1080.png"
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
shell-component=dbus-update-activation-environment --systemd DISPLAY WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP
ctrl-alt=t:miriway-unsnap kgx
shell-component=miriway-unsnap systemd-run --user --scope --slice=background.slice swaync

shell-component=miriway-unsnap swaybg --mode fill --output '*' --image '${background}'
shell-component=miriway-unsnap waybar
shell-meta=a:miriway-unsnap wofi --show drun --location top_left

shell-add-wayland-extension=ext_session_lock_manager_v1
shell-meta=l:miriway-unsnap swaylock

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
mkdir -p ~/.config/waybar/
cat <<EOT > "${waybar_config}"
{
    "layer": "bottom", // Waybar at top layer
    "height": 30, // Waybar height (to be removed for auto height)
    "modules-left": ["network"],
    "modules-center": ["clock"],
    "modules-right": ["cpu", "memory", "temperature", "battery"],
    "tray": {
        // "icon-size": 21,
        "spacing": 10
    },
    "clock": {
        "format": "{:%y-%m-%d %H:%M}",
        "tooltip-format": "{:%y-%m-%d | %H:%M}",
        "format-alt": "{:%H:%M}"
    },
    "cpu": {
        "format": "CPU: {usage}%",
        "tooltip": false
    },
    "memory": {
        "format": "Mem: {}%"
    },
    "temperature": {
        // "thermal-zone": 2,
        // "hwmon-path": "/sys/class/hwmon/hwmon2/temp1_input",
        "critical-threshold": 80,
        // "format-critical": "{temperatureC}°C {icon}",
        "format": "{temperatureC}°C",
        "format-icons": ["", "", ""]
    },
    "battery": {
        "states": {
            // "good": 95,
            "warning": 30,
            "critical": 15
        },
        "format-alt": "{capacity}% {icon}",
        "format-charging": "{capacity}% ",
        "format-plugged": "{capacity}% ",
        "format": "{time} ({capacity}%)",
        // "format-good": "", // An empty format will hide the module
        // "format-full": "",
        "format-icons": ["", "", "", "", ""]
    },
    "network": {
        // "interface": "wlp2*", // (Optional) To force the use of this interface
        "format-wifi": "{essid} ({signalStrength}%)",
        "format-ethernet": "{ifname}: {ipaddr}/{cidr} ",
        "format-linked": "{ifname} (No IP) ",
        "format-disconnected": "Disconnected ⚠",
        "format-alt": "{ifname}: {ipaddr}/{cidr}"
    }
}
EOT

cat <<EOT > "${waybar_style}"
* {
    border: none;
    border-radius: 0;
    /* 'otf-font-awesome' is required to be installed for icons */
    font-family: Roboto, Helvetica, Arial, sans-serif;
    font-size: 13px;
    min-height: 0;
}

window#waybar {
    /*background-color: rgba(43, 48, 59, 0.5);
    border-bottom: 3px solid rgba(100, 114, 125, 0.5);*/
    background-color: rgba(146, 0, 106, 0.3);
    border-bottom: 3px solid rgba(73, 0, 53, 0.3);
    color: #ffffff;
    transition-property: background-color;
    transition-duration: .5s;
}

window#waybar.hidden {
    opacity: 0.2;
}

window#waybar.termite {
    background-color: #3F3F3F;
}

window#waybar.chromium {
    background-color: #000000;
    border: none;
}

#mode {
    background-color: #64727D;
    border-bottom: 3px solid #ffffff;
}

#clock,
#battery,
#cpu,
#memory,
#temperature,
#backlight,
#network,
#pulseaudio,
#custom-media,
#tray,
#mode,
#idle_inhibitor,
#mpd {
    padding: 0 10px;
    margin: 0 4px;
    color: #ffffff;
}

#clock {
    background-color: #64727D;
}

#battery {
    background-color: #ffffff;
    color: #000000;
}

#battery.charging {
    color: #ffffff;
    background-color: #26A65B;
}

@keyframes blink {
    to {
        background-color: #ffffff;
        color: #000000;
    }
}

#battery.critical:not(.charging) {
    background-color: #f53c3c;
    color: #ffffff;
    animation-name: blink;
    animation-duration: 0.5s;
    animation-timing-function: linear;
    animation-iteration-count: infinite;
    animation-direction: alternate;
}

label:focus {
    background-color: #000000;
}

#cpu {
    background-color: #2ecc71;
    color: #000000;
}

#memory {
    background-color: #9b59b6;
}

#backlight {
    background-color: #90b1b1;
}

#network {
    background-color: #2980b9;
}

#network.disconnected {
    background-color: #f53c3c;
}

#temperature {
    background-color: #f0932b;
}

#temperature.critical {
    background-color: #eb4d4b;
}

#tray {
    background-color: #2980b9;
}
EOT
