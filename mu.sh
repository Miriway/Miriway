#!/bin/bash
if [ $(id -u) != "0" ]
then
	echo "Please run this script as root"
	echo "For installation instructions, please refer to readme.txt"
	exit 1
fi

rm -rf /usr/local/share/wayland-sessions/miriway.desktop
rm -f /usr/local/bin/miriway*
rm -r /usr/local/etc/xdg/xdg-miriway
xdg-desktop-menu forceupdate
echo "Miriway is successfully removed from your computer"
