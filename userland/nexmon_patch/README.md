# nexmon support patch for broadcast-tree protocol userspace implementation
Wi-Fi frames with ethertype `0x88df (35039)` in their LLC that are broadcasted or destined to the own hardware address are extended with a Radiotap header before beeing forwarded to the host as Ethernet frames. A corresponding frame ending up at the host has the following structure:
```C
struct btp_frame {
    /* ethernet header */
    uint8 ether_dhost[6];
    uint8 ether_shost[6];
    uint16 ether_type;
    /* radiotap header */
    uint8   it_version;
    uint8   it_pad;
    uint16  it_len;
    uint32  it_present;
    /* radiotap fields */
    uint32  tsf_l;
    uint32  tsf_h;
    uint8   data_rate;
    uint16  chan_freq;
    uint16  chan_flags;
    int8    dbm_antsignal;
    int8    dbm_antnoise;
    /* payload */
    int     payload_len;
    uint8   payload[];
};
```

## Installing the patch on a Raspberry Pi 3B running Raspbian with kernel version 4.4, 4.9, or 4.14
1. Download and setup [nexmon](https://nexmon.org/) (commit 1ad6a827e92efa8f531594c85d6cdbc184fee3e8 or newer)
    * follow the README on [nexmon](https://nexmon.org/), but:  
        ***do not update the kernel above 4.14 (e.g. due to running `apt-get upgrade`)***  
        ***install matching kernel-headers (e.g. by using [rpi-source](https://github.com/RPi-Distro/rpi-source) instead of using `apt-get install raspberrypi-kernel-headers`)***
2. Copy this directory to `/home/pi/nexmon/patches/bcm43430a1/7_45_41_46/nexmon_btp`
3. Change directory to `/home/pi/nexmon/patches/bcm43430a1/7_45_41_46/nexmon_btp` and run `sudo -E make install-firmware`
