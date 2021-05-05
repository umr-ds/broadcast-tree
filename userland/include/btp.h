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

/**
 * Common header of all BTP-frames
 */
typedef struct {
    uint8_t recv_err:1; // flag set if the channel has been very noisy
    uint8_t game_fin:1; // flag set if the node has alrady finished its game
    uint8_t mutex:1; // flasg set if something-something mutex
    uint8_t unused:2;
    frame_t frame_type:3; // see frame_t enum
    uint32_t tree_id; // uinque ID for each broadcast-tree
    uint16_t seq_num; // sequence-number to distinguish frames (different from sequence number for payloads)
    uint32_t tx_pwr; // power with which this frame has been sent
    mac_addr_t parent_addr; // address of the parent of the sending node
    uint32_t high_pwr; // power whith which the sending node sends data frames
    uint32_t snd_high_pwr; // power with which the sending node WOULD send data frames, if its furthest child were to disconnect
} btp_header_t;

/**
 * Common frame structure of all BTP-frames
 */
typedef struct {
    struct ether_header eth; // ethernet-header
    btp_header_t btp;
} btp_frame_t;

/**
 * Payload frames are used to transmit payload data after the tree has been built
 */
typedef struct {
    btp_frame_t btp_frame;
    uint16_t seq_num; // payload sequence number - separate from btp sequence number
    // TODO: buffer over-read if payload_len > len(payload)?
    uint16_t payload_len;
    uint8_t payload[MAX_PAYLOAD];
} btp_payload_frame_t;

/**
 * Frame for the Path-to-Source cycle avoidance scheme
 */
typedef struct {
    btp_frame_t btp_frame;
    uint8_t depth;
    mac_addr_t parents[MAX_DEPTH];
} path_to_source_frame_t;

/**
 * Initialises the construction of a new broadcast-tree
 * 
 * @param sockfd: File-descriptor of a raw ethernet socket which will be used for sending & receiving
 * @param laddr: MAC-address of the interface which contains our raw socket
 * @param l_sockaddr: No friggin clue...
 */
void init_tree_construction(int sockfd, mac_addr_t laddr, struct sockaddr_ll l_sockaddr);

/**
 * Parse the common btp-header
 * 
 * @param in_frame: Pointer to a btp_frame_t struct which will serve as the destination
 * @param racv_frame: Pointer to a raw bitstream as it was read from raw socket
 */
void parse_header(btp_frame_t *in_frame, uint8_t *recv_frame);

/**
 * Do stuf with a received packet
 * 
 * @param racv_frame: Pointer to a raw bitstream as it was read from raw socket
 */
void handle_packet(uint8_t *recv_frame);


#endif // __BTP_H__
