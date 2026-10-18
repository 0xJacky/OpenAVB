#pragma once

#include <stdint.h>


uint8_t pvpanic_initialize(void);
__attribute__((used, retain)) int pvpanic_probe_and_cache(void);
__attribute__((used, retain)) void pvpanic_raise_panic_event(void);
uint8_t pvpanic_select_panic_event(uint8_t capabilities);
_Noreturn void pvpanic_trigger(void);
