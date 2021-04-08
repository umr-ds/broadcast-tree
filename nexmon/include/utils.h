#ifndef UTILS_H
#define UTILS_H

#include <structs.h>

int char2int(char input);
char seq_num_is_duplicate(uint32_t seq_num);
int compare(const void *ptr1, const void *ptr2, size_t count);
size_t str_len(const char *str);

#endif // UTILS_H
