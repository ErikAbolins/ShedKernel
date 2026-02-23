#ifndef TIME_H
#define TIME_H

struct tm {
    u8 sec;
    u8 min;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
};

typedef unsigned long time_t;

extern time_t time(time_t *_timer);

extern struct tm *localtime_r(const time_t *_timer, struct tm *_tp);

extern void udelay(u32 usec);

#endif //TIME_H