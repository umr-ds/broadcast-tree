#include "btp.h"
#include "helpers.h"
#include "tree.h"

bool connected = false;
pending_connection_t pending_connection = { 0x0 };
parent_t parent = { 0x0 };
self_t self = { 0x0 };

extern struct sockaddr_ll L_SOCKADDR;
extern int sockfd;

ssize_t send_btp_frame(uint8_t *data, size_t data_len) {
#ifdef NEXMON
    // TODO
    return -1;
#else
    ssize_t sent_bytes = sendto(sockfd, data, data_len, 0, (struct sockaddr*)&L_SOCKADDR, sizeof(struct sockaddr_ll));
    if (sent_bytes < 0) {
        perror("Could not send data");
    }

    return sent_bytes;
#endif
}

void init_self(mac_addr_t laddr, uint32_t max_pwr) {
    self.max_pwr = max_pwr;
    memcpy(self.laddr, laddr, 6);
}

void init_tree_construction() {
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

    discovery_frame.btp.tree_id = gen_tree_id(self.laddr);

    memcpy(discovery_frame.eth.ether_shost, self.laddr, 6);

	/* Send packet */
    send_btp_frame((uint8_t *)&discovery_frame, sizeof(btp_frame_t));
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
    // If parent's sending power to reach us is lower than the power
    // the parent is currently using, we should not switch, since we
    // are not the furthest away child
    if (parent.own_pwr < parent.high_pwr) return false;

    // if the parent is already sending with a higher power than is needed to reach us,
    // then switching is essentially free. Just switch without further computations.
    if (new_parent_tx < header.high_pwr) return true;

    // The difference between the old parent's current sending power to reach us and
    // its gains when not connected to us.
    uint32_t gain = parent.own_pwr - parent.snd_high_pwr;

    // The difference between the new parents new sending power to reach us and its
    // current highest sending power, i.e., how bad would it be to switch to this parent.
    uint32_t loss = new_parent_tx - header.high_pwr;

    // If the old parent would gain more than the new loses, we should switch.
    return gain > loss;
}

void establish_connection(mac_addr_t potential_parent_addr, uint32_t new_parent_tx, uint32_t tree_id) {
    pending_connection.pending = true;
    memcpy(pending_connection.parent, potential_parent_addr, 6);
    pending_connection.tx_pwr = new_parent_tx;

    btp_frame_t child_request_frame = {
            .eth = {
                    .ether_dhost = { 0 },
                    .ether_shost = { 0 },
                    .ether_type = htons(BTP_ETHERTYPE)
            },
            .btp = {
                    .recv_err = 0,
                    .game_fin = 0,
                    .mutex = 0,
                    .unused = 0,
                    .frame_type = child_request,
                    .tree_id = tree_id,
                    .seq_num = 0,
                    .snd_high_pwr = 0
            }
    };

    memcpy(child_request_frame.eth.ether_shost, self.laddr, 6);
    memcpy(child_request_frame.eth.ether_dhost, potential_parent_addr, 6);

    send_btp_frame((uint8_t *)&child_request_frame, sizeof(btp_frame_t));
}

void handle_discovery(btp_frame_t *in_frame) {
    // TODO: handle updates from parent

    if (pending_connection.pending) return;

    mac_addr_t potential_parent_addr;
    memcpy(potential_parent_addr, in_frame->eth.ether_shost, 6);

    uint32_t new_parent_tx = compute_tx_pwr();

    if (connected && !should_switch(in_frame->btp, new_parent_tx)) return;

    establish_connection(potential_parent_addr, new_parent_tx, in_frame->btp.tree_id);
}

void handle_child_confirm() {

}

void handle_packet(uint8_t *recv_frame) {
    btp_frame_t in_frame = { 0x0 };
    parse_header(&in_frame, recv_frame);

    switch(in_frame.btp.frame_type) {
        case discovery:
            printf("Received Discovery\n");
            handle_discovery(&in_frame);
            break;
        case child_request:
            printf("Received Child Request. Not implemented, yet.\n");
            break;
        case child_confirm:
            printf("Received Child Confirm. Not implemented, yet.\n");
            break;
        case child_reject:
            printf("Received Child Reject. Not implemented, yet.\n");
            break;

        default:
            printf("Received unknown type\n");

    }
}
