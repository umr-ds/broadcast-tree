#! /bin/bash

set -Eeuox pipefail

# Get some base variables required for the setup
IFNAME="wlan0"
MAC_ADDRESS="$(cat /sys/class/net/$IFNAME/address)"
MIDDLE=$(echo "$MAC_ADDRESS" | cut -d':' -f5 | tr -d '[:space:]')
SUFFIX=$(echo "$MAC_ADDRESS" | cut -d':' -f6 | tr -d '[:space:]')
HNAME="btp-$SUFFIX"
IP_ADDR="10.10.$(( 16#$MIDDLE )).$(( 16#$SUFFIX ))"

# Set a unique hostname
echo "$HNAME" > /etc/hostname
sed -i "s/raspberrypi/$HNAME/g" /etc/hosts
hostname "$HNAME"

# Install the Raspberry Pi linux sources
rpi-source

# Install the nexmon patch for BTP
cp -r /btp/nexmon_patch /nexmon/patches/bcm43430a1/7_45_41_46/btp/
cd /nexmon/patches/bcm43430a1/7_45_41_46/btp/ || exit 1
source /nexmon/setup_env.sh
make install-firmware

# Make the nexmon installation permanent
MODPATH=$(/sbin/modinfo | head -1 | cut -d':' -f2 | tr -d '[:space:]')
cp "$MODPATH" /root/brcmfmac.ko.orig
cp /nexmon/patches/bcm43430a1/7_45_41_46/btp/brcmfmac_kernel49/brcmfmac.ko "$MODPATH"
depmod -a

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
