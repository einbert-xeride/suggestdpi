#ifndef FORMAT_H
#define FORMAT_H

#include <stdio.h>

const char *fmt_escape_char(char ch);
void fmt_escape_string(FILE *stream, const char *str);
void fmt_quote_string(FILE *stream, const char *str);

#endif // FORMAT_H
