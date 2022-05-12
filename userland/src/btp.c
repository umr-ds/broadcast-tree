#include <iwlib.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "log.h"
#include "btp.h"
#include "helpers.h"
#include "tree.h"

self_t self = {0x0};

uint8_t *payload_recv_buf = NULL;
bool *seq_nums = NULL;
uint16_t max_seq_num = 0;
uint16_t seq_num_cnt = 0;
bool payload_complete = false;

char *dummy = NULL;
int payload_transmit_time = 0;

bool max_power;

extern struct sockaddr_ll L_SOCKADDR;

ssize_t send_btp_frame(uint8_t *buf, size_t data_len, int8_t tx_pwr);
int disconnect_child(any_t item, any_t args);
void disconnect_all_children(void);
void send_payload(void);
void cycle_detection_ping(eth_radio_btp_pts_t *in_frame);
int8_t compute_tx_pwr(eth_radio_btp_t *in_frame);
bool should_switch(btp_header_t header, int8_t new_parent_tx);
void establish_connection(mac_addr_t potential_parent_addr, int8_t new_parent_tx, int8_t high_pwr, int8_t snd_high_pwr, uint32_t tree_id);
void handle_discovery(eth_radio_btp_t *in_frame);
void reject_child(eth_radio_btp_t *in_frame);
void accept_child(eth_radio_btp_t *in_frame, int8_t child_tx_pwr);
void handle_child_request(eth_radio_btp_t *in_frame);
void handle_child_confirm(eth_radio_btp_t *in_frame);
void handle_child_reject(eth_radio_btp_t *in_frame);
void handle_parent_revocation(eth_radio_btp_t *in_frame);
void handle_end_of_game(eth_radio_btp_t *in_frame);
void handle_cycle_detection_ping(uint8_t *recv_frame);
void forward_payload(eth_radio_btp_payload_t *in_frame);
void handle_data(uint8_t *recv_frame);
bool self_is_pending(void);
bool self_has_children(void);

bool self_is_connected(void) {
    return self.parent || self.is_source;
}

bool self_is_pending(void) {
    return self.pending_parent;
}

bool self_has_children(void) {
    return hashmap_length(self.children) != 0;
}

ssize_t send_btp_frame(uint8_t *buf, size_t data_len, int8_t tx_pwr) {
#ifdef NEXMON
    // TODO implement with nexmon.
    return -1;
#else
    set_tx_pwr(set_pwr(tx_pwr));
    ssize_t sent_bytes = sendto(self.sockfd, buf, data_len, 0, (struct sockaddr *) &L_SOCKADDR, sizeof(struct sockaddr_ll));
    if (sent_bytes < 0) {
        log_error("Could not send data. [error: %s]", strerror(errno));
    }

    log_debug("Successfully sent frame. [sent_bytes: %i, tx_pwr: %i]", sent_bytes, set_pwr(tx_pwr));

    return sent_bytes;
#endif
}

void init_self(mac_addr_t laddr, char *payload, char *if_name, int sockfd) {
    bool is_source = strlen(payload) == 0 ? false : true;
    self.is_source = is_source;
    self.children = hashmap_new();
    self.parent_blocklist = hashmap_new();
    self.max_pwr = 0;
    self.high_pwr = 0;
    self.snd_high_pwr = 0;
    memcpy(self.laddr, laddr, 6);
    self.parent = NULL;
    self.pending_parent = NULL;
    self.tree_id = is_source ? gen_tree_id(self.laddr) : 0;
    self.sockfd = sockfd;
    self.game_fin = false;
    self.round_unchanged_cnt = 0;

    strncpy(self.if_name, if_name, IFNAMSIZ);

    self.payload_fd = open(payload, O_RDONLY);

    dummy = (char *) malloc(sizeof(char));
}

int disconnect_child(any_t item, any_t args) {
    (void)(item);
    child_t *tmp_child = (child_t *) args;

    eth_btp_t child_rejection_frame = { 0x0 };
    build_frame(&child_rejection_frame, tmp_child->addr, 0, 0, child_reject, self.tree_id, self.max_pwr);

    send_btp_frame((uint8_t *) &child_rejection_frame, sizeof(eth_btp_t), self.max_pwr);

    log_debug("Child disconnected. [addr: %s]", mac_to_str(tmp_child->addr));

    return MAP_OK;
}

void disconnect_all_children(void) {
    if (hashmap_length(self.children) == 0) {
        return;
    }

    hashmap_iterate(self.children, disconnect_child, NULL);

    hashmap_free(self.children);

    self.children = hashmap_new();

    self.high_pwr = 0;
    self.snd_high_pwr = 0;

    log_debug("All children disconnected.");
}

void send_payload(void) {
    log_info("Starting sending payload.");

    payload_transmit_time = get_time_msec();
    int bytes_read = 1;
    uint16_t seq_num = 0;

    lseek(self.payload_fd, 0, SEEK_SET);

    struct stat file_stats;
    fstat(self.payload_fd, &file_stats);

    eth_btp_t payload_base = { 0x0 };
    mac_addr_t bcast_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    build_frame(&payload_base, bcast_addr, 0, 0, data, self.tree_id, self.high_pwr);

    eth_btp_payload_t payload_frame = { 0x0 };
    payload_frame.btp_frame = payload_base;
    payload_frame.payload_len = file_stats.st_size;

    log_debug("Starting chunking file for transfer.");
    while (bytes_read > 0) {
        if ((bytes_read = read(self.payload_fd, payload_frame.payload, MAX_PAYLOAD)) < 0) {
            log_error("Could not read from file. [error: %s]", strerror(errno));
            return;
        }

        if (bytes_read == 0) {
            log_debug("Done reading file.");
            return;
        }

        payload_frame.payload_chunk_len = bytes_read;
        payload_frame.seq_num = seq_num++;

        if (send_btp_frame((uint8_t *) &payload_frame, BTP_PAYLOAD_HEADER_SIZE + payload_frame.payload_chunk_len, self.high_pwr) < 0) {
            return;
        }

        log_debug("Successfully sent next chunk. [bytes_read: %i, seq_num: %i]", bytes_read, payload_frame.seq_num);
    }

    log_info("Completely sent file.");
}

void cycle_detection_ping(eth_radio_btp_pts_t *in_frame) {
    log_info("Sending Ping to Source cycle detection frame.");
    eth_btp_t pts_base = { 0x0 };
    build_frame(&pts_base, self.parent->addr, 0, 0, ping_to_source, self.tree_id, self.parent->own_pwr);

    eth_btp_pts_t pts_frame = { 0x0 };
    pts_frame.btp_frame = pts_base;

    // If the in_frame is given, we are meant to forward a ping to source to our parent.
    // Thus, copy the ping to source relevant stuff.
    if (in_frame) {
        memcpy(pts_frame.sender, in_frame->sender, 6);
        memcpy(pts_frame.new_parent, in_frame->new_parent, 6);
        memcpy(pts_frame.old_parent, in_frame->old_parent, 6);
        log_debug("Forwarding Ping to Source frame. [sender: %s, destination: %s]", mac_to_str(pts_frame.sender), mac_to_str(self.parent->addr));
    } else {
        memcpy(pts_frame.sender, self.laddr, 6);
        memcpy(pts_frame.new_parent, self.parent->addr, 6);
        if (self.prev_parent){
            memcpy(pts_frame.old_parent, self.prev_parent->addr, 6);
        }
        log_debug("Added missing parts to ping to source frame. [sender: %s, destination: %s]", mac_to_str(pts_frame.sender), mac_to_str(self.parent->addr));
    }

    /* Send packet */
    send_btp_frame((uint8_t *) &pts_frame, sizeof(eth_btp_pts_t), self.parent->own_pwr);

    log_info("Sent cycle detection frame. [tx_pwr: %i]", self.parent->own_pwr);
}

void broadcast_discovery() {
    eth_btp_t discovery_frame = { 0x0 };
    mac_addr_t bcast_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    build_frame(&discovery_frame, bcast_addr, 0, 0, discovery, self.tree_id, self.max_pwr);

    /* Send packet */
    send_btp_frame((uint8_t *) &discovery_frame, sizeof(eth_btp_t), self.max_pwr);

    log_debug("Broadcast discovery. [tx_pwr: %i]", self.max_pwr);
}

void parse_header(eth_radio_btp_t *in_frame, uint8_t *recv_frame) {
    memcpy(in_frame, recv_frame, sizeof(eth_radio_btp_t ));
    pprint_frame(in_frame);
    log_debug("Parsed header.");
}

int8_t compute_tx_pwr(eth_radio_btp_t *in_frame) {
    log_info("Computing TX power.");
    int8_t old_tx_power = in_frame->btp.tx_pwr;
    int8_t signal = in_frame->radiotap.dbm_antsignal;
    int8_t noise = in_frame->radiotap.dbm_antnoise;

    int32_t snr = signal - noise;

    int32_t new_tx_power = old_tx_power - (snr - MINIMAL_SNR);

    if (new_tx_power < -127 || new_tx_power > 128) {
        log_warn("Calculated tx power would overflow. [new_tx_power: %i]", new_tx_power);
    }

    int8_t result = (int8_t) (new_tx_power < 0 ? 0 : new_tx_power);

    log_debug("Computed tx power. [RSSI: %i, noise: %i, SNR: %i, sender tx: %i, computed tx: %i, result: %i]", signal, noise, snr, old_tx_power, new_tx_power, result);

    return result;
}

bool should_switch(btp_header_t header, int8_t new_parent_tx) {
    log_info("Checking if should switch to new parent.");
    // If parent's sending power to reach us is lower than the power
    // the parent is currently using, we should not switch, since we
    // are not the furthest away child
    if (self.parent->own_pwr < self.parent->high_pwr) {
        log_debug("Our parents power to reach us is lower than its current sending power. [reach power: %i, parent tx power: %i]", self.parent->own_pwr, self.parent->high_pwr);
        return false;
    }

    // if the parent is already sending with a higher power than is needed to reach us,
    // then switching is essentially free. Just switch without further computations.
    if (new_parent_tx < header.high_pwr) {
        log_debug("The potential parent's new tx power is lower than it's currently sending. [new_tx_pwr: %i, tx_pwr: %i]", new_parent_tx, header.high_pwr);
        return true;
    }

    // The difference between the old parent's current sending power to reach us and
    // its gains when not connected to us.
    int32_t gain = self.parent->own_pwr - self.parent->snd_high_pwr;

    // The difference between the new parents new sending power to reach us and its
    // current highest sending power, i.e., how bad would it be to switch to this parent.
    int32_t loss = new_parent_tx - header.high_pwr;

    bool _should_switch = gain > loss;

    // If the old parent would gain more than the new loses, we should switch.
    log_debug("Gain vs. loss comparison: [gain: %i, loss: %i, should_switch: %s]", gain, loss, _should_switch ? "true" : "false");
    return _should_switch;
}

void establish_connection(mac_addr_t potential_parent_addr, int8_t new_parent_tx, int8_t high_pwr, int8_t snd_high_pwr, uint32_t tree_id) {
    log_info("Establishing connection. [addr: %s]", mac_to_str(potential_parent_addr));
    self.pending_parent = (parent_t *) malloc(sizeof(parent_t));

    memcpy(self.pending_parent->addr, potential_parent_addr, 6);
    self.pending_parent->own_pwr = new_parent_tx;
    self.pending_parent->high_pwr = high_pwr;
    self.pending_parent->snd_high_pwr = snd_high_pwr;
    self.pending_parent->last_seen = get_time_msec();

    if (self.prev_parent) free(self.prev_parent);
    self.prev_parent = self.parent;

    log_debug("Updated self. [own_prw: %i, high_pwr: %i, snd_high_pwr: %i, last_seen: %i]", self.pending_parent->own_pwr, self.pending_parent->high_pwr, self.pending_parent->snd_high_pwr, self.pending_parent->last_seen);

    eth_btp_t child_request_frame = { 0x0 };
    build_frame(&child_request_frame, potential_parent_addr, 0, 0, child_request, tree_id, new_parent_tx);

    send_btp_frame((uint8_t *) &child_request_frame, sizeof(eth_btp_t), new_parent_tx);

    log_debug("Sent child request. [tx_pwr: %i]", new_parent_tx);
}

void handle_discovery(eth_radio_btp_t *in_frame) {
    log_info("Received Discovery frame.");

    child_t *potential_child;
    char key[18] = { 0x0 };
    prepare_key(in_frame->eth.ether_shost, key);
    if (hashmap_get(self.children, key, (void **)&potential_child) == MAP_OK) {
        if (memcmp(self.laddr, in_frame->btp.parent_addr, sizeof(mac_addr_t)) != 0) {
            log_warn("We are not the parent of this child anymore. Removing child. [child addr: %s, new parent add: %s]",
                     mac_to_str(in_frame->eth.ether_shost), mac_to_str(in_frame->btp.parent_addr));
            handle_parent_revocation(in_frame);
        } else {
            potential_child->game_fin = in_frame->btp.game_fin;
        }
    }


    // if we are the tree's source, we don't want to connect to anyone
    if (self.is_source && in_frame->btp.tree_id == self.tree_id) {
        log_debug("We are either the source or the tree ID is not matching. [is_source: %s, received tree ID: %i, our tree ID: %i]", self.is_source ? "true" : false, in_frame->btp.tree_id, self.tree_id);
        return;
    }

    // if we are currently trying to connect to a new parent node, we ignore other announcements
    if (self_is_pending()) {
        log_debug("Already waiting for connection. [addr: %s]", mac_to_str(self.pending_parent->addr));
        return;
    }

    // If the address is in the parent blocklist, we won't attempt to connect again
    if (hashmap_get(self.parent_blocklist, key, (void **)&dummy) == MAP_OK) {
        log_debug("Already ignoring potential parent. [addr: %s]", mac_to_str(in_frame->eth.ether_shost));
        return;
    }

    // If we receive a discovery from our parent, update our own state.
    if (self_is_connected() && memcmp(in_frame->eth.ether_shost, self.parent->addr, 6) == 0) {
        self.parent->high_pwr = in_frame->btp.high_pwr;
        self.parent->snd_high_pwr = in_frame->btp.snd_high_pwr;
        self.parent->last_seen = get_time_msec();
        log_debug("Updated parent information. [high_pwr: %i, snd_high_pwr: %i, last_seen: %i]", self.parent->high_pwr, self.parent->snd_high_pwr, self.parent->last_seen);
        return;
    }

    // if we are connected to a parent, and have finished "the game", we don't try to switch anymore
    if (self_is_connected() && self.game_fin) {
        log_debug("Game finished, doing nothing.");
        return;
    }

    mac_addr_t potential_parent_addr;
    memcpy(potential_parent_addr, in_frame->eth.ether_shost, 6);

    int8_t new_parent_tx = compute_tx_pwr(in_frame);

    bool is_connected = self_is_connected();
    if (!is_connected && !self.is_source) {
        establish_connection(potential_parent_addr, new_parent_tx, in_frame->btp.high_pwr, in_frame->btp.snd_high_pwr, in_frame->btp.tree_id);
        log_debug("We are not connected. [is_connected: %s]", is_connected ? "true" : false);
        return;
    }

    bool _should_switch = should_switch(in_frame->btp, new_parent_tx);
    if (_should_switch) {
        establish_connection(potential_parent_addr, new_parent_tx, in_frame->btp.high_pwr, in_frame->btp.snd_high_pwr, in_frame->btp.tree_id);
        log_debug("We should switch to new parent. [should_switch: %s, new parent addr: %s]", _should_switch ? "true" : "false", mac_to_str(potential_parent_addr));
        return;
    }
}

void reject_child(eth_radio_btp_t *in_frame) {
    eth_btp_t child_rejection_frame = { 0x0 };
    build_frame(&child_rejection_frame, in_frame->eth.ether_shost, 0, 0, child_reject, in_frame->btp.tree_id, self.max_pwr);

    send_btp_frame((uint8_t *) &child_rejection_frame, sizeof(eth_btp_t), self.max_pwr);
    log_info("Rejected child. [addr: %s]", mac_to_str(in_frame->eth.ether_shost));
}

void accept_child(eth_radio_btp_t *in_frame, int8_t child_tx_pwr) {
    child_t *new_child = (child_t *) malloc(sizeof(child_t));
    memcpy(new_child->addr, in_frame->eth.ether_shost, 6);
    new_child->tx_pwr = child_tx_pwr;

    // if we have already finished the game, all our children will also finish theirs. Otherwise, we save the child's current state
    if (self.game_fin) {
        new_child->game_fin = true;
    } else {
        new_child->game_fin = in_frame->btp.game_fin;
    }

    char *key = (char *) malloc(18);
    prepare_key(new_child->addr, key);
    if (hashmap_put(self.children, key, new_child) != MAP_OK) {
        log_error("Have to reject child because hashmap error. [addr: %s]", key);
        reject_child(in_frame);
        return;
    } else {
        log_debug("Added child to hashmap. [addr: %s]", key);
    }

    if (child_tx_pwr > self.high_pwr) {
        self.snd_high_pwr = set_pwr(self.high_pwr);
        self.high_pwr = set_pwr(child_tx_pwr);
    } else if (child_tx_pwr > self.snd_high_pwr) {
        self.snd_high_pwr = set_pwr(child_tx_pwr);
    }

    eth_btp_t child_confirm_frame = { 0x0 };
    build_frame(&child_confirm_frame, in_frame->eth.ether_shost, 0, 0, child_confirm, in_frame->btp.tree_id, child_tx_pwr);

    send_btp_frame((uint8_t *) &child_confirm_frame, sizeof(eth_btp_t), child_tx_pwr);

    log_info("Accepted child. [addr: %s]", key);
}

void handle_child_request(eth_radio_btp_t *in_frame) {
    // If we are not the source and are not connected, then there is nothing to do
    if (!self_is_connected()) {
        log_debug("We are the source or are not connected. Returning. [is_source: %s, is_connected: %s]", self.is_source ? "true": "false", self_is_connected() ? "true": "false");
        return;
    }

    // We are not asked for our tree.
    if (in_frame->btp.tree_id != self.tree_id) {
        log_debug("Wrong tree ID. [received ID: %i, our ID: %i]", in_frame->btp.tree_id, self.tree_id);
        return;
    }

    log_info("Received Child Request frame. [addr: %s]", mac_to_str(in_frame->eth.ether_shost));
    if (already_child(in_frame->eth.ether_shost)) {
        log_debug("Child is aready our child. [addr: %s]", mac_to_str(in_frame->eth.ether_shost));
        return;
    }

    int8_t potential_child_send_pwr = compute_tx_pwr(in_frame);
    if ((!self_is_connected())
        || hashmap_length(self.children) >= BREADTH
        || potential_child_send_pwr > self.max_pwr
       ) {
        log_info("Rejecting child (either we have no parent, are the source, have too much children or sending power would be too high). [is_connected: %s, is_source: %s, num children: %i, potential send power: %i, self_max_power: %i]",
                 self_is_connected() ? "true" : "false", self.is_source ? "true" : "false", hashmap_length(self.children), potential_child_send_pwr, self.max_pwr);
        reject_child(in_frame);
    } else {
        accept_child(in_frame, potential_child_send_pwr);
        self.round_unchanged_cnt = 0;
    }
}

void disconnect_from_parent(void) {
    if (self.is_source) {
        return;
    }

    eth_btp_t disconnect_frame = { 0x0 };
    build_frame(&disconnect_frame, self.parent->addr, 0, 0, parent_revocaction, self.tree_id, self.max_pwr);

    send_btp_frame((uint8_t *)&disconnect_frame, sizeof(eth_btp_t), self.max_pwr);

    log_info("Disconnected from parent. [addr: %s]", mac_to_str(self.parent->addr));
    free(self.parent);
    self.parent = NULL;
}

void handle_child_confirm(eth_radio_btp_t *in_frame) {
    // If we do not wait for an CHILD CONFIRM, ignore.
    if (!self_is_pending()) {
        log_debug("We are not waiting for a child confirm.");
        return;
    }

    // We received a confirmation from a parent we never asked. Ignore.
    if (memcmp(in_frame->eth.ether_shost, self.pending_parent->addr, 6) != 0) {
        log_debug("Received child confirm from unknown node. [potential parent: %s, received addr: %s]", mac_to_str(self.pending_parent->addr), mac_to_str(in_frame->eth.ether_shost));
        return;
    }

    // If we are already connected, disconnect from the current parent
    if (self_is_connected()) {
        log_debug("Disconnected from old parent to connect to new parent. [old parent: %s, new parent: %s]", mac_to_str(self.parent->addr),
                  mac_to_str(in_frame->eth.ether_shost));
        disconnect_from_parent();
    }

    log_info("Parent confirmed our request. [addr: %s]", mac_to_str(in_frame->eth.ether_shost));

    self.tree_id = in_frame->btp.tree_id;

    self.parent = self.pending_parent;
    self.parent->last_seen = get_time_msec();
    self.pending_parent = NULL;

    if (self_has_children()) {
        cycle_detection_ping(NULL);
        log_debug("We have children, sent ping.");
    }

    // if we have not yet finished our part of the game, reset the unchanged-round-counter and assume our parent's fin-state
    if (!self.game_fin) {
        self.round_unchanged_cnt = 0;
        self.game_fin = in_frame->btp.game_fin;
        log_debug("Our game not finished yet, resetting counter. [self.game_fin: %s]", self.game_fin ? "true" : "false");
    }
}

void handle_child_reject(eth_radio_btp_t *in_frame) {
    // If we are not currently pending, but receive a reject from our current parent, then we are no longer part of the tree and disconnect all our children as well.
    if (!self_is_pending()) {
        if (memcmp(in_frame->eth.ether_shost, self.parent->addr, 6) != 0) {
            log_debug("Ignoring child reject from node that is not our parent. [addr: %s, parent addr: %s]", mac_to_str(in_frame->eth.ether_shost), mac_to_str(self.parent->addr));
            return;
        }

        log_info("Received disconnection command from our parent. [addr: %s]", mac_to_str(self.parent->addr));

        disconnect_all_children();
        free(self.parent);
        self.parent = NULL;
        log_debug("Disconnected all children and reset parent.");
        return;
    }

    // We received a confirmation from a node we never asked. Ignore.
    if (memcmp(in_frame->eth.ether_shost, self.pending_parent->addr, 6) != 0) {
        log_debug("Ignoring child reject from unknown node. [addr: %s, parent addr: %s]", mac_to_str(in_frame->eth.ether_shost), mac_to_str(self.pending_parent->addr));
        return;
    }

    if (self.prev_parent && (memcmp(in_frame->eth.ether_shost, self.prev_parent->addr, 6) == 0)) {
        log_debug("Were unable to reconnect to previous parent. [prev parent addr: %s]", mac_to_str(self.prev_parent->addr));

        disconnect_all_children();
        free(self.prev_parent);
        self.prev_parent = NULL;
        free(self.pending_parent);
        self.pending_parent = NULL;
        return;
    }

    log_info("Pending parent rejected our request. [pending parent: %s]", mac_to_str(in_frame->eth.ether_shost));
    free(self.pending_parent);
    self.pending_parent = NULL;

    char *key = (char *) malloc(18);
    prepare_key(in_frame->eth.ether_shost, key);
    hashmap_put(self.parent_blocklist, key, (void**) &dummy);
    log_info("Blocked pending parent. [addr: %s]", key);
}

void handle_parent_revocation(eth_radio_btp_t *in_frame) {
    // If we do not have a tree id (i.e., we are not part of a tree) or the received tree id does not match ours, ignore.
    if (!self_is_connected() || self.tree_id != in_frame->btp.tree_id) {
        log_debug("We are either not connected or received wring tree id. [is_connected: %s, our tree ID: %i, received tree ID: %i]", self_is_connected() ? "true" : "false", self.tree_id, in_frame->btp.tree_id);
        return;
    }

    child_t *child = (child_t *) malloc(sizeof(child_t));

    char key[18] = { 0x0 };
    prepare_key(in_frame->eth.ether_shost, key);
    if (hashmap_get(self.children, key, (void **)&child) == MAP_MISSING) {
        log_warn("Received from from unknown node. Ignoring. [addr: %s]", key);
        return;
    }

    self.round_unchanged_cnt = 0;

    if (hashmap_remove(self.children, key) == MAP_MISSING) {
        log_warn("Could not remove child. [addr: %s]", key);
    }

    if (child->tx_pwr == self.high_pwr) {
        self.high_pwr = set_pwr(self.snd_high_pwr);
        self.snd_high_pwr = set_pwr(get_snd_pwr());
    } else if (child->tx_pwr == self.snd_high_pwr) {
        self.snd_high_pwr = set_pwr(get_snd_pwr());
    }

    free(child);

    log_info("Removed child due to parent revocation. [addr: %s]", key);
}

void handle_end_of_game(eth_radio_btp_t *in_frame) {
    // If we do not have a tree id (i.e., we are not part of a tree) or the received tree id does not match ours, ignore.
    if (!self_is_connected() || self.tree_id != in_frame->btp.tree_id) {
        log_debug("We are either not connected or received wrong tree id. [is_connected: %s, our tree ID: %i, received tree ID: %i]", self_is_connected() ? "true" : "false", self.tree_id, in_frame->btp.tree_id);
        return;
    }

    child_t *child = { 0x0 };

    char key[18] = { 0x0 };
    prepare_key(in_frame->eth.ether_shost, key);
    if (hashmap_get(self.children, key, (void **)&child) == MAP_MISSING) {
        log_warn("Not our child. Ignoring. [addr: %s]", key);
        return;
    }

    child->game_fin = true;

    log_info("Child has finished its game. [addr: %s]", key);
}

void handle_cycle_detection_ping(uint8_t *recv_frame) {
    // If we are the source or we have no parent, we are not able to forward this ping
    if (self.is_source || !self_is_connected()) {
        log_info("Have no parent, not forwarding Ping to Source. [is_source: %s, is_connected: %s]", self.is_source ? "true" : "false", !self_is_connected() ? "true" : "false");
        return;
    }

    eth_radio_btp_pts_t in_frame = { 0x0 };
    memcpy(&in_frame, recv_frame, sizeof(eth_radio_btp_pts_t));

    // If the received tree id does not match ours, ignore.
    if (self.tree_id != in_frame.btp_frame.btp.tree_id) {
        log_debug("Not our tree ID. [our tree ID: %i, received tree ID: %i]", self.tree_id, in_frame.btp_frame.btp.tree_id);
        return;
    }

    if (memcmp(self.laddr, in_frame.sender, 6) == 0) {
        log_warn("Detected potential cycle! [our addr: %s, sender addr: %s]", mac_to_str(self.laddr), mac_to_str(in_frame.sender));
        if (memcmp(self.parent->addr, in_frame.new_parent, 6) != 0) {
            // Here, our current parent and the parent in question from the Ping to Source frame do not match.
            // This means, that in the time from connecting to the parent in question and receiving this
            // Ping to Source, we probably already connected to another parent already and the cycle is already broken.
            log_info("Potential cycle turned out already broken. Ignoring this Ping to Source message. [our addr: %s, sender addr: %s]", mac_to_str(self.laddr), mac_to_str(in_frame.sender));
            return;
        } else {
            // So, we really detected a cycle. We disconnect from our parent and reconnect to the old parent.
            disconnect_from_parent();
            log_info("Disconnected from parent due to cycle.");

            if (!self.prev_parent){
                disconnect_all_children();
                log_warn("Have no previous parent, can not re-connect. Disconnected from all children.");
                return;
            }

            establish_connection(self.prev_parent->addr, self.max_pwr, self.prev_parent->high_pwr, self.prev_parent->snd_high_pwr, self.tree_id);
        }
        // In this case, we received our own ping to source frame, which indicated a cycle.
    } else {
        // We just receive a ping to source as an intermediate node. Simply forward.
        log_info("Forwarding Ping to Source.");
        cycle_detection_ping(&in_frame);
    }
}

void game_round(int cur_time) {
    // if we are currently waiting to connect to a new parent, we don't modify our state, since we are about to change the topology
    if (self_is_pending()) {
        log_debug("Currently pending new connection. [addr: %s]", mac_to_str(self.pending_parent->addr));
        if (cur_time >= self.pending_parent->last_seen + PENDING_TIMEOUT) {
            log_warn("Pending parent did not respond in time. Removing pending parent. [addr: %s]", mac_to_str(self.pending_parent->addr));
            free(self.pending_parent);
            self.pending_parent = NULL;
        }
        return;
    }

    // the end-of-game-state is final and once we reach it, we never switch back out of it
    if (self.game_fin) {
        log_debug("Game already finished, doing nothing");
        if (self.is_source && cur_time - payload_transmit_time > RETRANSMIT_TIMEOUT) {
            send_payload();
            log_debug("Re-sent payload.");
        }
        return;
    }

    if (self_is_connected()) {
        self.round_unchanged_cnt++;
        log_debug("Increased unchanged counter. [is_source: %s, is_connected: %s]", self.is_source ? "true" : "false", self_is_connected() ? "true" : "false");
    }

    log_debug("Current counter [counter: %i]", self.round_unchanged_cnt);

    bool children_fin = all_children_fin();
    if (self.round_unchanged_cnt >= MAX_UNCHANGED_ROUNDS && children_fin) {
        log_info("Ending game. [unchanged counter: %i, children_fin: %s]", self.round_unchanged_cnt, children_fin ? "true" : "false");
        self.game_fin = true;

        if (self.is_source) {
            send_payload();
            log_debug("Sent payload.");
        } else {
            eth_btp_t eog = {0x0};
            build_frame(&eog, self.parent->addr, 0, 0, end_of_game, self.tree_id, self.max_pwr);

            send_btp_frame((uint8_t *) &eog, sizeof(eth_btp_t), self.max_pwr);
            log_debug("Sent end of game frame. [parent addr: %s, tx power: %i]", mac_to_str(self.parent->addr), self.max_pwr);
        }
    }
}

void forward_payload(eth_radio_btp_payload_t *in_frame) {
    eth_radio_btp_t base_in_frame = in_frame->btp_frame;
    eth_btp_t base_frame = { 0x0 };
    build_frame(&base_frame, base_in_frame.eth.ether_dhost, base_in_frame.btp.recv_err, base_in_frame.btp.mutex, base_in_frame.btp.frame_type, base_in_frame.btp.tree_id, self.high_pwr);

    eth_btp_payload_t out_frame = { 0x0 };
    out_frame.btp_frame = base_frame;
    out_frame.seq_num = in_frame->seq_num;
    out_frame.payload_len = in_frame->payload_len;
    out_frame.payload_chunk_len = in_frame->payload_chunk_len;
    memcpy(out_frame.payload, in_frame->payload, in_frame->payload_chunk_len);

    if (send_btp_frame((uint8_t *) &out_frame, BTP_PAYLOAD_HEADER_SIZE + out_frame.payload_chunk_len, self.high_pwr) < 0) {
        log_warn("Could not forward payload. [seq num: %i]", out_frame.seq_num);
    } else {
        log_info("Successfully forwarded payload. [seq num: %i]", out_frame.seq_num);
    }
}

void handle_data(uint8_t *recv_frame) {
    eth_radio_btp_payload_t in_frame = { 0x0 };
    memcpy(&in_frame, recv_frame, sizeof(eth_radio_btp_payload_t));

    // If the received tree id does not match ours, ignore.
    if (self.tree_id != in_frame.btp_frame.btp.tree_id) {
        log_debug("This data frame is not for our tree. [our tree ID: %i, received tree ID: %i]", self.tree_id, in_frame.btp_frame.btp.tree_id);
        return;
    }

    log_info("Received data frame. [payload size: %i, addr: %s, seq num: %i]", in_frame.payload_len, mac_to_str(in_frame.btp_frame.eth.ether_shost), in_frame.seq_num);

    // If we are the source, we do not want any payload.
    // However, we assume that the payload is possible completely sent, as we are
    // already receiving data frames from other nodes.
    if (self.is_source) {
        payload_complete = true;
        log_debug("Only forwarding data frame. [is_source: %s]", self.is_source ? "true" : "false");
        return;
    }

    if (!payload_recv_buf) {
        max_seq_num = (in_frame.payload_len / MAX_PAYLOAD) + 1;
        payload_recv_buf = (uint8_t *) malloc(in_frame.payload_len);
        seq_nums = (bool *) malloc(sizeof(bool) * max_seq_num);
        memset(seq_nums, 0, sizeof(bool) * max_seq_num);
        log_debug("Initialized receive buffer stuff. [total payload size: %i]", in_frame.payload_len);
    }

    if (!seq_nums[in_frame.seq_num]){
        uint16_t offset = in_frame.seq_num * MAX_PAYLOAD;
        memcpy(payload_recv_buf + offset, in_frame.payload, in_frame.payload_chunk_len);
        seq_nums[in_frame.seq_num] = true;
        seq_num_cnt++;
        log_debug("Wrote payload chunk. [chunk size: %i, seq num: %i]", in_frame.payload_chunk_len, in_frame.seq_num);

    }

    if (!payload_complete && seq_num_cnt >= max_seq_num) {
        int out_fd;
        int written_bytes;
        char tmp_fname[] = "btp_result_XXXXXX";

        if ((out_fd = mkstemp(tmp_fname)) < 0) {
            log_error("Could not open payload file. [error: %s]", strerror(errno));
            return;
        }

        if ((written_bytes = write(out_fd, payload_recv_buf, in_frame.payload_len)) != in_frame.payload_len) {
            log_warn("Wrote too few bytes. [written: %i, expected: %i]", written_bytes, in_frame.payload_len);
        }

        log_info("Received entire payload. [file path: %s]", tmp_fname);
        close(out_fd);
        payload_complete = true;
    }

    forward_payload(&in_frame);
}

void handle_packet(uint8_t *recv_frame) {
    eth_radio_btp_t in_frame = {0x0};
    parse_header(&in_frame, recv_frame);

    switch (in_frame.btp.frame_type) {
        case discovery:
            handle_discovery(&in_frame);
            break;
        case child_request:
            handle_child_request(&in_frame);
            break;
        case child_confirm:
            handle_child_confirm(&in_frame);
            break;
        case child_reject:
            handle_child_reject(&in_frame);
            break;
        case parent_revocaction:
            handle_parent_revocation(&in_frame);
            break;
        case end_of_game:
            handle_end_of_game(&in_frame);
            break;
        case ping_to_source:
            handle_cycle_detection_ping(recv_frame);
            break;
        case data:
            handle_data(recv_frame);
            break;

        default:
            log_error("Received unknown type");
    }
}
