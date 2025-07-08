#ifndef UI_STATE_H
#define UI_STATE_H

#include <stdbool.h>

extern volatile bool g_ui_busy;

void ui_set_busy(bool busy);
bool ui_is_busy(void);
void ui_wait_until_free(void);

#endif // UI_STATE_H