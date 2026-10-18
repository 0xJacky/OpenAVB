#include "avp/platform/platform_panic_owner.h"

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


static jmp_buf reset_environment;
static unsigned interrupt_count;
static unsigned cache_count;
static unsigned console_count;
static unsigned panic_event_count;
static unsigned progress_count;
static bool bootstrap_task;
static uint32_t reset_reason;
static uint32_t progress_words[4];
static char console_messages[3][PLATFORM_PANIC_BUFFER_SIZE];
static char panic_buffer[PLATFORM_PANIC_BUFFER_SIZE];

uint64_t avp_virtual_counter_read_thunk(void) {
    return 0;
}

void platform_dma_cache_clean_all_unlocked(void) {
}

uint32_t platform_progress_read_code(void) {
    return 0;
}

void platform_progress_publish_words(
    uint32_t code,
    uint32_t detail,
    uint32_t auxiliary0,
    uint32_t auxiliary1
) {
    (void)code;
    (void)detail;
    (void)auxiliary0;
    (void)auxiliary1;
}

_Noreturn void platform_system_reset(uint32_t reason) {
    (void)reason;
    __builtin_trap();
}

void pvpanic_raise_panic_event(void) {
}

int32_t runtime_console_write_cstring_status(RuntimeFirebloomSpan source) {
    (void)source;
    return 0;
}

RuntimeFirebloomSpan runtime_current_task_bounded_view(void) {
    return (RuntimeFirebloomSpan){0};
}

bool runtime_current_task_is_bootstrap_sentinel(const void *task) {
    (void)task;
    return false;
}

void runtime_interrupt_mask_push_svc(void) {
}

static void interrupt_push(void *context) {
    assert(context == &interrupt_count);
    ++interrupt_count;
}

static void cache_clean(void *context) {
    assert(context == &interrupt_count);
    ++cache_count;
}

static int console_write(const char *message, void *context) {
    assert(context == &interrupt_count);
    assert(console_count < 3);
    size_t length = strlen(message);
    assert(length < sizeof(console_messages[0]));
    memcpy(console_messages[console_count], message, length + 1);
    ++console_count;
    return 0;
}

static uint64_t counter_read(void *context) {
    assert(context == &interrupt_count);
    return UINT64_C(0x12345678abcdef01);
}

static uint32_t progress_read(void *context) {
    assert(context == &interrupt_count);
    return 0x55aa;
}

static void progress_publish(
    uint32_t code,
    uint32_t detail,
    uint32_t auxiliary0,
    uint32_t auxiliary1,
    void *context
) {
    assert(context == &interrupt_count);
    progress_words[0] = code;
    progress_words[1] = detail;
    progress_words[2] = auxiliary0;
    progress_words[3] = auxiliary1;
    ++progress_count;
}

static void panic_event_raise(void *context) {
    assert(context == &interrupt_count);
    ++panic_event_count;
}

static void reset(uint32_t reason, void *context) {
    assert(context == &interrupt_count);
    reset_reason = reason;
    longjmp(reset_environment, 1);
}

static bool current_task_is_bootstrap(void *context) {
    assert(context == &interrupt_count);
    return bootstrap_task;
}

static void reset_fixture(void) {
    interrupt_count = 0;
    cache_count = 0;
    console_count = 0;
    panic_event_count = 0;
    progress_count = 0;
    bootstrap_task = false;
    reset_reason = 0;
    memset(progress_words, 0, sizeof(progress_words));
    memset(console_messages, 0, sizeof(console_messages));
    memset(panic_buffer, 0, sizeof(panic_buffer));
    platform_panic_bind((PlatformPanicBinding){
        .interrupt_mask_push = interrupt_push,
        .cache_clean_all = cache_clean,
        .console_write = console_write,
        .counter_read = counter_read,
        .progress_read = progress_read,
        .progress_publish = progress_publish,
        .panic_event_raise = panic_event_raise,
        .reset = reset,
        .current_task_is_bootstrap = current_task_is_bootstrap,
        .panic_buffer = panic_buffer,
        .panic_buffer_capacity = sizeof(panic_buffer),
        .context = &interrupt_count,
    });
}

static void test_double_panic_transcript(void) {
    reset_fixture();
    if (setjmp(reset_environment) == 0) {
        platform_publish_double_panic("allocator");
    }
    assert(reset_reason == PLATFORM_PANIC_RESET_REASON);
    assert(interrupt_count == 1 && cache_count == 1);
    assert(console_count == 3 && panic_event_count == 0);
    assert(strcmp(console_messages[0], "\n\n") == 0);
    assert(strcmp(console_messages[1], "double panic in allocator") == 0);
    assert(strcmp(console_messages[2], "\n\n") == 0);
    const PlatformPanicRecord *record = platform_panic_last_record();
    assert(strcmp(record->buffer, "double panic in allocator") == 0);
    assert(strcmp(record->source.current, "allocator") == 0);
}

static void test_iboot_panic_transcript(void) {
    reset_fixture();
    static const RuntimeFirebloomType byte_type = {
        .identity = 1,
        .element_size = 1,
        .bytewise_copyable = true,
    };
    char message[] = "image rejected";
    RuntimeFirebloomSpan span = {
        .current = message,
        .lower_bound = message,
        .upper_bound = message + sizeof(message),
        .type = &byte_type,
    };
    if (setjmp(reset_environment) == 0) {
        platform_publish_iboot_panic("boot", &span);
    }
    assert(reset_reason == PLATFORM_PANIC_RESET_REASON);
    assert(interrupt_count == 1 && cache_count == 2);
    assert(console_count == 3 && panic_event_count == 1);
    assert(strcmp(console_messages[1],
        "iBoot Panic: boot: image rejected") == 0);
    assert(progress_count == 1);
    assert(progress_words[0] == PLATFORM_PANIC_PROGRESS_CODE);
    assert(progress_words[1] == UINT32_C(0xabcdef01));
    assert(progress_words[2] == 0x55aa && progress_words[3] == 0);
    const PlatformPanicRecord *record = platform_panic_last_record();
    assert(record->caller == UINT64_C(0x12345678abcdef01));
    assert(record->progress_code == 0x55aa);

    reset_fixture();
    bootstrap_task = true;
    if (setjmp(reset_environment) == 0) {
        platform_publish_iboot_panic(0, &span);
    }
    assert(interrupt_count == 0);
    assert(strcmp(console_messages[1],
        "iBoot Panic: : image rejected") == 0);
}

int main(void) {
    test_double_panic_transcript();
    test_iboot_panic_transcript();
    platform_panic_reset_binding();
    puts("platform panic owner tests passed");
}
