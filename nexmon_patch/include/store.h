#ifndef STORE_H
#define STORE_H

typedef struct
{
    uint32_t *store;
    size_t used;
    size_t size;
} Store;

void *resize_store(uint32_t *st, size_t original_size, size_t new_size);

void init_store(Store *st, size_t initialSize);

void insert_store(Store *st, uint32_t element);

void free_store(Store *st);

#endif /*STORE_H*/
