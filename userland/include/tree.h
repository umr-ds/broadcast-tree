#ifndef __TREE_H__
#define __TREE_H__

#include <stdbool.h>

#include "btp.h"

#define BREADTH 10

typedef struct {
    mac_addr_t addr;
    uint32_t high_pwr;
    uint32_t snd_high_pwr;
    uint32_t own_pwr;
} parent_t;

typedef struct {
    mac_addr_t addr;
    uint32_t tx_pwr;
} child_t;

typedef struct {
    child_t children[BREADTH];
    uint32_t max_pwr;
    uint32_t high_pwr;
    uint32_t snd_high_pwr;
} self_t;

typedef struct {
    bool pending;
    mac_addr_t parent;
    uint32_t tx_pwr;
} pending_connection_t;

#endif // __TREE_H__