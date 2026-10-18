#pragma once

#include <stdbool.h>
#include <stdint.h>


uint64_t avp_time_counter(void);
uint64_t avp_time_ticks_per_microsecond(void);
uint64_t avp_time_microseconds(void);
uint64_t avp_time_microseconds_to_ticks(uint64_t microseconds);
bool avp_time_has_elapsed(uint64_t start, uint64_t duration);
void avp_time_delay_microseconds(uint64_t microseconds);
void avp_time_wait_until_microseconds(uint64_t deadline);
