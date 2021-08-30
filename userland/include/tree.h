#ifndef __TREE_H__
#define __TREE_H__

#include "btp.h"
#include "hashmap.h"

#define BREADTH 10

/**
 * State of our parent
 */
typedef struct {
    mac_addr_t addr;
    int8_t high_pwr; // the power at which the parent does currently broadcast data frames
    int8_t snd_high_pwr; // the power at which the parent WOULD broadacst data frames, if its furthest child were to disconnect
    int8_t own_pwr; // the minimum power with which the parent has to broadcast to reach us
} parent_t;

/**
 * State of one of our children
 */
typedef struct {
    mac_addr_t addr;
    int8_t tx_pwr; // minimum power with which we have to broadcast to reach this child
} child_t;

/**
 * Our own state
 */
typedef struct {
    bool is_source; // whether we are the rood of the broadcast tree
    map_t children; // hashmap of currently connected children
    int8_t max_pwr; // maximum power at which we are able (or willing) to broadcast
    int8_t high_pwr; // the power at which we currently broadcast data frames
    int8_t snd_high_pwr; // the power at which we WOULD broadacst data frames, if our furthest child were to disconnect
    mac_addr_t laddr; // local mac address
    parent_t *parent; // currently connected parent
    parent_t *pending_parent;
    uint32_t tree_id; // the tree to which we belong
    bool game_fin;
    char if_name[IFNAMSIZ]; // the interface name to be used
    int sockfd;
} self_t;

#endif // __TREE_H__
