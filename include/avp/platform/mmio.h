#pragma once

#include <stdint.h>


static inline uint8_t mmio_read8(uintptr_t address) {
    return *(volatile uint8_t *)address;
}

static inline uint32_t mmio_read32(uintptr_t address) {
    return *(volatile uint32_t *)address;
}

static inline uint64_t mmio_read64(uintptr_t address) {
    return *(volatile uint64_t *)address;
}

static inline void mmio_write8(uintptr_t address, uint8_t value) {
    *(volatile uint8_t *)address = value;
}

static inline void mmio_write32(uintptr_t address, uint32_t value) {
    *(volatile uint32_t *)address = value;
}

static inline void mmio_write64(uintptr_t address, uint64_t value) {
    *(volatile uint64_t *)address = value;
}

static inline void avp_memory_barrier(void) {
    __asm__ volatile("dmb sy" ::: "memory");
}
