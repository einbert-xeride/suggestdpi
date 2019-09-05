#ifndef LOG_H
#define LOG_H

#include <stdio.h>

typedef enum LogLevel {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} LogLevel;

void log_set_level(LogLevel level);
LogLevel log_get_level();
void log_set_output(FILE *restrict output);
FILE *log_get_output();

void log_print(LogLevel level, const char *file, int line, const char *fmt, ...);
FILE *log_print_begin(LogLevel level, const char *file, int line);
FILE *log_print_begin_msg(LogLevel level, const char *file, int line, const char *msg);

#define LOG(L, FMT, ...) log_print(LOG_LEVEL_##L, __FILE__, __LINE__, FMT, ##__VA_ARGS__)
#define LOGB(L, out) for (FILE *out = log_print_begin(LOG_LEVEL_##L, __FILE__, __LINE__); (out) != NULL; fputc('\n', (out)), (out) = NULL)
#define LOGBM(L, out, msg) for (FILE *out = log_print_begin_msg(LOG_LEVEL_##L, __FILE__, __LINE__, msg); (out) != NULL; fputc('\n', (out)), (out) = NULL)

#endif // LOG_H
