#include <stdarg.h>


void error(int errno, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
}