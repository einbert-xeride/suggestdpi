#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "format.h"
#include "screen_info.h"

#ifndef DEFAULT_CONFIG_PATH
# define DEFAULT_CONFIG_PATH "/etc/suggestdpi.conf"
#endif

struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"verbose", no_argument, NULL, 'v'},
    {"config", optional_argument, NULL, 'c'},
    {0, 0, 0, 0},
};

void print_usage(const char *exe)
{
    static const char *usage =
        "usage: %s [-hv] [-c CONFIG]\n"
        "\n"
        "options:\n"
        "    -h, --help\n"
        "           show this help\n"
        "    -v, --verbose\n"
        "           increase verbosity\n"
        "    -c, --config=CONFIG\n"
        "           load dpi config from CONFIG instead of " DEFAULT_CONFIG_PATH "\n";
    fprintf(stderr, usage, exe);
}

int main(int argc, char *argv[])
{
    int option_idx = 0, option_chr;
    const char *config_path = DEFAULT_CONFIG_PATH;
    while ((option_chr = getopt_long(argc, argv, "hvc:", long_options, &option_idx)) != -1) {
        switch (option_chr) {
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 'v':
            log_set_level(log_get_level() - 1);
            break;
        case 'c':
            config_path = optarg;
            break;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    ScreenInfo primary_screen_info;
    if (!screen_info_primary(&primary_screen_info)) {
        return EXIT_FAILURE;
    }

    uint16_t dpi = 0;

    FILE *config_file = fopen(config_path, "r");
    if (config_file == NULL) {
        LOGB(ERROR, out) {
            fputs("failed to open config file ", out);
            fmt_quote_string(out, config_path);
        }
    } else {
        ConfigRow row;
        const EdidInfo *edid = &primary_screen_info.edid_info;
        while (read_config_row(config_file, &row)) {
            if (row.has_pnp && strcmp(row.pnp, (const char *) edid->pnp_id) != 0) continue;
            if (row.has_product && row.product != edid->product_id) continue;
            if (row.has_name && strcmp(row.name, edid->product_name) != 0) continue;
            if (row.has_serial && strcmp(row.serial, edid->serial_number) != 0) continue;
            if (row.has_dpi) dpi = row.dpi;
            LOG(DEBUG, "matched line %d, dpi=%u", row.line, dpi);
        }
        fclose(config_file);
    }

    if (dpi != 0) {
        printf("%u\n", dpi);
        return EXIT_SUCCESS;
    }

    unsigned physical_width = primary_screen_info.edid_info.physical_width;
    unsigned physical_height = primary_screen_info.edid_info.physical_height;
    if (physical_width == 0 || physical_height == 0) {
        LOG(INFO, "real monitor size is unknown");
        return EXIT_FAILURE;
    }

    unsigned screen_width = primary_screen_info.geometry.width;
    unsigned screen_height = primary_screen_info.geometry.height;
    if (screen_width == 0 || screen_height == 0) {
        LOG(ERROR, "failed to get primary screen size");
        return EXIT_FAILURE;
    }

    static const double INCH_PER_CM = 0.3937008;
    double fdpi = sqrt((pow(screen_width, 2) + pow(screen_height, 2))
                           / (pow(physical_width * INCH_PER_CM, 2) + pow(physical_height * INCH_PER_CM, 2)));
    LOG(DEBUG, "raw dpi: %g", fdpi);

    dpi = (uint16_t) ((fdpi + 12) / 24) * 24;
    printf("%u\n", dpi);

    return EXIT_SUCCESS;
}
