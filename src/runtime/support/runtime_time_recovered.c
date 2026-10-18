#include "avp/runtime/support/runtime_time.h"


/*
 * Related public time conversion APIs:
 * Zephyr 0d65ce4 k_ticks_to_us_floor64 (Apache-2.0):
 * https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9
 * FreeRTOS-Kernel ce221a8 pdMS_TO_TICKS (MIT):
 * https://github.com/FreeRTOS/FreeRTOS-Kernel/commit/ce221a8bb468e462ca6b435cef66a9636e00baf4
 * musl f21a965 clock_gettime layer (MIT):
 * https://github.com/ifduyue/musl/commit/f21a96538f78fa8e2040831b4209b35f2fb581da
 * AVP exposes the architectural virtual counter directly, so host tests use
 * narrow hooks while AArch64 keeps the compatible register behavior.
 */

enum {
    RUNTIME_MICROSECONDS_PER_SECOND = 1000000,
};

static RuntimeTimePlatform runtime_time_platform;
static bool runtime_time_platform_bound;

static _Noreturn void runtime_time_fail(void) {
    __builtin_trap();
}

void runtime_time_bind_platform(const RuntimeTimePlatform *platform) {
    if (platform == 0 || platform->read_counter == 0 ||
        platform->counter_frequency == 0) {
        runtime_time_fail();
    }
    runtime_time_platform = *platform;
    runtime_time_platform_bound = true;
}

uint64_t avp_virtual_counter_read(void) {
    if (runtime_time_platform_bound) {
        return runtime_time_platform.read_counter(
            runtime_time_platform.context
        );
    }
#if defined(__aarch64__)
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
#else
    runtime_time_fail();
#endif
}

uint64_t avp_counter_frequency(void) {
    if (runtime_time_platform_bound) {
        return runtime_time_platform.counter_frequency(
            runtime_time_platform.context
        );
    }
#if defined(__aarch64__)
    uint64_t frequency;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    return frequency;
#else
    runtime_time_fail();
#endif
}

uint64_t avp_counter_ticks_per_microsecond(void) {
    const uint64_t ticks = avp_counter_frequency() /
        RUNTIME_MICROSECONDS_PER_SECOND;
    if (ticks == 0) {
        runtime_time_fail();
    }
    return ticks;
}

uint64_t avp_counter_ticks_to_microseconds(uint64_t ticks) {
    return ticks / avp_counter_ticks_per_microsecond();
}

uint64_t avp_microseconds_to_ticks(uint64_t microseconds) {
    const uint64_t ticks_per_microsecond =
        avp_counter_ticks_per_microsecond();
    if (microseconds > UINT64_MAX / ticks_per_microsecond) {
        runtime_time_fail();
    }
    return microseconds * ticks_per_microsecond;
}

uint64_t avp_virtual_counter_read_thunk(void) {
    return avp_virtual_counter_read();
}

uint64_t j_avp_virtual_counter_read_clone(void) {
    return avp_virtual_counter_read();
}

uint64_t runtime_microsecond_time_read(void) {
    return avp_counter_ticks_to_microseconds(
        avp_virtual_counter_read_thunk()
    );
}

bool runtime_microsecond_interval_elapsed(
    uint64_t start_microseconds,
    uint64_t duration_microseconds
) {
    return runtime_microsecond_time_read() - start_microseconds >=
        duration_microseconds;
}

static void runtime_time_wait_iteration(void) {
    if (runtime_time_platform_bound &&
        runtime_time_platform.wait_iteration != 0) {
        runtime_time_platform.wait_iteration(runtime_time_platform.context);
        return;
    }
#if defined(__aarch64__)
    __asm__ volatile("yield");
#endif
}

void runtime_delay_microseconds(uint64_t microseconds) {
    const uint64_t start = avp_virtual_counter_read_thunk();
    const uint64_t duration = avp_microseconds_to_ticks(microseconds);
    while (avp_virtual_counter_read_thunk() - start < duration) {
        runtime_time_wait_iteration();
    }
}

void runtime_conditional_delay_microseconds(uint64_t microseconds) {
    if (runtime_time_platform_bound &&
        runtime_time_platform.should_skip_delay != 0 &&
        runtime_time_platform.should_skip_delay(
            runtime_time_platform.context
        )) {
        return;
    }
    runtime_delay_microseconds(microseconds);
}
