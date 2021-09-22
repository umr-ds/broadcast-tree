#include <iwlib.h>
#include <sys/stat.h>

#include "log.h"
#include "btp.h"
#include "helpers.h"
#include "tree.h"

self_t self = {0x0};

char *dummy = NULL;

extern struct sockaddr_ll L_SOCKADDR;

bool self_is_connected() {
    return self.parent;
}

bool self_is_pending() {
    return self.pending_parent;
}

bool self_has_children() {
    return hashmap_length(self.children) != 0;
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

void init_self(mac_addr_t laddr, char *payload, char *if_name, int sockfd) {
    log_debug("Initializing self.");
    bool is_source = strlen(payload) == 0 ? false : true;

    self.is_source = is_source;
    self.payload = payload;
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

    dummy = (char *) malloc(sizeof(char));
}

void send_payload() {
    log_info("Will start sending payload.");

    int payload_fd;
    int bytes_read = 1;
    uint16_t seq_num = 0;

    size_t total_frame_size_base = sizeof(eth_btp_t) + sizeof(uint16_t) + sizeof(uint16_t);

    if ((payload_fd = open(self.payload,O_CREAT | O_WRONLY,S_IRUSR | S_IWUSR))== -1) {
        log_error("Could not read open file: %s", strerror(errno));
    }

    log_debug("Preparing data frame.");
    struct stat file_stats;
    fstat(payload_fd, &file_stats);

    eth_btp_t payload_base = { 0x0 };
    build_frame(&payload_base, self.parent->addr, 0, 0, data, self.tree_id, self.high_pwr);

    eth_btp_payload_t payload_frame = { 0x0 };
    payload_frame.btp_frame = payload_base;
    payload_frame.payload_len = file_stats.st_size;

    log_debug("Starting chunking file for transfer.");
    while (bytes_read > 0) {
        if ((bytes_read = read(payload_fd, payload_frame.payload, MAX_PAYLOAD)) < 0) {
            log_error("Could not read from file: %s", strerror(errno));
            return;
        }

        log_debug("Read %d bytes from file.", bytes_read);

        payload_frame.seq_num = seq_num++;

        if (send_btp_frame((uint8_t *) &payload_frame, total_frame_size_base + payload_frame.payload_len) < 0) {
            return;
        }

    }

    close(payload_fd);
    log_debug("File is completely sent.");
}

void cycle_detection_ping(eth_radio_btp_pts_t *in_frame) {
    log_info("Initializing Ping to Source cycle detection.");
    eth_btp_t pts_base = { 0x0 };
    build_frame(&pts_base, self.parent->addr, 0, 0, ping_to_source, self.tree_id, self.max_pwr);

    eth_btp_pts_t pts_frame = { 0x0 };
    pts_frame.btp_frame = pts_base;

    // If the in_frame is given, we are ment to forward a ping to source to our parent.
    // Thus, copy the ping to source relevant stuff.
    if (in_frame) {
        log_debug("Forwarding Ping to Source frame.");
        memcpy(pts_frame.sender, in_frame->sender, 6);
        memcpy(pts_frame.new_parent, in_frame->new_parent, 6);
        memcpy(pts_frame.old_parent, in_frame->old_parent, 6);
    } else {
        log_debug("Adding missing parts to ping to source frame.");
        memcpy(pts_frame.sender, self.laddr, 6);
        memcpy(pts_frame.new_parent, self.parent->addr, 6);
        if (self.prev_parent){
            memcpy(pts_frame.old_parent, self.prev_parent->addr, 6);
        }
    }

    log_debug(
            "Checking for cycle with ping to source: "
            "  Frame Type: %i; "
            "  TX Power: %i",
            ping_to_source,
            self.max_pwr
    );


    int8_t cur_tx_pwr = get_tx_pwr();
    set_max_tx_pwr();

    /* Send packet */
    send_btp_frame((uint8_t *) &pts_frame, sizeof(eth_btp_pts_t));
    set_tx_pwr(cur_tx_pwr);
}

void broadcast_discovery() {
    eth_btp_t discovery_frame = { 0x0 };
    mac_addr_t bcast_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    build_frame(&discovery_frame, bcast_addr, 0, 0, discovery, self.tree_id, self.max_pwr);

    log_debug(
            "Broadcasting discovery: "
            "  Frame Type: %i; "
            "  TX Power: %i",
            discovery,
            self.max_pwr
    );

    set_max_tx_pwr();

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
    self.pending_parent->last_seen = get_time_msec();

    set_tx_pwr(new_parent_tx);

    eth_btp_t child_request_frame = { 0x0 };
    build_frame(&child_request_frame, potential_parent_addr, 0, 0, child_request, tree_id, new_parent_tx);

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
    log_debug("Received Discovery frame.");
    // if we are the tree's source, we don't want to connect to anyone
    if (self.is_source) return;

    // if we are currently trying to connect to a new parent node, we ignore other announcements
    if (self_is_pending()) {
        log_debug("Already waiting for connection");
        return;
    }

    // If the address is in the parent blocklist, we won't attempt to connect again
    char key[18] = { 0x0 };
    prepare_key(in_frame->eth.ether_shost, key);
    if (hashmap_get(self.parent_blocklist, key, (void **)&dummy) == MAP_OK) {
        log_debug("Already ignoring potential parent.");
        return;
    }

    // If we receive a discovery from our parent, update our own state.
    if (self_is_connected() && memcmp(in_frame->eth.ether_shost, self.parent->addr, 6) == 0) {
        log_debug("Updating parent information.");
        self.parent->high_pwr = in_frame->btp.high_pwr;
        self.parent->snd_high_pwr = in_frame->btp.snd_high_pwr;
        self.parent->last_seen = get_time_msec();
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

    if (self_is_connected() && !should_switch(in_frame->btp, new_parent_tx)) return;

    establish_connection(potential_parent_addr, new_parent_tx, in_frame->btp.tree_id);
}

void reject_child(eth_radio_btp_t *in_frame) {
    log_info("Sending Child Reject frame.");
    eth_btp_t child_rejection_frame = { 0x0 };
    build_frame(&child_rejection_frame, in_frame->eth.ether_shost, 0, 0, child_reject, in_frame->btp.tree_id, self.max_pwr);

    log_info("Rejecting child.");

    int8_t cur_tx_pwr = get_tx_pwr();
    set_tx_pwr(self.max_pwr);

    send_btp_frame((uint8_t *) &child_rejection_frame, sizeof(eth_btp_t));

    set_tx_pwr(cur_tx_pwr);
}

void accept_child(eth_radio_btp_t *in_frame, int8_t child_tx_pwr) {
    log_info("Sending Child Accept frame.");
    child_t *new_child = (child_t *) malloc(sizeof(child_t));
    memcpy(new_child->addr, in_frame->eth.ether_shost, 6);
    new_child->tx_pwr = child_tx_pwr;

    // if we have already finished the game, all our children will also finish theirs. Otherwise, we save the child's current state
    if (self.game_fin) {
        new_child->game_fin = true;
    } else {
        new_child->game_fin = in_frame->btp.game_fin;
    }

    char key[18] = { 0x0 };
    prepare_key(in_frame->eth.ether_shost, key);
    if (hashmap_put(self.children, key, new_child) != MAP_OK) {
        log_error("Could not safe child %s in hashmap.", key);
        reject_child(in_frame);
    } else {
        log_debug("Added child %s to hashmap.", key);
    }

    if (child_tx_pwr > self.high_pwr) {
        self.snd_high_pwr = self.high_pwr;
        self.high_pwr = child_tx_pwr;
    } else if (child_tx_pwr > self.snd_high_pwr) {
        self.snd_high_pwr = child_tx_pwr;
    }

    set_tx_pwr(child_tx_pwr);

    eth_btp_t child_confirm_frame = { 0x0 };
    build_frame(&child_confirm_frame, in_frame->eth.ether_shost, 0, 0, child_confirm, in_frame->btp.tree_id, child_tx_pwr);

    log_info("Accepting child.");

    send_btp_frame((uint8_t *) &child_confirm_frame, sizeof(eth_btp_t));
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
        self.round_unchanged_cnt = 0;
    }
}

void disconnect_from_parent() {
    eth_btp_t disconnect_frame = { 0x0 };
    build_frame(&disconnect_frame, self.parent->addr, 0, 0, parent_revocaction, self.tree_id, self.max_pwr);

    int8_t cur_tx_pwr = get_tx_pwr();
    set_tx_pwr(self.max_pwr);

    send_btp_frame((uint8_t *)&disconnect_frame, sizeof(eth_btp_t));

    set_tx_pwr(cur_tx_pwr);
    free(self.parent);
    self.parent = NULL;
}

void handle_child_confirm(eth_radio_btp_t *in_frame) {
    // If we do not wait for an CHILD CONFIRM, ignore.
    if (!self_is_pending()) return;

    // We received a confirmation from a parent we never asked. Ignore.
    if (memcmp(in_frame->eth.ether_shost, self.pending_parent->addr, 6) != 0) return;

    // If we are already connected, disconnect from the current par
    if (self_is_connected()) disconnect_from_parent();

    log_info("Parent confirmed our request.");

    self.tree_id = in_frame->btp.tree_id;

    self.prev_parent = self.parent;
    self.parent = self.pending_parent;
    self.parent->last_seen = get_time_msec();
    self.pending_parent = NULL;

    if (self_has_children()) {
        cycle_detection_ping(NULL);
    }

    // if we have not yet finished our part of the game, reset the unchanged-round-counter and assume our parent's fin-state
    if (!self.game_fin) {
        self.round_unchanged_cnt = 0;
        self.game_fin = in_frame->btp.game_fin;
    }
}


void handle_child_reject(eth_radio_btp_t *in_frame) {
    // If we do not wait for an CHILD CONFIRM, ignore.
    if (!self_is_pending()) return;

    // We received a confirmation from a parent we never asked. Ignore.
    if (memcmp(in_frame->eth.ether_shost, self.pending_parent->addr, 6) != 0) return;

    log_info("Parent rejected our request.");
    free(self.pending_parent);
    self.pending_parent = NULL;

    char key[18] = { 0x0 };
    prepare_key(in_frame->eth.ether_shost, key);
    hashmap_put(self.parent_blocklist, key, (void**) &dummy);
}

void handle_parent_revocation(eth_radio_btp_t *in_frame) {
    child_t *child = (child_t *) malloc(sizeof(child_t));

    char key[18] = { 0x0 };
    prepare_key(in_frame->eth.ether_shost, key);
    if (hashmap_get(self.children, key, (void **)&child) == MAP_MISSING) {
        log_warn("%s is not our child. Ignoring.", key);
        return;
    }

    self.round_unchanged_cnt = 0;

    if (hashmap_remove(self.children, key) == MAP_MISSING) {
        log_warn("Could not remove child %s", key);
    }

    if (child->tx_pwr == self.high_pwr) {
        self.high_pwr = self.snd_high_pwr;
        self.snd_high_pwr = get_snd_pwr();
    } else if (child->tx_pwr == self.snd_high_pwr) {
        self.snd_high_pwr = get_snd_pwr();
    }
}

void handle_end_of_game(eth_radio_btp_t *in_frame) {
    child_t *child = { 0x0 };

    char key[18] = { 0x0 };
    prepare_key(in_frame->eth.ether_shost, key);
    if (hashmap_get(self.children, key, (void **)&child) == MAP_MISSING) {
        log_warn("%s is not our child. Ignoring.", key);
        return;
    }

    log_warn("Child %s has finished its game.", key);

    child->game_fin = true;
}

void handle_cycle_detection_ping(uint8_t *recv_frame) {
    // If we are the source or we have no parent, we are not able to forward this ping
    if (self.is_source || !self_is_connected()) {
        log_info("Have no parent, not forwarding Ping to Source");
        return;
    }

    eth_radio_btp_pts_t in_frame = { 0x0 };
    memcpy(&in_frame, recv_frame, sizeof(eth_radio_btp_pts_t));

    if (memcmp(self.laddr, in_frame.sender, 6) == 0) {
        log_warn("Detected potential cycle!");
        if (memcmp(self.parent, in_frame.new_parent, 6) != 0) {
            // Here, our current parent and the parent in question from the Ping to Source frame do not match.
            // This means, that in the time from connecting to the parent in question and receiving this
            // Ping to Source, we probably already connected to another parent already and the cycle is already broken.
            log_info("Potential cycle turned out already broken. Ignoring this Ping to Source message.");
            return;
        } else {
            // So, we really detected a cycle. We disconnect from our parent and reconnect to the old parent.
            disconnect_from_parent();
            establish_connection(self.prev_parent->addr, self.max_pwr, self.tree_id);
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
        if (cur_time >= self.pending_parent->last_seen + PENDING_TIMEOUT) {
            log_warn("Pending parent did not respond in time. Removing pending parent.");
            free(self.pending_parent);
            self.pending_parent = NULL;
        }
        log_debug("Currently pending new connection");
        return;
    }

    // the end-of-game-state is final and once we reach it, we never switch back out of it
    if (self.game_fin) {
        log_debug("Game already finished, doing nothing");
        return;
    }

    self.round_unchanged_cnt++;

    log_debug("Current unchanged counter: %d", self.round_unchanged_cnt);

    if (self.round_unchanged_cnt >= MAX_UNCHANGED_ROUNDS && all_children_fin()) {
        log_info("Unchanged-round-counter reached max, ending game.");
        self.game_fin = true;

        if (self.is_source) {
            send_payload();
        } else {
            eth_btp_t eog = {0x0};
            build_frame(&eog, self.parent->addr, 0, 0, end_of_game, self.tree_id, self.max_pwr);

            int8_t cur_tx_pwr = get_tx_pwr();
            set_tx_pwr(self.max_pwr);

            send_btp_frame((uint8_t *) &eog, sizeof(eth_btp_t));

            set_tx_pwr(cur_tx_pwr);
        }


    }
}

void handle_data(uint8_t *recv_frame) {

}

void handle_packet(uint8_t *recv_frame) {
    eth_radio_btp_t in_frame = {0x0};
    parse_header(&in_frame, recv_frame);

    switch (in_frame.btp.frame_type) {
        case discovery:
            log_debug("Received Discovery");
            handle_discovery(&in_frame);
            break;
        case child_request:
            log_debug("Received Child Request.");
            handle_child_request(&in_frame);
            break;
        case child_confirm:
            log_debug("Received Child Confirm.");
            handle_child_confirm(&in_frame);
            break;
        case child_reject:
            log_debug("Received Child Reject.");
            handle_child_reject(&in_frame);
            break;
        case parent_revocaction:
            log_debug("Received Parent Revocation.");
            handle_parent_revocation(&in_frame);
            break;
        case end_of_game:
            log_debug("Received End of Game.");
            handle_end_of_game(&in_frame);
            break;
        case ping_to_source:
            log_debug("Received Ping to Source cycle detection.");
            handle_cycle_detection_ping(recv_frame);
            break;
        case data:
            log_debug("Received data.");
            handle_data(recv_frame);
            break;

        default:
            log_error("Received unknown type");

    }
}
