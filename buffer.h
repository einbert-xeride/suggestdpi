#ifndef BUFFER_H
#define BUFFER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct Buffer {
    uint8_t *ptr;
    size_t   len;
} Buffer;

void buffer_hexdump(FILE *restrict stream, const Buffer *restrict buffer);
void buffer_free(Buffer *restrict buffer);

#endif // BUFFER_H
