#ifndef BTP_H
#define BTP_H

#include <structs.h>          // structures that are used by the code in the firmware
#include <helper.h>           // useful helper functions
#include <wrapper.h>          // wrapper definitions for functions that already exist in the firmware
#include <sendframe.h>        // sendframe functionality

#include "wifi.h"
#include "store.h"

#define BTP_HEADER_SIZE 20

// define ioctl number 600 for starting the BT construction
#define BTP_CONSTRUCTION_INIT 600

// define ioctl number 601 for storing the own mac addresss
#define BTP_SAVE_ADDRESS 601

// Ethertype for BTP. We have to check every frame.
#define BTP_ETHERTYPE_NIBBLE 0x01

// We assume, that the RSSI is a representation of SNR and signal strength.
// In Wi-Fi, the SNR can be between 5 and 30 dBm (typically).
// Therefore, we have 25 steps for 8 bit, so we have a factor of about 10.
#define RSSI_SNR_FACTOR 10

typedef struct {
    uint8_t used_tx_pwr; // The transmit power of this particular frame.
    uint8_t max_tx_pwr;  // If parent: the power to reach all childs. If child: 0 (no meaning)
    uint8_t snd_tx_pwr;  // If parent: power to reach all childs except the one which requires max_tx_pwr. If child: 0 (no meaning)
    int8_t max_snr;      // the required SNR at the receiver, to decode this frame
} Power_data;

typedef struct {
    uint32_t seq_num;       // Sequence number for every frame (used as ID).
    uint16_t src_flg;       // Denotes, if this frame comes from the originator (1) or not (0).
    uint16_t ttl;           // A Time-To-Live per frame. If 0, frame will be dropped.
    uint16_t type;          // Type of the frame, can be 1 to 7.
    uint16_t fin_flg;       // In construction: 0 if unfinished, 1 otherwise. In transmission: 1 if last of data, 0 otherwise.
    uint16_t pwr_flg;       // 1, if optional power header is available, 0 otherwise.
    uint16_t route_len;     // 0, if optional route data is availble, > 0 otherwise (where the number is the amount of data).
    Power_data pwr_data;    // Holds additional power information (see typedef for more information) (optional).
    MAC *route_data;        // Holds the route from source until here (optional).
    char *data;             // The payload of the frame (optional).
} BTP;

enum btp_type {
    neighbour_discovery,
    child_request,
    parent_rejection,
    parent_revocation,
    parnet_confirmation,
    local_end,
    data
};

uint8_t calculate_min_tx_pwr(uint8_t sender_tx_pwr, uint8_t measured_rssi);

void build_frame(BTP *frame, uint16_t src_flg, uint16_t ttl, uint16_t type, uint16_t fin_flg, uint16_t pwr_flg, uint16_t route_len, Power_data *pwr_data, MAC **route_data, char **payload);
void send_packet(struct wlc_info *wlc, BTP *frame, MAC *sa, MAC *da, size_t len);

int end_of_game(BTP *btp_frame);
int check_source_node(MAC *source);
int addr_is_in_route(MAC *addr, MAC *addr_list, uint16_t route_len);

#endif /*BTP_H*/
