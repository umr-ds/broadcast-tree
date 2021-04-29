#ifndef __HELPERS_H__
#define __HELPERS_H__

#include "btp.h"

/**
 * Very simple hash function
 */
uint32_t gen_tree_id(unsigned char *str);

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

#endif // __HELPERS_H__