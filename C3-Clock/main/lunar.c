// Vietnamese lunar calendar, after Ho Ngoc Duc's published algorithm (astronomical new moons
// and sun longitude, time zone UTC+7).
#include "lunar.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TZ 7.0
#define PI 3.14159265358979323846

static const char *CAN[] = {"Giáp", "Ất", "Bính", "Đinh", "Mậu", "Kỷ", "Canh", "Tân", "Nhâm", "Quý"};
static const char *CHI[] = {"Tý", "Sửu", "Dần", "Mão", "Thìn", "Tỵ", "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"};

static long jd_from_date(int dd, int mm, int yy) {
    int a = (14 - mm) / 12, y = yy + 4800 - a, m = mm + 12 * a - 3;
    long jd = dd + (153 * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 - 32045;
    if (jd < 2299161) jd = dd + (153 * m + 2) / 5 + 365L * y + y / 4 - 32083;
    return jd;
}

static long new_moon_day(int k) {
    double T = k / 1236.85, T2 = T * T, T3 = T2 * T, dr = PI / 180;
    double jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * T2 - 0.000000155 * T3;
    jd1 += 0.00033 * sin((166.56 + 132.87 * T - 0.009173 * T2) * dr);
    double M = 359.2242 + 29.10535608 * k - 0.0000333 * T2 - 0.00000347 * T3;
    double Mpr = 306.0253 + 385.81691806 * k + 0.0107306 * T2 + 0.00001236 * T3;
    double F = 21.2964 + 390.67050646 * k - 0.0016528 * T2 - 0.00000239 * T3;
    double C1 = (0.1734 - 0.000393 * T) * sin(M * dr) + 0.0021 * sin(2 * dr * M);
    C1 = C1 - 0.4068 * sin(Mpr * dr) + 0.0161 * sin(dr * 2 * Mpr);
    C1 = C1 - 0.0004 * sin(dr * 3 * Mpr);
    C1 = C1 + 0.0104 * sin(dr * 2 * F) - 0.0051 * sin(dr * (M + Mpr));
    C1 = C1 - 0.0074 * sin(dr * (M - Mpr)) + 0.0004 * sin(dr * (2 * F + M));
    C1 = C1 - 0.0004 * sin(dr * (2 * F - M)) - 0.0006 * sin(dr * (2 * F + Mpr));
    C1 = C1 + 0.0010 * sin(dr * (2 * F - Mpr)) + 0.0005 * sin(dr * (2 * Mpr + M));
    double dt;
    if (T < -11) dt = 0.001 + 0.000839 * T + 0.0002261 * T2 - 0.00000845 * T3 - 0.000000081 * T * T3;
    else dt = -0.000278 + 0.000265 * T + 0.000262 * T2;
    return (long)floor(jd1 + C1 - dt + 0.5 + TZ / 24);
}

static double sun_longitude_rad(double jd_ut) {
    double T = (jd_ut - 2451545.0) / 36525, T2 = T * T, dr = PI / 180;
    double M = 357.52910 + 35999.05030 * T - 0.0001559 * T2 - 0.00000048 * T * T2;
    double L0 = 280.46645 + 36000.76983 * T + 0.0003032 * T2;
    double DL = (1.914600 - 0.004817 * T - 0.000014 * T2) * sin(dr * M);
    DL += (0.019993 - 0.000101 * T) * sin(dr * 2 * M) + 0.000290 * sin(dr * 3 * M);
    double L = (L0 + DL) * dr;
    return L - 2 * PI * floor(L / (2 * PI));
}

static int sun_longitude(long jdn) { return (int)floor(sun_longitude_rad(jdn - 0.5 - TZ / 24) / PI * 6); }

static long lunar_month11(int yy) {
    long off = jd_from_date(31, 12, yy) - 2415021;
    int k = (int)floor(off / 29.530588853);
    long nm = new_moon_day(k);
    if (sun_longitude(nm) >= 9) nm = new_moon_day(k - 1);
    return nm;
}

static int leap_month_offset(long a11) {
    int k = (int)floor((a11 - 2415021.076998695) / 29.530588853 + 0.5);
    int last, i = 1, arc = sun_longitude(new_moon_day(k + i));
    do {
        last = arc;
        i++;
        arc = sun_longitude(new_moon_day(k + i));
    } while (arc != last && i < 14);
    return i - 1;
}

void lunar_from_solar(int dd, int mm, int yy, lunar_t *o) {
    long day = jd_from_date(dd, mm, yy);
    int k = (int)floor((day - 2415021.076998695) / 29.530588853);
    long month_start = new_moon_day(k + 1);
    if (month_start > day) month_start = new_moon_day(k);
    long a11 = lunar_month11(yy), b11 = a11;
    int ly;
    if (a11 >= month_start) { ly = yy; a11 = lunar_month11(yy - 1); }
    else { ly = yy + 1; b11 = lunar_month11(yy + 1); }
    int ld = (int)(day - month_start + 1);
    int diff = (int)floor((month_start - a11) / 29.0);
    int leap = 0, lm = diff + 11;
    if (b11 - a11 > 365) {
        int lmd = leap_month_offset(a11);
        if (diff >= lmd) {
            lm = diff + 10;
            if (diff == lmd) leap = 1;
        }
    }
    if (lm > 12) lm -= 12;
    if (lm >= 11 && diff < 4) ly -= 1;
    o->day = ld; o->month = lm; o->year = ly; o->leap = leap; o->jd = day;
}

void lunar_can_chi_year(int y, char *b, size_t n) { snprintf(b, n, "%s %s", CAN[(y + 6) % 10], CHI[(y + 8) % 12]); }
void lunar_can_chi_month(int m, int y, char *b, size_t n) { snprintf(b, n, "%s %s", CAN[(y * 12 + m + 3) % 10], CHI[(m + 1) % 12]); }
void lunar_can_chi_day(long jd, char *b, size_t n) { snprintf(b, n, "%s %s", CAN[(jd + 9) % 10], CHI[(jd + 1) % 12]); }

void lunar_good_hours(long jd, char *b, size_t n) {
    static const char *GIO_HD[] = {"110100101100", "001101001011", "110011010010",
                                   "101100110100", "001011001101", "010010110011"};
    const char *bits = GIO_HD[((jd + 1) % 12) % 6];
    b[0] = 0;
    for (int i = 0; i < 12; i++) {
        if (bits[i] != '1') continue;
        if (b[0]) strncat(b, ", ", n - strlen(b) - 1);
        strncat(b, CHI[i], n - strlen(b) - 1);
    }
}

const char *lunar_solar_term(int dd, int mm, int yy) {
    static const char *TERMS[] = {"Xuân phân", "Thanh minh", "Cốc vũ", "Lập hạ", "Tiểu mãn", "Mang chủng",
                                  "Hạ chí", "Tiểu thử", "Đại thử", "Lập thu", "Xử thử", "Bạch lộ",
                                  "Thu phân", "Hàn lộ", "Sương giáng", "Lập đông", "Tiểu tuyết", "Đại tuyết",
                                  "Đông chí", "Tiểu hàn", "Đại hàn", "Lập xuân", "Vũ thủy", "Kinh trập"};
    double L = sun_longitude_rad(jd_from_date(dd, mm, yy) + 0.5 - TZ / 24) * 180 / PI;   // end of local day
    return TERMS[(int)(L / 15) % 24];
}

const char *lunar_holiday(int dd, int mm, const lunar_t *l) {
    static const struct { int d, m; const char *name; } SOLAR[] = {
        {1, 1, "Tết Dương lịch"}, {14, 2, "Lễ Tình nhân"}, {8, 3, "Quốc tế Phụ nữ"},
        {30, 4, "Giải phóng miền Nam"}, {1, 5, "Quốc tế Lao động"}, {1, 6, "Quốc tế Thiếu nhi"},
        {2, 9, "Quốc khánh"}, {20, 10, "Phụ nữ Việt Nam"}, {20, 11, "Nhà giáo Việt Nam"},
        {22, 12, "Quân đội Nhân dân"}, {24, 12, "Đêm Giáng sinh"}, {25, 12, "Giáng sinh"}};
    static const struct { int d, m; const char *name; } LUNAR[] = {
        {1, 1, "Tết Nguyên Đán"}, {2, 1, "Mùng 2 Tết"}, {3, 1, "Mùng 3 Tết"}, {15, 1, "Rằm tháng Giêng"},
        {10, 3, "Giỗ Tổ Hùng Vương"}, {15, 4, "Lễ Phật Đản"}, {5, 5, "Tết Đoan Ngọ"},
        {15, 7, "Lễ Vu Lan"}, {15, 8, "Tết Trung Thu"}, {23, 12, "Ông Công Ông Táo"}};
    if (!l->leap)
        for (size_t i = 0; i < sizeof LUNAR / sizeof LUNAR[0]; i++)
            if (LUNAR[i].d == l->day && LUNAR[i].m == l->month) return LUNAR[i].name;
    for (size_t i = 0; i < sizeof SOLAR / sizeof SOLAR[0]; i++)
        if (SOLAR[i].d == dd && SOLAR[i].m == mm) return SOLAR[i].name;
    if (l->day == 1) return "Mùng 1";
    if (l->day == 15) return "Ngày Rằm";
    return NULL;
}

int solar_days_in_month(int m, int y) {
    static const int D[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return D[m - 1];
}

int solar_weekday(int d, int m, int y) { return (int)((jd_from_date(d, m, y) + 1) % 7); }
