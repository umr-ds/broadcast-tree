#pragma NEXMON targetregion "patch"

#include <structs.h>            // structures that are used by the code in the firmware

#include "store.h"
#include "utils.h"

extern Store seq_nums;

int char2int(char input) {
    if (input >= '0' && input <= '9')
        return input - '0';
    if (input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    return -1;
}

char seq_num_is_duplicate(uint32_t seq_num) {
    for (int i = 0; i < seq_nums.used; i++) {
        if (seq_num == seq_nums.store[i]) {
            return 1;
        }
    }

    return 0;
}

int compare(const void *ptr1, const void *ptr2, size_t count) {
    const char *s1 = ptr1;
    const char *s2 = ptr2;

    while (count-- > 0) {
        if (*s1++ != *s2++) {
            return s1[-1] < s2[-1] ? -1 : 1;
        }
    }
    return 0;
}

size_t str_len(const char *str) {
    return (*str) ? str_len(++str) + 1 : 0;
}
