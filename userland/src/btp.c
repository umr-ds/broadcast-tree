#include <time.h>

#include "btp.h"
#include "helpers.h"
#include "tree.h"

bool connected = false;
pending_connection_t pending_connection = { 0x0 };
parent_t parent = { 0x0 };
self_t self = { 0x0 };

extern int sockfd;

void init_tree_construction(int sockfd, mac_addr_t laddr, struct sockaddr_ll l_sockaddr) {
    btp_frame_t discovery_frame = {
        .eth = {
            .ether_dhost = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
            .ether_shost = { 0 },
            .ether_type = htons(BTP_ETHERTYPE)
        },
        .btp = {
            .recv_err = 0,
            .game_fin = 0,
            .mutex = 0,
            .unused = 0,
            .frame_type = discovery,
            .tree_id = 0,
            .seq_num = 0,
            .snd_high_pwr = 0
        }
    };

    unsigned char buf[10];
    time_t cur_time = time(NULL);
    memcpy(buf, laddr, 6);
    memcpy(buf + 6, &cur_time, 4);
    discovery_frame.btp.tree_id = gen_tree_id(buf);

    memcpy(discovery_frame.eth.ether_shost, laddr, 6);

	/* Send packet */
	if (sendto(sockfd, &discovery_frame, sizeof(btp_frame_t), 0, (struct sockaddr*)&l_sockaddr, sizeof(struct sockaddr_ll)) < 0) {
	    perror("Could not send data");
        exit(1);
    }
}

void parse_header(btp_frame_t *in_frame, uint8_t *recv_frame) {
    memcpy(in_frame, recv_frame, sizeof(btp_frame_t));
    pprint_frame(in_frame);
}

uint32_t compute_tx_pwr() {
    // TODO: get SNR, RX power
    return 4;
}

bool should_switch(btp_header_t header, uint32_t new_parent_tx) {
    if (parent.own_pwr < parent.high_pwr) return false;
    uint32_t gain = parent.own_pwr - parent.snd_high_pwr;

    if (new_parent_tx < header.high_pwr) return true;
    uint32_t loss = new_parent_tx - header.high_pwr;

    return gain > loss;
}

void establish_connection(mac_addr_t potential_parent_addr, uint32_t new_parent_tx) {
    pending_connection.pending = true;
    memcpy(pending_connection.parent, potential_parent_addr, 6);
    pending_connection.tx_pwr = new_parent_tx;

    // TODO: Resume here with sending child_request frame
}

void handle_discovery(btp_frame_t *in_frame) {
    // TODO: handle updates from parent

    if (pending_connection.pending) return;

    mac_addr_t potential_parent_addr;
    memcpy(potential_parent_addr, in_frame->eth.ether_shost, 6);

    uint32_t new_parent_tx = compute_tx_pwr();

    if (connected && !should_switch(in_frame->btp, new_parent_tx)) return;

    establish_connection(potential_parent_addr, new_parent_tx);
}

void handle_packet(uint8_t *recv_frame) {
    btp_frame_t in_frame = { 0x0 };
    parse_header(&in_frame, recv_frame);

    switch(in_frame.btp.frame_type) {
        case discovery:
            printf("Received Discovery\n");
            handle_discovery(&in_frame);
            break;

        default:
            printf("Received unknown type\n");

    }
}
