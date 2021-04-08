# Nexmon-based Broadcast Tree Protocol

This folder contains everything required for running the Broadcast Tree Protocol on the Wi-Fi chip of a Raspberry Pi using the [Nexmon framework](https://github.com/seemoo-lab/nexmon).

**Note: Currently, this project only works on a Raspberry Pi 3 using Raspian Stretch with kernel version 4.14. It was not tested on other Operating Systems, Raspberry Pis or other devices supported by Nexmon!.**

## Get it running

To use this firmware patch, copy this entire folder to `patches/bcm43430a1/7_45_41_46` and follow the Nexmon [instructions](https://github.com/seemoo-lab/nexmon#steps-to-create-your-own-firmware-patches).

## Setup

Before the protocol can work, each Wi-Fi chip on every device needs to know it's own mac address. Therefore, we crated an IOCTL, which has to be executed on every device before the construction phase:

```
nexutil -s 601 -v b827eb75df9b -l 13
```

This tells the `nexutil` to issue the IOCTL `601` with payload `b827eb75df9b` (which is the MAC address of your Pi) and the payload size of `13` bytes.

## Starting the construction phase

The construction phase init is implemented using an IOCTL. Use the following command to start it:

```
nexutil -s 600
```
