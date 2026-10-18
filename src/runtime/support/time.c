#include "avp/runtime/support/time.h"


uint64_t avp_time_counter(void) {
    uint64_t counter;
    __asm__ volatile(
        "dsb sy\n"
        "isb\n"
        "mrs %0, cntvct_el0"
        : "=r"(counter)
        :
        : "memory"
    );
    return counter;
}

uint64_t avp_time_ticks_per_microsecond(void) {
    uint64_t frequency;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    return frequency / 1000000;
}

uint64_t avp_time_microseconds(void) {
    return avp_time_counter() / avp_time_ticks_per_microsecond();
}

uint64_t avp_time_microseconds_to_ticks(uint64_t microseconds) {
    return microseconds * avp_time_ticks_per_microsecond();
}

bool avp_time_has_elapsed(uint64_t start, uint64_t duration) {
    return avp_time_counter() - start >= duration;
}

void avp_time_delay_microseconds(uint64_t microseconds) {
    const uint64_t start = avp_time_counter();
    const uint64_t duration = avp_time_microseconds_to_ticks(microseconds);
    while (!avp_time_has_elapsed(start, duration)) {
        __asm__ volatile("yield");
    }
}

void avp_time_wait_until_microseconds(uint64_t deadline) {
    const uint64_t current = avp_time_microseconds();
    if (current < deadline) {
        avp_time_delay_microseconds(deadline - current);
    }
}
