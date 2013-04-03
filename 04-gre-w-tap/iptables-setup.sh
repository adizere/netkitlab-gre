#!/bin/bash

# this should be run as root

if [ "$(id -u)" != "0" ]; then
   echo "Please run as root" 1>&2
   exit 1;
fi

iptables -t nat -I POSTROUTING 1 -o wlan0 -j MASQUERADE
iptables -I FORWARD 1 -i nk_tap_adizere -j ACCEPT

echo "1" > /proc/sys/net/ipv4/ip_forward
