#pragma once

#include <stdbool.h>

// Pin 1S Li-ion o CN4: do dien ap qua cau chia R52 68k / R57 100k vao GPIO53 (trang 2 so do).
typedef struct {
    bool valid;      // da co it nhat mot lan do
    bool present;    // co pin (dien ap trong khoang hop ly)
    int pin_mv;      // dien ap tai GPIO53
    int battery_mv;  // dien ap pin sau khi nhan he so chia
    int percent;     // uoc luong tu dien ap, -1 neu khong co pin
} battery_info_t;

void battery_init(void);
void battery_update(void);
void battery_get(battery_info_t *out);
