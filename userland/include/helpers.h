#ifndef __HELPERS_H__
#define __HELPERS_H__

#include "btp.h"

uint32_t gen_tree_id(unsigned char *str);
void hexdump(const void* data, size_t size);
void print_mac(uint8_t * addr);
void pprint_frame(btp_frame_t *in_frame);

#endif // __HELPERS_H__