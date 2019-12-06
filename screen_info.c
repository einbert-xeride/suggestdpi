#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>

#include "buffer.h"
#include "log.h"
#include "format.h"
#include "screen_info.h"

static const char *ATOM_NAMES[] = {
    "EDID\0",
    "EDID_DATA\0",
    "XFree86_DDC_EDID1_RAWDATA\0",
};

typedef enum Atom {
    EDID,
    EDID_DATA,
    XFree86_DDC_EDID1_RAWDATA,
    NumAtom,
} Atom;

#define SYNC_XCB_CALL(conn, func, ...) func##_reply(conn, func(conn, __VA_ARGS__), NULL)

static bool query_xcb_randr(xcb_connection_t *conn, uint32_t major_version, uint32_t minor_version)
{
    const xcb_query_extension_reply_t *ext_reply = xcb_get_extension_data(conn, &xcb_randr_id);
    if (!ext_reply || !ext_reply->present) {
        return false;
    }

    xcb_randr_query_version_reply_t *reply = SYNC_XCB_CALL(conn, xcb_randr_query_version, major_version, minor_version);
    if (!reply) {
        return false;
    }
    bool ok = (reply->major_version == 1 && reply->minor_version >= 2);
    free(reply);
    return ok;
}

static void init_xcb_atoms(xcb_connection_t *conn, const char *atom_names[], xcb_atom_t atoms[], size_t size)
{
    memset(atoms, 0, sizeof(xcb_atom_t) * size);

    size_t cookie_size = sizeof(xcb_intern_atom_cookie_t) * size;
    xcb_intern_atom_cookie_t *cookies = malloc(cookie_size);
    if (cookies == NULL) return;
    memset(cookies, 0, cookie_size);

    for (size_t i = 0; i < size; ++i) {
        cookies[i] = xcb_intern_atom(conn, 0, strlen(atom_names[i]), atom_names[i]);
    }
    for (size_t i = 0; i < size; ++i) {
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookies[i], NULL);
        if (reply) {
            atoms[i] = reply->atom;
            free(reply);
        } else {
            atoms[i] = 0;
        }
    }

    free(cookies);
}

static xcb_window_t make_dummy_window(xcb_connection_t *conn)
{
    xcb_screen_t *first_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_window_t window = xcb_generate_id(conn);
    xcb_create_window(conn, 0, window, first_screen->root, 0, 0, 1, 1, 0, 0, 0, 0, NULL);
    return window;
}

static const xcb_randr_output_t NO_RANDR_OUTPUT = ~(xcb_randr_output_t)0;

static xcb_randr_output_t get_output_primary(xcb_connection_t *conn, xcb_window_t window)
{
    xcb_randr_get_output_primary_reply_t *reply = SYNC_XCB_CALL(conn, xcb_randr_get_output_primary, window);
    if (!reply) {
        return NO_RANDR_OUTPUT;
    }
    xcb_randr_output_t output = reply->output;
    free(reply);
    return output;
}

static xcb_timestamp_t get_window_timestamp(xcb_connection_t *conn, xcb_window_t window)
{
    xcb_randr_get_screen_resources_current_reply_t
        *reply = SYNC_XCB_CALL(conn, xcb_randr_get_screen_resources_current, window);
    if (!reply) {
        return 0;
    }
    xcb_timestamp_t ret = reply->timestamp;
    free(reply);
    return ret;
}

static OutputGeometry get_output_geometry(xcb_connection_t *conn, xcb_randr_output_t output, xcb_timestamp_t timestamp)
{
    OutputGeometry geo = {0, 0, 0, 0, 0};
    xcb_randr_get_output_info_reply_t *output_reply = SYNC_XCB_CALL(conn, xcb_randr_get_output_info, output, timestamp);
    if (!output_reply) {
        return geo;
    }
    xcb_randr_crtc_t crtc = output_reply->crtc;
    free(output_reply);
    xcb_randr_get_crtc_info_reply_t *reply = SYNC_XCB_CALL(conn, xcb_randr_get_crtc_info, crtc, timestamp);
    if (!reply) {
        return geo;
    }
    geo.x = reply->x;
    geo.y = reply->y;
    geo.width = reply->width;
    geo.height = reply->height;
    geo.rotation = reply->rotation;
    free(reply);
    return geo;
}

static Buffer get_output_property(xcb_connection_t *conn, xcb_randr_output_t output, xcb_atom_t atom)
{
    Buffer buffer = {NULL, 0};
    xcb_randr_get_output_property_reply_t
        *reply = SYNC_XCB_CALL(conn, xcb_randr_get_output_property, output, atom, XCB_ATOM_ANY, 0, 100, false, false);
    if (!reply) {
        LOG(DEBUG, "xcb randr get output property %d failed", atom);
        return buffer;
    }
    buffer.len = reply->num_items;
    if (buffer.len > 0) {
        buffer.ptr = malloc(buffer.len);
        memcpy(buffer.ptr, xcb_randr_get_output_property_data(reply), buffer.len);
    }
    free(reply);
    return buffer;
}

static const char *get_xcb_rotation_name(uint16_t rotation)
{
    switch (rotation) {
    case XCB_RANDR_ROTATION_ROTATE_0:
        return "normal";
    case XCB_RANDR_ROTATION_ROTATE_90:
        return "left";
    case XCB_RANDR_ROTATION_ROTATE_180:
        return "inverted";
    case XCB_RANDR_ROTATION_ROTATE_270:
        return "right";
    case XCB_RANDR_ROTATION_REFLECT_X:
        return "reflect_x";
    case XCB_RANDR_ROTATION_REFLECT_Y:
        return "reflect_y";
    default:
        return "unknown";
    }
}

static void copy_edid_string(char *dst, const uint8_t *ptr) {
    memset(dst, 0, 16);
    memcpy(dst, ptr, 13);
    for (char *ch = dst; *ch != '\0'; ++ch) {
        if (*ch == '\r' || *ch == '\n') {
            *ch = '\0';
        }
    }
    char *begin = dst;
    while (*begin != '\0' && isspace(*begin)) ++begin;
    char *end = begin + strlen(begin) - 1;
    while (end >= begin && isspace(*end)) --end;
    ++end;
    for (char *s = begin, *d = dst; s < end; ++s, ++d) {
        *d = *s;
    }
    *(dst + (end - begin)) = '\0';
}

static bool parse_edid(Buffer buff, EdidInfo *edid)
{
    static const int EDID_PNP_ID_LO = 8;
    static const int EDID_PNP_ID_HI = 9;
    static const int EDID_PRODUCT = 10;
    static const int EDID_SERIAL = 12;
    static const int EDID_PHYSICAL_WIDTH = 21;
    static const int EDID_PHYSICAL_HEIGHT = 22;
    static const int EDID_DATA_BLOCKS = 54;
    static const uint8_t EDID_HEADER[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

    memset(edid, 0, sizeof(EdidInfo));

    if (buff.len < 128) {
        LOG(DEBUG, "edid length %lu insufficient", buff.len);
        return false;
    }
    if (memcmp(EDID_HEADER, buff.ptr, sizeof(EDID_HEADER)) != 0) {
        LOG(DEBUG, "edid header mismatch");
        return false;
    }

    // PNP ID
    edid->pnp_id[0] = (char) ('A' + ((buff.ptr[EDID_PNP_ID_LO] & 0x7cu) >> 2u) - 1);
    edid->pnp_id[1] = (char) ('A' + ((buff.ptr[EDID_PNP_ID_LO] & 0x03u) << 3u) + ((buff.ptr[EDID_PNP_ID_HI] & 0xe0u) >> 5u) - 1);
    edid->pnp_id[2] = (char) ('A' + (buff.ptr[EDID_PNP_ID_HI] & 0x1fu) - 1);
    edid->pnp_id[3] = '\0';

    // PRODUCT ID
    edid->product_id = ((uint16_t) buff.ptr[EDID_PRODUCT])
        + ((uint16_t) buff.ptr[EDID_PRODUCT + 1] << 8u);

    // SERIAL
    edid->serial_num = ((uint32_t) buff.ptr[EDID_SERIAL])
        + ((uint32_t) buff.ptr[EDID_SERIAL + 1] << 8u)
        + ((uint32_t) buff.ptr[EDID_SERIAL + 2] << 16u)
        + ((uint32_t) buff.ptr[EDID_SERIAL + 3] << 24u);

    // SCREEN SIZE
    edid->physical_width = buff.ptr[EDID_PHYSICAL_WIDTH];
    edid->physical_height = buff.ptr[EDID_PHYSICAL_HEIGHT];

    for (int i = 0; i < 5; ++i) {
        int offset = EDID_DATA_BLOCKS + i * 18;

        if (buff.ptr[offset] != 0 || buff.ptr[offset + 1] != 0 || buff.ptr[offset + 2] != 0) {
            continue;
        }

        switch (buff.ptr[offset + 3]) {
        case 0xfc: // EDID_DESC_PRODUCT_NAME
            copy_edid_string(edid->product_name, buff.ptr + offset + 5);
            break;
        case 0xfe: // EDID_DESC_ALPHANUMERIC_STRING
            copy_edid_string(edid->identifier, buff.ptr + offset + 5);
            break;
        case 0xff: // EDID_DESC_SERIAL_NUMBER
            copy_edid_string(edid->serial_number, buff.ptr + offset + 5);
            break;
        }
    }

    return true;
}

bool screen_info_primary(ScreenInfo *restrict info)
{
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    xcb_atom_t atoms[NumAtom];

    if (!query_xcb_randr(conn, 1, 6)) {
        LOG(ERROR, "failed to intialize xrandr");
        xcb_disconnect(conn);
        return false;
    }

    init_xcb_atoms(conn, ATOM_NAMES, atoms, NumAtom);
    LOG(DEBUG, "xcb atoms: [%s:%d, %s:%d, %s:%d]", ATOM_NAMES[0], atoms[0], ATOM_NAMES[1], atoms[1], ATOM_NAMES[2], atoms[2]);

    xcb_window_t window = make_dummy_window(conn);
    LOG(DEBUG, "xcb window: 0x%08x", window);
    xcb_randr_output_t primary = get_output_primary(conn, window);
    LOG(DEBUG, "xcb primary output: 0x%08x", primary);

    xcb_timestamp_t config_timestamp = get_window_timestamp(conn, window);
    LOG(DEBUG, "xcb window config timestamp: %u", config_timestamp);
    info->geometry = get_output_geometry(conn, primary, config_timestamp);
    LOG(DEBUG, "xcb primary geometry: [x:%d, y:%d, w:%u, h:%u, r:%s]",
        info->geometry.x, info->geometry.y,
        info->geometry.width, info->geometry.height,
        get_xcb_rotation_name(info->geometry.rotation));

    Buffer edid_buf;
    edid_buf = get_output_property(conn, primary, atoms[EDID]);
    if (edid_buf.len == 0) {
        edid_buf = get_output_property(conn, primary, atoms[EDID_DATA]);
    }
    if (edid_buf.len == 0) {
        edid_buf = get_output_property(conn, primary, atoms[XFree86_DDC_EDID1_RAWDATA]);
    }
    if (edid_buf.len == 0) {
        LOG(ERROR, "failed to get edid data");
        xcb_disconnect(conn);
        return false;
    }

    if (!parse_edid(edid_buf, &info->edid_info)) {
        LOG(ERROR, "failed to parse edid data");
        xcb_disconnect(conn);
        return false;
    }
    LOG(  DEBUG, "xcb randr edid data:");
    LOGBM(DEBUG, out, "  - pnp_id: ") fmt_quote_string(out, info->edid_info.pnp_id);
    LOG(  DEBUG, "  - product_id: 0x%04" PRIx16, info->edid_info.product_id);
    LOG(  DEBUG, "  - serial_num: 0x%08" PRIx32, info->edid_info.serial_num);
    LOGBM(DEBUG, out, "  - product_name: ") fmt_quote_string(out, info->edid_info.product_name);
    LOGBM(DEBUG, out, "  - identifier: ") fmt_quote_string(out, info->edid_info.identifier);
    LOGBM(DEBUG, out, "  - serial_number: ") fmt_quote_string(out, info->edid_info.serial_number);
    LOG(  DEBUG, "  - physical_width: %" PRIu8, info->edid_info.physical_width);
    LOG(  DEBUG, "  - physical_height: %" PRIu8, info->edid_info.physical_height);
    LOG(  DEBUG, "config template:");
    LOGBM(DEBUG, out, "  ") {
        fputs("pnp=", out);
        fmt_quote_string(out, info->edid_info.pnp_id);
        fprintf(out, " product=0x%04" PRIx16, info->edid_info.product_id);
        fputs(" name=", out);
        fmt_quote_string(out, info->edid_info.product_name);
        fputs(" serial=", out);
        fmt_quote_string(out, info->edid_info.serial_number);
        fputs(" dpi=96 # change it to your desirable value", out);
    };

    buffer_free(&edid_buf);
    xcb_disconnect(conn);
    return true;
}
