#include "splash_screen.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



/**
 * @brief Draw the Ethereum logo and a progress bar using U8g2.
 */
void draw_splash_progress(u8g2_t *u8g2, int progress_percent) {
    u8g2_ClearBuffer(u8g2);

    // Set large font for brand-like appearance
    u8g2_SetFont(u8g2, u8g2_font_helvB08_tf);  // You can choose a different font if you prefer

    const char *text = "PENGUIN";
    int textWidth = u8g2_GetStrWidth(u8g2, text);
    int fontAscent = u8g2_GetAscent(u8g2);    // Height above baseline
    int fontDescent = u8g2_GetDescent(u8g2);  // Depth below baseline (usually negative)

    int textHeight = fontAscent - fontDescent;
    int spacing = 4;         // Space between text and progress bar
    int barHeight = 4;
    int totalHeight = textHeight + spacing + barHeight;

    int displayWidth = u8g2_GetDisplayWidth(u8g2);
    int displayHeight = u8g2_GetDisplayHeight(u8g2);

    // Vertically center the whole content
    int startY = (displayHeight - totalHeight) / 2;

    // Horizontally center the text
    int textX = (displayWidth - textWidth) / 2;
    int textY = startY + fontAscent;

    u8g2_DrawStr(u8g2, textX, textY, text);

    // Draw progress bar below the text
    int barY = textY + spacing;
    int barWidth = displayWidth;

    u8g2_DrawFrame(u8g2, 0, barY, barWidth, barHeight);
    u8g2_DrawBox(u8g2, 0, barY, (barWidth * progress_percent) / 100, barHeight);

    u8g2_SendBuffer(u8g2);
}


/**
 * @brief Show splash screen and sequentially run init tasks.
 */
void show_splash_screen(u8g2_t *u8g2, InitTask tasks[], int task_count) {
    draw_splash_progress(u8g2, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    for (int i = 0; i < task_count; ++i) {
        tasks[i]();
        int progress = ((i + 1) * 100) / task_count;
        draw_splash_progress(u8g2, progress);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    vTaskDelay(pdMS_TO_TICKS(500));
}