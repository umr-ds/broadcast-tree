#include <time.h>
#include <tree.h>

#include "helpers.h"

extern self_t self;

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

void hexdump(const void *data, size_t size) {
    char chars[17];
    chars[16] = '\0';

    size_t data_index;
    size_t padding_index;

    for (data_index = 0; data_index < size; ++data_index) {
        printf("%02x ", ((unsigned char *) data)[data_index]);

        if (((unsigned char *) data)[data_index] >= ' ' && ((unsigned char *) data)[data_index] <= '~') {
            chars[data_index % 16] = ((unsigned char *) data)[data_index];
        } else {
            chars[data_index % 16] = '.';
        }

        if ((data_index + 1) % 8 == 0 || data_index + 1 == size) {
            printf(" ");

            if ((data_index + 1) % 16 == 0) {
                printf("|  %s \n", chars);
            } else if (data_index + 1 == size) {
                chars[(data_index + 1) % 16] = '\0';

                if ((data_index + 1) % 16 <= 8) {
                    printf(" ");
                }

                for (padding_index = (data_index + 1) % 16; padding_index < 16; ++padding_index) {
                    printf("   ");
                }

                printf("|  %s \n", chars);
            }
        }
    }
}

void print_mac(uint8_t *addr) {
    printf("%x:%x:%x:%x:%x:%x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

void pprint_frame(btp_frame_t *in_frame) {
    // ETHERNET
    struct ether_header eth = in_frame->eth;
    printf("BTP Frame:\n");
    printf("->Ethernet:\n");
    printf("--->Source:............");
    print_mac(eth.ether_shost);
    printf("--->Destination:.......");
    print_mac(eth.ether_dhost);
    printf("--->EtherType:.........%hu\n", ntohs(eth.ether_type));

    // RADIOTAP
    radiotap_t rdio = in_frame->radiotap;
    printf("->RadioTap Header:\n");
    printf("--->Version:...........%hhu\n", rdio.it_version);
    printf("--->Length:............%hu\n", rdio.it_len);
    printf("--->Present:...........%u\n", rdio.it_present);

    printf("->RadioTap Header:\n");
    printf("--->Time Sync:.........%u%u\n", rdio.tsf_h, rdio.tsf_l);
    printf("--->Flags:.............%hhu\n", rdio.flags);
    printf("--->data_rate:.........%hhu\n", rdio.data_rate);
    printf("--->Frequency:.........%hu\n", rdio.chan_freq);
    printf("--->Channel Flags:.....%hu\n", rdio.chan_flags);
    printf("--->Signal:............%hhi\n", rdio.dbm_antsignal);
    printf("--->Noise:.............%hhi\n", rdio.dbm_antnoise);

    // BTP
    btp_header_t btp = in_frame->btp;
    printf("->BTP:\n");
    printf("--->Recv Error:........%hhu\n", btp.recv_err);
    printf("--->Game Fin:..........%hhu\n", btp.game_fin);
    printf("--->Mutex:.............%hhu\n", btp.mutex);
    printf("--->Frame Type:........%i\n", btp.frame_type);
    printf("--->Tree ID:...........%u\n", btp.tree_id);
    printf("--->TX Power:..........%hhu\n", btp.tx_pwr);
    printf("--->Parent Addr:.......");
    print_mac(btp.parent_addr);
    printf("--->Highest Power:.....%hhu\n", btp.high_pwr);
    printf("--->2nd highest power:.%hhu\n", btp.snd_high_pwr);

    fflush(stdout);
}

void build_frame(btp_frame_t *out, mac_addr_t daddr, uint8_t recv_err, uint8_t game_fin, uint8_t mutex,
            frame_t frame_type, uint32_t tree_id, int8_t tx_pwr) {
    out->btp.recv_err = recv_err;
    out->btp.game_fin = game_fin;
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
