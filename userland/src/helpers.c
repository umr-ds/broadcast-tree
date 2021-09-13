#include <time.h>
#include <tree.h>

#include "libexplain/ioctl.h"
#include "log.h"
#include "helpers.h"

extern self_t self;

bool all_children_fin() {
    int num_children = hashmap_length(self.children);
    char **keys = (char **) malloc(num_children);

    if (hashmap_get_keys(self.children, keys) != MAP_OK) {
        log_warn("Could not get children.");
        return -1;
    }

    int i;
    child_t *tmp_child = malloc(sizeof(child_t));
    for (i = 0; i < num_children; i++) {
        if(hashmap_get(self.children, keys[i], (void **) tmp_child) == MAP_MISSING) {
            log_warn("For some reason this child could not be found.");
            continue;
        }

        if (!tmp_child->game_fin) {
            return false;
        }
    }

    return true;
}

int8_t get_snd_pwr() {
    int num_children = hashmap_length(self.children);
    char **keys = (char **) malloc(num_children);

    if (hashmap_get_keys(self.children, keys) != MAP_OK) {
        log_warn("Could not get children.");
        return -1;
    }

    int i;
    int8_t high_tmp = 0;
    int8_t snd_high_tmp = 0;
    child_t *tmp_child = malloc(sizeof(child_t));
    for (i = 0; i < num_children; i++) {
        if(hashmap_get(self.children, keys[i], (void **) tmp_child) == MAP_MISSING) {
            log_warn("For some reason this child could not be found.");
            continue;
        }

        if (tmp_child->tx_pwr > high_tmp) {
            snd_high_tmp = high_tmp;
            high_tmp = tmp_child->tx_pwr;
        } else if (tmp_child->tx_pwr > snd_high_tmp) {
            snd_high_tmp = tmp_child->tx_pwr;
        }
    }

    return snd_high_tmp;
}

int get_time_msec() {
    struct timeval tval;
    gettimeofday(&tval, NULL);
    return ((tval.tv_sec * 1000000) + tval.tv_usec) / 1000;
}

bool already_child(mac_addr_t potential_child_addr) {
    child_t child = {0x0};
    if (hashmap_get(self.children, (char *)potential_child_addr, (any_t*)&child) == MAP_OK) {
        return true;
    }

    return false;
}

uint32_t gen_tree_id(mac_addr_t laddr) {
    unsigned char *str = malloc(10);

    time_t cur_time = time(NULL);
    memcpy(str, laddr, 6);
    memcpy(str + 6, &cur_time, 4);

    uint32_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

bool set_max_tx_pwr() {
    log_debug("Setting max TX power.");
    int8_t max_tx_pwr;
    if ((max_tx_pwr = get_max_tx_pwr()) < 0) {
        return false;
    }

    if (!set_tx_pwr(max_tx_pwr)) {
        return false;
    }

    return true;
}

bool set_tx_pwr(int8_t tx_pwr) {
    struct iwreq wrq;
    wrq.u.txpower.value = (int32_t) tx_pwr;
    wrq.u.txpower.fixed = 1;
    wrq.u.txpower.disabled = 0;
    wrq.u.txpower.flags = IW_TXPOW_DBM;

    if(iw_set_ext(self.sockfd, self.if_name, SIOCSIWTXPOW, &wrq) < 0) {
        log_error("Could not set txpower: %s", explain_ioctl(self.sockfd, SIOCSIWTXPOW, &wrq));
        return false;
    }

    return true;
}

int8_t get_tx_pwr() {
    struct iwreq *wrq = malloc(sizeof(struct iwreq));
    memset(wrq, 0, sizeof(struct iwreq));
    int32_t dbm;

    int res;
    /* Get current Transmit Power */
    if((res = iw_get_ext(self.sockfd, self.if_name, SIOCGIWTXPOW, wrq)) >= 0) {
        if(wrq->u.txpower.disabled) {
            log_error("Transmission is disabled.");
            return -1;
        } else {
            if(wrq->u.txpower.flags & IW_TXPOW_MWATT) {
                dbm = iw_mwatt2dbm(wrq->u.txpower.value);
            } else {
                dbm = wrq->u.txpower.value;
            }
            return (int8_t) dbm;
        }
    } else {
        log_error("Could not get current txpower: %s", explain_ioctl(self.sockfd, SIOCGIWTXPOW, wrq));
        return (int8_t) res;
    }
}

int8_t get_max_tx_pwr() {
    int8_t cur_tx_pwr;
    log_debug("Getting current TX power.");
    if ((cur_tx_pwr = get_tx_pwr()) < 0) {
        return -1;
    }

    log_debug("Setting arbitrary high TX power.");
    if (!set_tx_pwr(INT8_MAX)) {
        return -1;
    }

    log_debug("Getting highest reported TX power.");
    int8_t max_tx_pwr;
    if ((max_tx_pwr = get_tx_pwr()) < 0) {
        return -1;
    }

    log_debug("Setting old tx power.");
    if (!set_tx_pwr(cur_tx_pwr)) {
        return -1;
    }

    log_debug("Getting max txpower");
    return max_tx_pwr;
}

void hexdump(const void *data, size_t size) {
    char chars[17];
    chars[16] = '\0';

    size_t data_index;
    size_t padding_index;

    for (data_index = 0; data_index < size; ++data_index) {
        log_debug("%02x ", ((unsigned char *) data)[data_index]);

        if (((unsigned char *) data)[data_index] >= ' ' && ((unsigned char *) data)[data_index] <= '~') {
            chars[data_index % 16] = ((unsigned char *) data)[data_index];
        } else {
            chars[data_index % 16] = '.';
        }

        if ((data_index + 1) % 8 == 0 || data_index + 1 == size) {
            log_debug(" ");

            if ((data_index + 1) % 16 == 0) {
                log_debug("|  %s \n", chars);
            } else if (data_index + 1 == size) {
                chars[(data_index + 1) % 16] = '\0';

                if ((data_index + 1) % 16 <= 8) {
                    log_debug(" ");
                }

                for (padding_index = (data_index + 1) % 16; padding_index < 16; ++padding_index) {
                    log_debug("   ");
                }

                log_debug("|  %s \n", chars);
            }
        }
    }
}

void pprint_frame(eth_radio_btp_t *in_frame) {
    struct ether_header eth = in_frame->eth;
    radiotap_header_t rdio = in_frame->radiotap;
    btp_header_t btp = in_frame->btp;

    log_debug("BTP Frame:\n"
              "- Ethernet:\n"
              "    Source:............%x:%x:%x:%x:%x:%x\n"
              "    Destination:.......%x:%x:%x:%x:%x:%x\n"
              "    EtherType:.........%hu\n"
              "- RadioTap Header:\n"
              "    Version:...........%hhu\n"
              "    Length:............%hu\n"
              "    Present:...........%u\n"
              "    Time Sync:.........%u%u\n"
              "    Flags:.............%hhu\n"
              "    data_rate:.........%hhu\n"
              "    Frequency:.........%hu\n"
              "    Channel Flags:.....%hu\n"
              "    Signal:............%hhi\n"
              "    Noise:.............%hhi\n"
              "- BTP:\n"
              "    Recv Error:........%hhu\n"
              "    Game Fin:..........%hhu\n"
              "    Mutex:.............%hhu\n"
              "    Frame Type:........%i\n"
              "    Tree ID:...........%u\n"
              "    TX Power:..........%hhu\n"
              "    Parent Addr:.......%x:%x:%x:%x:%x:%x\n"
              "    Highest Power:.....%hhu\n"
              "    2nd highest power:.%hhu",
              eth.ether_shost[0], eth.ether_shost[1], eth.ether_shost[2], eth.ether_shost[3], eth.ether_shost[4], eth.ether_shost[5],
              eth.ether_dhost[0], eth.ether_dhost[1], eth.ether_dhost[2], eth.ether_dhost[3], eth.ether_dhost[4], eth.ether_dhost[5],
              ntohs(eth.ether_type),
              rdio.it_version,
              rdio.it_len,
              rdio.it_present,
              rdio.tsf_h, rdio.tsf_l,
              rdio.flags,
              rdio.data_rate,
              rdio.chan_freq,
              rdio.chan_flags,
              rdio.dbm_antsignal,
              rdio.dbm_antnoise,
              btp.recv_err,
              btp.game_fin,
              btp.mutex,
              btp.frame_type,
              btp.tree_id,
              btp.tx_pwr,
              btp.parent_addr[0], btp.parent_addr[1], btp.parent_addr[2], btp.parent_addr[3], btp.parent_addr[4], btp.parent_addr[5],
              btp.high_pwr,
              btp.snd_high_pwr
    );
}

void build_frame(eth_btp_t *out, mac_addr_t daddr, uint8_t recv_err, uint8_t mutex,
            frame_t frame_type, uint32_t tree_id, int8_t tx_pwr) {
    out->btp.recv_err = recv_err;
    out->btp.game_fin = self.game_fin;
    out->btp.mutex = mutex;
    out->btp.frame_type = frame_type;
    out->btp.tree_id = tree_id;
    out->btp.tx_pwr = tx_pwr;
    out->btp.high_pwr = self.high_pwr;
    out->btp.snd_high_pwr = self.snd_high_pwr;
    if (self.parent) {
        memcpy(out->btp.parent_addr, self.parent, 6);
    }

    out->eth.ether_type = htons(BTP_ETHERTYPE);
    memcpy(out->eth.ether_dhost, daddr, 6);
    memcpy(out->eth.ether_shost, self.laddr, 6);
}
