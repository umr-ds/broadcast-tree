#pragma NEXMON targetregion "patch"

#include "btp.h"
#include "utils.h"

extern MAC my_mac;
extern Store seq_nums;

int check_source_node(MAC *source) {
    if (compare(source->addr, my_mac.addr, 6) == 0) {
        printf("Not considering own frames.\n\n");
        return 1;
    }

    return 0;
}

uint8_t calculate_min_tx_pwr(uint8_t sender_tx_pwr, uint8_t measured_rssi) {
    uint8_t calculated_snr = measured_rssi / RSSI_SNR_FACTOR;

    if (calculated_snr >= sender_tx_pwr) {
        return sender_tx_pwr;
    }

    return sender_tx_pwr - (calculated_snr + 1);
}

int addr_is_in_route(MAC *addr, MAC *addr_list, uint16_t route_len) {
    int i;
    for (i = route_len; i < route_len; i++) {
        if (compare(addr, addr_list[i].addr, 6) == 0) {
            return 0;
        }
    }

    return 1;
}

void build_frame(BTP *frame, uint16_t src_flg, uint16_t ttl, uint16_t type, uint16_t fin_flg, uint16_t pwr_flg, uint16_t route_len, Power_data *pwr_data, MAC **route_data, char **payload) {
    // We use the time since the device is up for the initial sequence number.
    // After that, the number is increased or decreased appropriately.
    uint32_t t = (uint32_t) hndrte_time_ms();

    if (seq_nums.store) {
        insert_store(&seq_nums, t);
    } else {
        printf("Fatal! The sequence store is NULL. Undefined behaviour might occur!\n\n");
    }

    frame->seq_num = t;
    frame->src_flg = src_flg;
    frame->ttl = ttl;
    frame->type = type;
    frame->fin_flg = fin_flg;
    frame->pwr_flg = pwr_flg;
    frame->route_len = route_len;
    frame->pwr_data = *pwr_data;
    frame->route_data = 0; //*route_data;
    frame->data = 0; //*payload;

    printf("Copying frame: %u      \n", sizeof(*frame));
}

void send_packet(struct wlc_info *wlc, BTP *frame, MAC *sa, MAC *da, size_t len) {
    // TODO: Set maximum transmission power.
    // TODO: Currently, the power data is set to some fixed values. If we can calculate them
    // (see above TODO), we should set them accordingly.

    Wifi_frame head = {
        .fc = {0x08, 0x00},
        .did = {0x00, 0x00},
        .sa = *sa, 
        .da = *da,
        .bssid = {.addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
        .seqctl = {0x00, 0x00},
        .llc = {.dsap = {0xaa}, .ssap = {0xaa}, .ctrl = {0x03}, .oui = {0x00, 0x00, 0x00}, .ethertype = {0x01, 0x01}},
        .payload_len = len + BTP_HEADER_SIZE,
        .payload = {0}
    };

    memcpy(head.payload, frame, head.payload_len);

    int tmp_len = sizeof(head) - MAX_PAYLOAD_SIZE + head.payload_len;
    sk_buff *p = pkt_buf_get_skb(wlc->osh, tmp_len + 202);
    Wifi_frame *frame_skb = (Wifi_frame *)skb_pull(p, 202);
    memcpy(frame_skb, &head, tmp_len);

    sendframe(wlc, p, 1, 0);
}
