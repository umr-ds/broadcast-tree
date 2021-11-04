#! /bin/bash

set -Eeuox pipefail

# Get some base variables required for the setup
IFNAME="wlan0"
MAC_ADDRESS="$(cat /sys/class/net/$IFNAME/address)"
MIDDLE=$(echo "$MAC_ADDRESS" | cut -d':' -f5 | tr -d '[:space:]')
SUFFIX=$(echo "$MAC_ADDRESS" | cut -d':' -f6 | tr -d '[:space:]')
HNAME="btp-${MIDDLE}-${SUFFIX}"
IP_ADDR="10.10.$(( 16#$MIDDLE )).$(( 16#$SUFFIX ))"

# Set a unique hostname
echo "$HNAME" > /etc/hostname
sed -i "s/raspberrypi/$HNAME/g" /etc/hosts
hostname "$HNAME"

# Install the nexmon patch for BTP
cd /nexmon/patches/bcm43430a1/7_45_41_46/btp/
source /nexmon/setup_env.sh
make install-firmware

# Setup ad-hoc mode
ifconfig "$IFNAME" down
iwconfig "$IFNAME" key off
iwconfig "$IFNAME" mode ad-hoc
ifconfig "$IFNAME" up
iwconfig "$IFNAME" essid btp
iwconfig "$IFNAME" channel 1
iwconfig "$IFNAME" ap c0:ff:ee:c0:ff:ee
ifconfig "$IFNAME" "$IP_ADDR"
ifconfig "$IFNAME" netmask 255.255.0.0
