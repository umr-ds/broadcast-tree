#ifndef __HELPERS_H__
#define __HELPERS_H__

#include "btp.h"

/**
 * Generate a (hopefully unique) id for a new braodcast tree.
 * This is done running the source node's mac address together with the current timestamp through a very simple hashing function.
 * 
 * @param laddr: MAC address of the source node of the new tree
 */
uint32_t gen_tree_id(mac_addr_t laddr);

/**
 * 
 */
void hexdump(const void* data, size_t size);

/**
 * Prints MAC address, since doing it by hand is very bothersome...
 */
void print_mac(uint8_t * addr);

/**
 * Pretty-print a BTP-frame
 */
void pprint_frame(btp_frame_t *in_frame);

bool already_child(mac_addr_t potential_child_addr);

void build_frame(btp_frame_t *out, mac_addr_t daddr, uint8_t recv_err, uint8_t game_fin, uint8_t mutex,
                 frame_t frame_type, uint32_t tree_id,
                 uint16_t seq_num, uint32_t tx_pwr);

#endif // __HELPERS_H__
