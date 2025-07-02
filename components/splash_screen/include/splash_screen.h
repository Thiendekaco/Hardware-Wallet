#ifndef SPLASH_SCREEN_H
#define SPLASH_SCREEN_H

#include "u8g2.h"

typedef void (*InitTask)();

/**
 * @brief Show splash screen with U8g2 driver.
 *
 * @param u8g2 Pointer to U8g2 instance.
 * @param tasks Array of init functions to run during splash.
 * @param taskCount Number of init tasks.
 */
void show_splash_screen(u8g2_t *u8g2, InitTask tasks[], int taskCount);


/**
 * @brief Draw the Ethereum logo and a progress bar.
 *
 * @param u8g2 Pointer to U8g2 instance.
 * @param progress_percent Progress percentage (0-100).
 */
void draw_splash_progress(u8g2_t *u8g2, int progress_percent);

#endif // SPLASH_SCREEN_H
