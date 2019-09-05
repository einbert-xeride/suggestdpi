#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct ConfigRow {
    int      line;
    char     pnp[4];
    bool     has_pnp;
    uint16_t product;
    bool     has_product;
    char     name[16];
    bool     has_name;
    char     serial[16];
    bool     has_serial;
    uint16_t dpi;
    bool     has_dpi;
} ConfigRow;

bool read_config_row(FILE *restrict stream, ConfigRow *restrict config_row);

#endif // CONFIG_H
