#include "btp.h"
#include "helpers.h"
#include "tree.h"

self_t self = {0x0};

extern struct sockaddr_ll L_SOCKADDR;
extern int sockfd;

bool is_connected(self_t self) {
    return self.parent;
}

bool is_pending(self_t self) {
    return self.pending_parent;
}

ssize_t send_btp_frame(uint8_t *data, size_t data_len) {
#ifdef NEXMON
    // TODO
    return -1;
#else
    ssize_t sent_bytes = sendto(sockfd, data, data_len, 0, (struct sockaddr *) &L_SOCKADDR, sizeof(struct sockaddr_ll));
    if (sent_bytes < 0) {
        perror("Could not send data");
    }

    return sent_bytes;
#endif
}

void init_self(mac_addr_t laddr, uint32_t max_pwr, bool is_source) {
    self.is_source = is_source;
    self.children = hashmap_new();
    self.max_pwr = max_pwr;
    self.high_pwr = 0;
    self.snd_high_pwr = 0;
    memcpy(self.laddr, laddr, 6);
    self.parent = NULL;
    self.pending_parent = NULL;
    self.tree_id = 0;
}

void init_tree_construction() {
    btp_frame_t discovery_frame = { 0x0 };
    mac_addr_t bcast_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    build_frame(&discovery_frame, bcast_addr, 0, 0, 0, discovery, gen_tree_id(self.laddr), 0);

    /* Send packet */
    send_btp_frame((uint8_t *) &discovery_frame, sizeof(btp_frame_t));
}

void parse_header(btp_frame_t *in_frame, uint8_t *recv_frame) {
    memcpy(in_frame, recv_frame, sizeof(btp_frame_t));
    pprint_frame(in_frame);
}

uint32_t compute_tx_pwr() {
    // TODO: get SNR, RX power
    return 0;
}

bool should_switch(btp_header_t header, uint32_t new_parent_tx) {
    // If parent's sending power to reach us is lower than the power
    // the parent is currently using, we should not switch, since we
    // are not the furthest away child
    if (self.parent->own_pwr < self.parent->high_pwr) return false;

    // if the parent is already sending with a higher power than is needed to reach us,
    // then switching is essentially free. Just switch without further computations.
    if (new_parent_tx < header.high_pwr) return true;

    // The difference between the old parent's current sending power to reach us and
    // its gains when not connected to us.
    uint32_t gain = self.parent->own_pwr - self.parent->snd_high_pwr;

    // The difference between the new parents new sending power to reach us and its
    // current highest sending power, i.e., how bad would it be to switch to this parent.
    uint32_t loss = new_parent_tx - header.high_pwr;

    // If the old parent would gain more than the new loses, we should switch.
    return gain > loss;
}

void establish_connection(mac_addr_t potential_parent_addr, uint32_t new_parent_tx, uint32_t tree_id) {
    self.pending_parent = (parent_t *) malloc(sizeof(parent_t));

    memcpy(self.pending_parent->addr, potential_parent_addr, 6);
    self.pending_parent->own_pwr = new_parent_tx;

    btp_frame_t child_request_frame = { 0x0 };
    build_frame(&child_request_frame, potential_parent_addr, 0, 0, 0, child_request, tree_id, 0);

    send_btp_frame((uint8_t *) &child_request_frame, sizeof(btp_frame_t));
}

void handle_discovery(btp_frame_t *in_frame) {
    // if we are the tree's source, we don't want to connect to anyone
    if (self.is_source) return;

    // TODO: handle updates from parent

    if (is_pending(self)) return;

    mac_addr_t potential_parent_addr;
    memcpy(potential_parent_addr, in_frame->eth.ether_shost, 6);

    uint32_t new_parent_tx = compute_tx_pwr();

    if (is_connected(self) && !should_switch(in_frame->btp, new_parent_tx)) return;

    establish_connection(potential_parent_addr, new_parent_tx, in_frame->btp.tree_id);
}

void accept_child(btp_frame_t *in_frame, uint32_t child_tx_pwr) {
    child_t *new_child = (child_t *) malloc(sizeof(child_t));
    memcpy(new_child->addr, in_frame->eth.ether_shost, 6);
    new_child->tx_pwr = child_tx_pwr;

    hashmap_put(self.children, (char *)in_frame->eth.ether_shost, new_child);

    if (child_tx_pwr > self.high_pwr) {
        self.snd_high_pwr = self.high_pwr;
        self.high_pwr = child_tx_pwr;
    } else if (child_tx_pwr > self.snd_high_pwr) {
        self.snd_high_pwr = child_tx_pwr;
    }

    btp_frame_t child_confirm_frame = { 0x0 };
    build_frame(&child_confirm_frame, in_frame->eth.ether_shost, 0, 0, 0, child_confirm, in_frame->btp.tree_id, 0);

    printf("Accepting child.\n");

    send_btp_frame((uint8_t *) &child_confirm_frame, sizeof(btp_frame_t));
}

void reject_child(btp_frame_t *in_frame) {
    btp_frame_t child_rejection_frame = { 0x0 };
    build_frame(&child_rejection_frame, in_frame->eth.ether_shost, 0, 0, 0, child_reject, in_frame->btp.tree_id, 0);

    printf("Rejecting child.\n");

    send_btp_frame((uint8_t *) &child_rejection_frame, sizeof(btp_frame_t));
}

void handle_child_request(btp_frame_t *in_frame) {
    if (already_child(in_frame->eth.ether_shost)) return;

    uint32_t potential_child_send_pwr = compute_tx_pwr();
    if ((!is_connected(self) && !self.is_source)
        || hashmap_length(self.children) >= BREADTH
        || potential_child_send_pwr > self.max_pwr
       ) {
        reject_child(in_frame);
    } else {
        accept_child(in_frame, potential_child_send_pwr);
    }
}

void disconnect_from_parent() {
    btp_frame_t disconnect_frame = { 0x0 };
    build_frame(&disconnect_frame, self.parent->addr, 0, 0, 0, parent_revocaction, self.tree_id, 0);

    free(self.parent);

    send_btp_frame((uint8_t *)&disconnect_frame, sizeof(btp_frame_t));
}

void handle_child_confirm(btp_frame_t *in_frame) {
    // We received a confirmation from a parent we never asked. Ignore.
    if (is_pending(self) && memcmp(in_frame->eth.ether_shost, self.pending_parent->addr, 6) != 0) return;

    // If we are already connected, disconnect from the current par
    if (is_connected(self)) disconnect_from_parent();

    self.tree_id = in_frame->btp.tree_id;

    self.parent = self.pending_parent;
    self.pending_parent = NULL;
}

void handle_packet(uint8_t *recv_frame) {
    btp_frame_t in_frame = {0x0};
    parse_header(&in_frame, recv_frame);

    switch (in_frame.btp.frame_type) {
        case discovery:
            printf("Received Discovery\n");
            handle_discovery(&in_frame);
            break;
        case child_request:
            printf("Received Child Request.\n");
            handle_child_request(&in_frame);
            break;
        case child_confirm:
            printf("Received Child Confirm.\n");
            handle_child_confirm(&in_frame);
            break;
        case child_reject:
            printf("Received Child Reject. Not implemented, yet.\n");
            break;
        case parent_revocaction:
            printf("Received Parent Revocation. Not implemented, yet.\n");
            break;

        default:
            printf("Received unknown type\n");

    }
}
