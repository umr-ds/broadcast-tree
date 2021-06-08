#! /bin/bash

set -x

sudo iwconfig wlan0 mode Ad-hoc
sudo iwconfig wlan0 essid BTP
sudo ifconfig wlan0 $1 netmask 255.255.255.0
