#ifndef WIFI_H
#define WIFI_H

#define MAX_PAYLOAD_SIZE 2312

typedef struct {
    char addr[6];
} MAC;

typedef struct {
    char dsap[1];
    char ssap[1];
    char ctrl[1];
    char oui[3];
    char ethertype[2];
} LLC;

typedef struct {
    char fc[2];
    char did[2];
    MAC da;
    MAC sa;
    MAC bssid;
    char seqctl[2];
    LLC llc;
    int payload_len;
    char payload[MAX_PAYLOAD_SIZE];
} Wifi_frame;

#endif /*WIFI_H*/
