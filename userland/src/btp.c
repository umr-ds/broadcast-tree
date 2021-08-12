#include <iwlib.h>

#include "log.h"
#include "btp.h"
#include "helpers.h"
#include "tree.h"

self_t self = {0x0};

extern struct sockaddr_ll L_SOCKADDR;

bool self_is_connected() {
    return self.parent;
}

bool self_is_pending() {
    return self.pending_parent;
}

ssize_t send_btp_frame(uint8_t *data, size_t data_len) {
#ifdef NEXMON
    // TODO implement with nexmon.
    return -1;
#else
    ssize_t sent_bytes = sendto(self.sockfd, data, data_len, 0, (struct sockaddr *) &L_SOCKADDR, sizeof(struct sockaddr_ll));
    if (sent_bytes < 0) {
        log_error("Could not send data: %s", strerror(errno));
    }

    log_debug("Sent %i bytes", sent_bytes);

    return sent_bytes;
#endif
}

void init_self(mac_addr_t laddr, bool is_source, char *if_name, int sockfd) {
    log_debug("Initializing self.");
    self.is_source = is_source;
    self.children = hashmap_new();
    self.max_pwr = 0;
    self.high_pwr = 0;
    self.snd_high_pwr = 0;
    memcpy(self.laddr, laddr, 6);
    self.parent = NULL;
    self.pending_parent = NULL;
    self.tree_id = is_source ? gen_tree_id(self.laddr) : 0;
    self.sockfd = sockfd;
    strncpy(self.if_name, if_name, IFNAMSIZ);
}

void init_tree_construction() {
    log_info("Starting tree construction.");
    eth_btp_t discovery_frame = { 0x0 };
    mac_addr_t bcast_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    build_frame(&discovery_frame, bcast_addr, 0, 0, 0, discovery, self.tree_id, self.max_pwr);

    log_debug(
            "Sending init tree frame: "
            "  Frame Type: %i; "
            "  TX Power: %i",
            discovery,
            self.max_pwr
    );

    /* Send packet */
    send_btp_frame((uint8_t *) &discovery_frame, sizeof(eth_btp_t));
}

void parse_header(eth_radio_btp_t *in_frame, uint8_t *recv_frame) {
    log_debug("Parsing header.");
    memcpy(in_frame, recv_frame, sizeof(eth_radio_btp_t ));
    pprint_frame(in_frame);
}

int8_t compute_tx_pwr(eth_radio_btp_t *in_frame) {
    log_info("Computing TX power.");
    int8_t old_tx_power = in_frame->btp.tx_pwr;
    int8_t signal = in_frame->radiotap.dbm_antsignal;
    int8_t noise = in_frame->radiotap.dbm_antnoise;

    int8_t snr = signal - noise;

    int8_t new_tx_power = old_tx_power - (snr - MINIMAL_SNR);

    log_debug("Peer sent with %hhi dBm (RSSI: %hhi, noise: %hhi, SNR: %hhi). New tx power is %hhi dBm.", old_tx_power, signal, noise, snr, new_tx_power);

    return new_tx_power < 0 ? 0 : new_tx_power;
}

bool should_switch(btp_header_t header, int8_t new_parent_tx) {
    log_info("Checking if should switch to new parent.");
    // If parent's sending power to reach us is lower than the power
    // the parent is currently using, we should not switch, since we
    // are not the furthest away child
    if (self.parent->own_pwr < self.parent->high_pwr) return false;
    log_debug("Parent's own_pwr (%hhi) is higher than it's high_pwr (%hhi)", self.parent->own_pwr, self.parent->high_pwr);

    // if the parent is already sending with a higher power than is needed to reach us,
    // then switching is essentially free. Just switch without further computations.
    if (new_parent_tx < header.high_pwr) return true;
    log_debug("Parent's new tx power (%hhi) is higher than it's high_pwr (%hhi)", new_parent_tx, header.high_pwr);

    // The difference between the old parent's current sending power to reach us and
    // its gains when not connected to us.
    int8_t gain = self.parent->own_pwr - self.parent->snd_high_pwr;

    // The difference between the new parents new sending power to reach us and its
    // current highest sending power, i.e., how bad would it be to switch to this parent.
    int8_t loss = new_parent_tx - header.high_pwr;

    // If the old parent would gain more than the new loses, we should switch.
    log_debug("Gain: %hhi; Loss: %hhi", gain, loss);
    return gain > loss;
}

void establish_connection(mac_addr_t potential_parent_addr, int8_t new_parent_tx, uint32_t tree_id) {
    log_info(
            "Establishing connection to %x:%x:%x:%x:%x:%x",
            potential_parent_addr[0],
            potential_parent_addr[1],
            potential_parent_addr[2],
            potential_parent_addr[3],
            potential_parent_addr[4],
            potential_parent_addr[5]
            );
    self.pending_parent = (parent_t *) malloc(sizeof(parent_t));

    memcpy(self.pending_parent->addr, potential_parent_addr, 6);
    self.pending_parent->own_pwr = new_parent_tx;

    set_tx_pwr(new_parent_tx);

    eth_btp_t child_request_frame = { 0x0 };
    build_frame(&child_request_frame, potential_parent_addr, 0, 0, 0, child_request, tree_id, new_parent_tx);

    log_debug(
            "Sending child request: "
            "Frame Type: %i; "
            "Tree ID: %li; "
            "TX Power: %i",
            discovery,
            tree_id,
            new_parent_tx
    );

    send_btp_frame((uint8_t *) &child_request_frame, sizeof(eth_btp_t));
}

void handle_discovery(eth_radio_btp_t *in_frame) {
    log_info("Received Discovery frame.");
    // if we are the tree's source, we don't want to connect to anyone
    if (self.is_source) return;

    if (self_is_pending()) return;

    if (self_is_connected() && memcmp(in_frame->eth.ether_shost, self.parent->addr, 6) == 0) {
        self.parent->high_pwr = in_frame->btp.high_pwr;
        self.parent->snd_high_pwr = in_frame->btp.snd_high_pwr;
        return;
    }

    mac_addr_t potential_parent_addr;
    memcpy(potential_parent_addr, in_frame->eth.ether_shost, 6);

    int8_t new_parent_tx = compute_tx_pwr(in_frame);

    if (self_is_connected() && !should_switch(in_frame->btp, new_parent_tx)) return;

    establish_connection(potential_parent_addr, new_parent_tx, in_frame->btp.tree_id);
}

void accept_child(eth_radio_btp_t *in_frame, int8_t child_tx_pwr) {
    log_info("Sending Child Accept frame.");
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

    set_tx_pwr(child_tx_pwr);

    eth_btp_t child_confirm_frame = { 0x0 };
    build_frame(&child_confirm_frame, in_frame->eth.ether_shost, 0, 0, 0, child_confirm, in_frame->btp.tree_id, child_tx_pwr);

    log_info("Accepting child.");

    send_btp_frame((uint8_t *) &child_confirm_frame, sizeof(eth_btp_t));
}

void reject_child(eth_radio_btp_t *in_frame) {
    log_info("Sending Child Reject frame.");
    eth_btp_t child_rejection_frame = { 0x0 };
    build_frame(&child_rejection_frame, in_frame->eth.ether_shost, 0, 0, 0, child_reject, in_frame->btp.tree_id, self.max_pwr);

    log_warn("Rejecting child.");

    int8_t cur_tx_pwr = get_tx_pwr();
    set_tx_pwr(self.max_pwr);

    send_btp_frame((uint8_t *) &child_rejection_frame, sizeof(eth_btp_t));

    set_tx_pwr(cur_tx_pwr);
}

void handle_child_request(eth_radio_btp_t *in_frame) {
    log_info("Received Child Request frame.");
    if (already_child(in_frame->eth.ether_shost)) return;

    int8_t potential_child_send_pwr = compute_tx_pwr(in_frame);
    if ((!self_is_connected() && !self.is_source)
        || hashmap_length(self.children) >= BREADTH
        || potential_child_send_pwr > self.max_pwr
       ) {
        reject_child(in_frame);
    } else {
        accept_child(in_frame, potential_child_send_pwr);
    }
}

void disconnect_from_parent() {
    eth_btp_t disconnect_frame = { 0x0 };
    build_frame(&disconnect_frame, self.parent->addr, 0, 0, 0, parent_revocaction, self.tree_id, self.max_pwr);

    int8_t cur_tx_pwr = get_tx_pwr();
    set_tx_pwr(self.max_pwr);

    send_btp_frame((uint8_t *)&disconnect_frame, sizeof(eth_btp_t));

    set_tx_pwr(cur_tx_pwr);
    free(self.parent);
}

void handle_child_confirm(eth_radio_btp_t *in_frame) {
    // We received a confirmation from a parent we never asked. Ignore.
    if (self_is_pending() && memcmp(in_frame->eth.ether_shost, self.pending_parent->addr, 6) != 0) return;

    // If we are already connected, disconnect from the current par
    if (self_is_connected()) disconnect_from_parent();

    self.tree_id = in_frame->btp.tree_id;

    self.parent = self.pending_parent;
    self.pending_parent = NULL;
}

void handle_packet(uint8_t *recv_frame) {
    eth_radio_btp_t in_frame = {0x0};
    parse_header(&in_frame, recv_frame);

    switch (in_frame.btp.frame_type) {
        case discovery:
            log_info("Received Discovery");
            handle_discovery(&in_frame);
            break;
        case child_request:
            log_info("Received Child Request.");
            handle_child_request(&in_frame);
            break;
        case child_confirm:
            log_info("Received Child Confirm.");
            handle_child_confirm(&in_frame);
            break;
        case child_reject:
            log_warn("Received Child Reject. Not implemented, yet.");
            break;
        case parent_revocaction:
            log_warn("Received Parent Revocation. Not implemented, yet.");
            break;

        default:
            log_error("Received unknown type");

    }
}
