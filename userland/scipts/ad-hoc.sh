#!/usr/bin/env bash

set -Eeu

if ! [ "${EUID:-$(id -u)}" -eq 0 ]; then
  echo "Please run as root."
  exit 1
fi

if [ "$#" -ne 2 ]; then
  echo "First parameter must be interface name, second parameter must be IP address."
  exit 2
fi

IFNAME=$1
IPADDR=$2

set -x

ifconfig "$IFNAME" down
iwconfig "$IFNAME" key "s:12345"
iwconfig "$IFNAME" mode ad-hoc
ifconfig "$IFNAME" up
iwconfig "$IFNAME" essid btp
iwconfig "$IFNAME" channel 1
iwconfig "$IFNAME" ap c0:ff:ee:c0:ff:ee
ifconfig "$IFNAME" "$IPADDR"
ifconfig "$IFNAME" netmask 255.255.255.0
