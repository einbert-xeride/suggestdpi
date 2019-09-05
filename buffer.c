#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void buffer_hexdump(FILE *restrict stream, const Buffer *restrict buffer)
{
    static const char *hexdigits = "0123456789abcdef";
    const uint8_t *end = buffer->ptr + buffer->len;
    fputc('[', stream);
    for (const uint8_t *begin = buffer->ptr; begin != end; ++begin) {
        char hi = hexdigits[(*begin >> 4u) & 0xFu];
        char lo = hexdigits[*begin & 0xFu];
        fputc(hi, stream);
        fputc(lo, stream);
        if (begin + 1 != end) {
            fputc(' ', stream);
        }
    }
    fputc(']', stream);
}

void buffer_free(Buffer *restrict buffer)
{
    if (buffer == NULL) return;
    if (buffer->ptr == NULL) return;
    free(buffer->ptr);
    buffer->ptr = NULL;
    buffer->len = 0;
}
