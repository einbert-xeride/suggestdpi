#include "config.h"

#include <ctype.h>
#include <string.h>
#include <limits.h>

#include "format.h"
#include "log.h"

const uint8_t xdigit_table[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11, 12, 13, 14, 15,
};

static char *lstrip(char *str)
{
    while (*str != '\0' && isspace(*str)) ++str;
    return str;
}

static bool isodigit(int c)
{
    return '0' <= c && c < '8';
}

static char *read_key(char *str)
{
    while (*str != '\0' && (isalnum(*str) || *str == '_' || *str == '-')) ++str;
    return str;
}

typedef struct ReadStat {
    char *next;
    bool ok;
    bool overflow;
} ReadStat;

#define STAT_OK(x) do { stat.next = str + (x); stat.ok = true; stat.overflow = false; return stat; } while (0)
#define STAT_FAIL(x) do { stat.next = str + (x); stat.ok = false; stat.overflow = false; return stat; } while (0)
#define STAT_OVERFLOW(x) do { stat.next = str + (x); stat.ok = true; stat.overflow = true; return stat; } while (0)

static ReadStat read_string(char *restrict str, char *restrict buf, size_t cap)
{
    ReadStat stat = {str, true, false};
    memset(buf, 0, cap);
    char *ptr = buf;
    if (*str != '"') {
        STAT_FAIL(0);
    }
    for (++str ;; ++str) {
        if (*str == '"') {
            STAT_OK(1);
        }
        if (*str == '\0') {
            STAT_FAIL(0);
        }
        if (*str == '\\') {
            ++str;
            switch (*str) {
            case 'a': *ptr++ = '\a'; break;
            case 'b': *ptr++ = '\b'; break;
            case 'e': *ptr++ = '\033'; break;
            case 'f': *ptr++ = '\f'; break;
            case 'n': *ptr++ = '\n'; break;
            case 'r': *ptr++ = '\r'; break;
            case 't': *ptr++ = '\t'; break;
            case 'v': *ptr++ = '\v'; break;
            case '?': *ptr++ = '?'; break;
            case '\\': *ptr++ = '\\'; break;
            case '\'': *ptr++ = '\''; break;
            case '\"': *ptr++ = '\"'; break;
            case 'x':
                if (!isxdigit(str[1])) {
                    STAT_FAIL(1);
                }
                if (!isxdigit(str[2])) {
                    STAT_FAIL(2);
                }
                *ptr++ = (char) ((xdigit_table[(uint8_t) str[1]] << 4u) + xdigit_table[(uint8_t) str[2]]);
                str += 2;
                break;
            default:
                if (!('0' <= *str && *str < '4')) {
                    STAT_FAIL(0);
                }
                if (str[0] == '0' && !isodigit(str[1])) {
                    *ptr++ = '\0';
                    break;
                }
                if (!isodigit(str[1])) {
                    STAT_FAIL(1);
                }
                if (!isodigit(str[2])) {
                    STAT_FAIL(2);
                }
                *ptr++ = (char) ((xdigit_table[(uint8_t) str[0]] << 6u) + (xdigit_table[(uint8_t) str[1]] << 3u) + xdigit_table[(uint8_t) str[2]]);
                str += 2;
                break;
            }
        } else {
            *ptr++ = *str;
        }
        if (ptr == buf + cap) {
            ptr[-1] = '\0';
            STAT_OVERFLOW(0);
        }
    }
}

static ReadStat read_unsigned(char *str, uint16_t *ptr)
{
    uint64_t val = 0;
    ReadStat stat = {str, true, false};
    if (!isdigit(*str)) {
        STAT_FAIL(0);
    }

    bool is_hex = str[0] == '0' && (str[1] == 'x' || str[1] == 'X');
    bool is_bin = str[0] == '0' && (str[1] == 'b' || str[1] == 'B');
    bool is_oct_o = str[0] == '0' && (str[1] == 'o' || str[1] == 'O');
    bool is_oct_c = str[0] == '0' && isdigit(str[1]);

    if (is_hex || is_oct_o || is_bin) {
        str += 2;
    } else if (is_oct_c) {
        str += 1;
    }

    if (!isdigit(*str)) {
        STAT_FAIL(0);
    }

    bool is_oct = is_oct_o || is_oct_c;

    for (; *str != '\0'; ++str) {
        if (isspace(*str)) {
            *ptr = (uint16_t) val;
            STAT_OK(0);
        }
        if (is_hex && isxdigit(*str)) {
            val = val * 16 + xdigit_table[(uint8_t) *str];
        } else if (is_oct && isodigit(*str)) {
            val = val * 8 + xdigit_table[(uint8_t) *str];
        } else if (is_bin && (*str == '0' || *str == '1')) {
            val = val * 2 + xdigit_table[(uint8_t) *str];
        } else if (isdigit(*str)) {
            val = val * 10 + xdigit_table[(uint8_t) *str];
        } else {
            STAT_FAIL(0);
        }
        if (val > UINT16_MAX) {
            *ptr = (uint16_t) val;
            STAT_OVERFLOW(0);
        }
    }
    *ptr = (uint16_t) val;
    STAT_OK(0);
}

bool read_config_row(FILE *restrict stream, ConfigRow *restrict config_row)
{
    int *pline = &config_row->line;
    char buf[1024];
    char *line = buf;
    memset(buf, 0, sizeof(buf));
    while (fgets(buf, sizeof(buf), stream) != NULL) {
        ++*pline;
        line = lstrip(buf);
        if (*line != '\0') {
            break;
        }
    }
    if (feof(stream)) return false;
    if (ferror(stream)) {
        LOG(ERROR, "config: line %d: read error", *pline);
        return false;
    }

    size_t length = strlen(line);
    if (line[length - 1] != '\n') {
        LOG(ERROR, "config: line %d: line is too long", *pline);
        return false;
    }

    config_row->has_pnp = false;
    config_row->has_product = false;
    config_row->has_name = false;
    config_row->has_serial = false;
    config_row->has_dpi = false;

    for (;;) {
        char *key = lstrip(line);
        char *key_end = read_key(key);
        if (key == key_end) {
            if (*key_end == '\0' || *key_end == '#') {
                // end of line or comment
                break;
            } else {
                LOG(ERROR, "config: line %d: unexpected char '%s'", *pline, fmt_escape_char(*key));
                return false;
            }
        }
        char *equ_begin = lstrip(key_end);
        if (*equ_begin != '=') {
            LOG(ERROR, "config: line %d: expected '=', got '%s'", *pline, fmt_escape_char(*key));
            return false;
        }
        *key_end = '\0';

        char *value_begin = lstrip(equ_begin + 1);
        ReadStat read_stat;

        if (strcmp(key, "pnp") == 0) {
            read_stat = read_string(value_begin, config_row->pnp, sizeof(config_row->pnp));
            config_row->has_pnp = read_stat.ok;
        } else if (strcmp(key, "product") == 0) {
            read_stat = read_unsigned(value_begin, &config_row->product);
            config_row->has_product = read_stat.ok;
        } else if (strcmp(key, "name") == 0) {
            read_stat = read_string(value_begin, config_row->name, sizeof(config_row->name));
            config_row->has_name = read_stat.ok;
        } else if (strcmp(key, "serial") == 0) {
            read_stat = read_string(value_begin, config_row->serial, sizeof(config_row->serial));
            config_row->has_serial = read_stat.ok;
        } else if (strcmp(key, "dpi") == 0) {
            read_stat = read_unsigned(value_begin, &config_row->dpi);
            config_row->has_dpi = read_stat.ok;
        } else {
            LOGB(ERROR, out) {
                fprintf(out, "config: line %d: unknown key ", *pline);
                fmt_quote_string(out, key);
            }
            return false;
        }

        if (!read_stat.ok) {
            LOG(ERROR, "config: line %d col %ld: unexpected char '%s'", *pline, read_stat.next - buf + 1, fmt_escape_char(*read_stat.next));
            return false;
        }
        if (read_stat.overflow) {
            LOGB(ERROR, out) {
                fprintf(out, "config: line %d col %ld: value of ", *pline, read_stat.next - buf + 1);
                fmt_quote_string(out, key);
                fputs(" is too long", out);
            }
            return false;
        }

        line = read_stat.next;
    }

    LOGB(DEBUG, out) {
        fprintf(out, "config: line %d:", *pline);
        if (config_row->has_pnp) {
            fputs(" pnp=", out);
            fmt_quote_string(out, config_row->pnp);
        }
        if (config_row->has_product) {
            fprintf(out, " product=%04x", config_row->product);
        }
        if (config_row->has_name) {
            fputs(" name=", out);
            fmt_quote_string(out, config_row->name);
        }
        if (config_row->has_serial) {
            fputs(" serial=", out);
            fmt_quote_string(out, config_row->serial);
        }
        if (config_row->has_dpi) {
            fprintf(out, " dpi=%u", config_row->dpi);
        }
        fputs(" # eol", out);
    }

    return true;
}
