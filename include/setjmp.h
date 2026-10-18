#ifndef AVP_FREESTANDING_SETJMP_H
#define AVP_FREESTANDING_SETJMP_H

#if __STDC_HOSTED__
#include_next <setjmp.h>
#else
#include <stdint.h>

typedef struct {
    uint64_t registers[13];
} jmp_buf[1];

int avp_setjmp(jmp_buf environment) __attribute__((returns_twice));
_Noreturn void avp_longjmp(jmp_buf environment, int value);

#define setjmp(environment) avp_setjmp(environment)
#define longjmp(environment, value) avp_longjmp(environment, value)
#endif

#endif
