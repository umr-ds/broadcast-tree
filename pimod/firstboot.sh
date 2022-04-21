#! /bin/bash

set -Eeuox pipefail

# Get some base variables required for the setup
IFNAME="$(grep b8:27 /sys/class/net/wlan*/address | cut -d'/' -f5)"
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
cd /broadcast-tree/nexmon/patches/bcm43430a1/7_45_41_46/btp/
source /broadcast-tree/nexmon/setup_env.sh
cp brcmfmac43430-sdio.bin /lib/firmware/brcm/brcmfmac43430-sdio.bin
rmmod brcmfmac || echo "brcmfmac module not loaded"
modprobe brcmutil
modprobe brcmfmac

sleep 1

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

# Mount folder for logs
BASE_PATH=/btree_data
mkdir -p "$BASE_PATH"

mount -t nfs 172.23.42.1:/srv/nfs/btree_data "$BASE_PATH"
