#pragma NEXMON targetregion "patch"

#include <firmware_version.h> // definition of firmware version macros
#include <wrapper.h>          // wrapper definitions for functions that already exist in the firmware
#include <structs.h>          // structures that are used by the code in the firmware
#include <helper.h>           // useful helper functions
#include <patcher.h>          // macros used to craete patches such as BLPatch, BPatch, ...
#include <rates.h>            // rates used to build the ratespec for frame injection

#include <store.h>

void *resize_store(uint32_t *st, size_t original_size, size_t new_size) {
    if (new_size == 0) {
        free(st);
        return 0;
    } else if (!st) {
        return malloc(new_size, 0);
    } else {
        uint32_t *st_new = (uint32_t *) malloc(new_size * sizeof(uint32_t), 0);
        if (st_new) {
            memcpy(st_new, st, original_size);
            free(st);
        }
        return st_new;
    }
}

void init_store(Store *st, size_t initial_size) {
    st->store = (uint32_t *) malloc(initial_size * sizeof(uint32_t), 0);
    memset(st->store, 0, initial_size * sizeof(uint32_t));
    st->used = 0;
    st->size = initial_size;
}

void insert_store(Store *st, uint32_t element) {
    if (st->used == st->size) {
        size_t old_size = st->size * sizeof(uint32_t);
        st->store = (uint32_t *) resize_store(st->store, old_size, old_size * 2);
        st->size *= 2;
    }
    st->store[st->used++] = element;
}

void free_store(Store *st) {
    free(st->store);
    st->store = 0;
    st->used = st->size = 0;
}
