#ifndef SPLASH_SCREEN_H
#define SPLASH_SCREEN_H

#include "ssd1306.h"
typedef void (*InitTask)();

void show_splash_screen(SSD1306_t *dev, InitTask tasks[], int taskCount);

#endif
