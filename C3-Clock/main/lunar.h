#pragma once
#include <stddef.h>

typedef struct {
    int day, month, year, leap;
    long jd;   // Julian day number of the solar date
} lunar_t;

void lunar_from_solar(int dd, int mm, int yy, lunar_t *out);   // Vietnam time zone (UTC+7)
void lunar_can_chi_year(int lunar_year, char *buf, size_t n);
void lunar_can_chi_month(int lunar_month, int lunar_year, char *buf, size_t n);
void lunar_can_chi_day(long jd, char *buf, size_t n);
void lunar_good_hours(long jd, char *buf, size_t n);              // giờ hoàng đạo
const char *lunar_solar_term(int dd, int mm, int yy);            // tiết khí
const char *lunar_holiday(int dd, int mm, const lunar_t *l);     // NULL if none
int solar_days_in_month(int m, int y);
int solar_weekday(int d, int m, int y);                          // 0 = Sunday
