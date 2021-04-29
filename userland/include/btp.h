#ifndef __BTP_H__
#define __BTP_H__

#include <stdlib.h>
#include <string.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <stdio.h>

#define MTU 1500
#define BTP_HEADER_SIZE 25
#define MAX_PAYLOAD (MTU - BTP_HEADER_SIZE)
#define MAX_DEPTH 20

#define POLL_TIMEOUT 1000   // timeout for poll in ms

#define BTP_ETHERTYPE 35039

typedef uint8_t mac_addr_t[6];

typedef enum {
    data,
    end_of_game,
    parent_revocaction,
    child_reject,
    child_confirm,
    child_request,
    discovery,
    path_to_source
} frame_t;

typedef struct {
    uint8_t recv_err:1;
    uint8_t game_fin:1;
    uint8_t mutex:1;
    uint8_t unused:2;
    frame_t frame_type:3;
    uint32_t tree_id;
    uint16_t seq_num;
    uint32_t tx_pwr;
    mac_addr_t parent_addr;
    uint32_t high_pwr;
    uint32_t snd_high_pwr;
} btp_header_t;

typedef struct {
    struct ether_header eth;
    btp_header_t btp;
} btp_frame_t;

typedef struct {
    struct ether_header eth;
    btp_header_t btp;
    uint16_t seq_num;
    // TODO: buffer over-read if payload_len > len(payload)?
    uint16_t payload_len;
    uint8_t payload[MAX_PAYLOAD];
} btp_payload_frame_t;

typedef struct {
    struct ether_header eth;
    btp_header_t btp;
    uint8_t depth;
    mac_addr_t parents[MAX_DEPTH];
} path_to_source_frame_t;


void init_tree_construction(int sockfd, mac_addr_t laddr, struct sockaddr_ll l_sockaddr);
void parse_header(btp_frame_t *in_frame, uint8_t *recv_frame);
void handle_packet(uint8_t *recv_frame);


#endif // __BTP_H__