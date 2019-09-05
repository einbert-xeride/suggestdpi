#ifndef SCREEN_INFO_H
#define SCREEN_INFO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct OutputGeometry {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t rotation;
} OutputGeometry;

typedef struct EdidInfo {
    char pnp_id[4];
    uint16_t product_id;
    uint32_t serial_num;
    char product_name[16];
    char identifier[16];
    char serial_number[16];
    uint8_t physical_width;
    uint8_t physical_height;
} EdidInfo;

typedef struct ScreenInfo {
    OutputGeometry geometry;
    EdidInfo edid_info;
} ScreenInfo;

bool screen_info_primary(ScreenInfo *restrict info);

#endif // SCREEN_INFO_H
