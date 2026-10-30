#pragma once

#include <stdbool.h>
#include "driver/i2c_master.h"

// Khoi tao bus I2C cho codec va quet cac thiet bi dang tra loi (de chan doan phan cung).
i2c_master_bus_handle_t board_i2c_init(void);
bool board_i2c_has_device(uint8_t addr);

// Khoi tao nut BOOT va cam bien cham (neu co).
void board_button_init(void);
bool board_button_pressed(void);
bool board_touch_pressed(void);
