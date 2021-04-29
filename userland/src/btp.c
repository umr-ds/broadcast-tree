#include <time.h>

#include "btp.h"
#include "helpers.h"

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

void handle_packet(uint8_t *recv_frame) {
    btp_frame_t in_frame = { 0x0 };
    parse_header(&in_frame, recv_frame);

    switch(in_frame.btp.frame_type) {
        case discovery:
            printf("Received Discovery\n");
            break;

        default:
            printf("Received unknown type\n");

    }
}
