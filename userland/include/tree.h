#ifndef __TREE_H__
#define __TREE_H__

#include <stdbool.h>

#include "btp.h"

#define BREADTH 10

/**
 * State of our parent
 */
typedef struct {
    mac_addr_t addr;
    uint32_t high_pwr; // the power at which the parent does currently broadcast data frames
    uint32_t snd_high_pwr; // the power at which the parent WOULD broadacst data frames, if its furthest child were to disconnect
    uint32_t own_pwr; // the minimum power with which the parent has to broadcast to reach us
} parent_t;

/**
 * State of one of our children
 */
typedef struct {
    mac_addr_t addr;
    uint32_t tx_pwr; // minimum power with which we have to broadcast to reach this child
} child_t;

/**
 * Our own state
 */
typedef struct {
    child_t children[BREADTH]; // list of currently connected children
    uint32_t max_pwr; // maximum power at which we are able (or willing) to broadcast
    uint32_t high_pwr; // the power at which we currently broadcast data frames
    uint32_t snd_high_pwr; // // the power at which we WOULD broadacst data frames, if our furthest child were to disconnect
} self_t;

/**
 * Track state of a currently pending connection
 */
typedef struct {
    bool pending; // are we currently waiting for a response to a connection request?
    mac_addr_t parent; // the parent to which we are attempting to connect
    uint32_t tx_pwr; // power at which the parent has to broadcast to reach us (used later to update the parent's high_pwr)
} pending_connection_t;

#endif // __TREE_H__
