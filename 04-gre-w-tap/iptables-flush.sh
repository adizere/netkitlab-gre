#!/bin/bash

# this should be run as root

if [ "$(id -u)" != "0" ]; then
   echo "Please run as root" 1>&2
   exit 1;
fi


iptables -F
iptables -X

iptables -t nat -F
iptables -t nat -X

iptables -P INPUT ACCEPT
iptables -P FORWARD ACCEPT
iptables -P OUTPUT ACCEPT
