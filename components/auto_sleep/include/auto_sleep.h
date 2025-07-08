#ifndef AUTO_SLEEP_H
#define AUTO_SLEEP_H

#include <stdint.h>

void auto_sleep_init(uint64_t wakeup_mask);
void auto_sleep_record_activity(void);
void auto_sleep_check(void);

#endif // AUTO_SLEEP_H