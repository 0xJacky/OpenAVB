#pragma once

#include <stdint.h>


#define RUNTIME_BOOTSTRAP_STACK_GUARD UINT64_C(0x4d6963720035746b)
#define RUNTIME_STACK_GUARD_MASK UINT64_C(0xffffffffffff00ff)

uintptr_t runtime_reset_entry_pointer(void);
uint64_t runtime_generate_stack_guard_seed(void);
void runtime_stage_stack_guard(uint64_t guard);
void runtime_install_stack_guard(void);
uint64_t runtime_stack_guard(void);
