#include "log.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static LogLevel log_level = LOG_LEVEL_INFO;
static FILE *log_output = NULL;

void log_set_level(LogLevel level)
{
    log_level = level;
}

LogLevel log_get_level()
{
    return log_level;
}

void log_set_output(FILE *restrict output)
{
    log_output = output;
}

FILE *log_get_output()
{
    return log_output ? log_output : stderr;
}

void log_print(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    FILE *stream = log_print_begin(level, file, line);
    if (stream == NULL) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(stream, fmt, args);
    va_end(args);

    fputc('\n', stream);
}

FILE *log_print_begin(LogLevel level, const char *file, int line)
{
    FILE *stream = log_output ? log_output : stderr;
    if (level < log_level) return NULL;

    switch (level) {
    case LOG_LEVEL_TRACE: fprintf(stream, "trace: "); break;
    case LOG_LEVEL_DEBUG: fprintf(stream, "debug: "); break;
    case LOG_LEVEL_INFO: fprintf(stream, "info: "); break;
    case LOG_LEVEL_WARN: fprintf(stream, "warn: "); break;
    case LOG_LEVEL_ERROR: fprintf(stream, "error: "); break;
    default: fprintf(stream, "LogLevel(%d): ", level);
    }

    for (const char *ch = file + strlen(file) - 1; ch >= file; --ch) {
        if (*ch == '/') {
            file = ch + 1;
            break;
        }
    }

    fprintf(stream, "(%s:%d) ", file, line);

    return stream;
}

FILE *log_print_begin_msg(LogLevel level, const char *file, int line, const char *msg)
{
    FILE *stream = log_print_begin(level, file, line);
    if (stream == NULL) return NULL;
    fputs(msg, stream);
    return stream;
}
