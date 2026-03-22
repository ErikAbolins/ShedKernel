/*
 * time.c - Kernel time utilities
 *
 * Converts a broken-down struct tm into a Unix timestamp (seconds
 * since 1970-01-01 00:00:00). Assumes a 365.25-day year model with
 * a fixed leap-year correction — good enough for boot-time RTC use.
 */

#include "time.h"

#define MINUTE  60
#define HOUR    (MINUTE * 60)
#define DAY     (24 * HOUR)
#define YEAR    (365 * DAY)

/* Cumulative seconds from Jan 1 to the start of each month (non-leap year,
 * but February is kept at 29 days — the leap correction happens below). */
static const int month[12] = {
    0,
    DAY * 31,                                       /* Feb  */
    DAY * (31 + 29),                                /* Mar  */
    DAY * (31 + 29 + 31),                           /* Apr  */
    DAY * (31 + 29 + 31 + 30),                      /* May  */
    DAY * (31 + 29 + 31 + 30 + 31),                 /* Jun  */
    DAY * (31 + 29 + 31 + 30 + 31 + 30),            /* Jul  */
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31),       /* Aug  */
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),  /* Sep  */
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),          /* Oct */
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),     /* Nov */
    DAY * (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30) /* Dec */
};

long kernel_mktime(struct tm *tm)
{
    int  year = tm->tm_year - 70;
    long res  = YEAR * year + DAY * ((year + 1) / 4);

    res += month[tm->tm_mon];

    /* Subtract a day for non-leap years past February */
    if (tm->tm_mon > 1 && ((year + 4) % 4))
        res -= DAY;

    res += DAY    * (tm->tm_mday - 1);
    res += HOUR   * tm->tm_hour;
    res += MINUTE * tm->tm_min;
    res += tm->tm_sec;

    return res;
}
