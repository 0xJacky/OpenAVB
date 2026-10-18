#include "avp/runtime/memory/runtime_memory.h"

#include <assert.h>
#include <stdint.h>


_Noreturn void pvpanic_trigger(void) {
    assert(0 && "unexpected runtime memory fatal path");
    __builtin_unreachable();
}

int main(void) {
    uint8_t bytes[16];
    runtime_memset(bytes, 0x5a, sizeof(bytes));
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        assert(bytes[index] == 0x5a);
    }

    runtime_bzero(bytes + 4, 8);
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        assert(bytes[index] == (index >= 4 && index < 12 ? 0 : 0x5a));
    }

    uint8_t forward[] = {0, 1, 2, 3, 4, 5, 6, 7};
    runtime_memmove(forward, forward + 2, 6);
    const uint8_t expected_forward[] = {2, 3, 4, 5, 6, 7, 6, 7};
    for (size_t index = 0; index < sizeof(forward); ++index) {
        assert(forward[index] == expected_forward[index]);
    }

    uint8_t backward[] = {0, 1, 2, 3, 4, 5, 6, 7};
    runtime_memmove(backward + 2, backward, 6);
    const uint8_t expected_backward[] = {0, 1, 0, 1, 2, 3, 4, 5};
    for (size_t index = 0; index < sizeof(backward); ++index) {
        assert(backward[index] == expected_backward[index]);
    }

    const char text[] = "avp";
    assert(runtime_bounded_strnlen(text, text, text + sizeof(text), 2) == 2);
    assert(runtime_bounded_strnlen(
        text,
        text,
        text + sizeof(text),
        sizeof(text)
    ) == 3);
    assert(runtime_bounded_strlen(text, text, text + sizeof(text)) == 3);
}
