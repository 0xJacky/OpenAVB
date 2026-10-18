#include "avp/runtime/boot/boot_progress_recovered.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


typedef struct {
    uint32_t words[4];
    uint32_t transcript[32];
    size_t transcript_count;
} ProgressFixture;

static uint32_t fixture_read(void *context, uint32_t index) {
    ProgressFixture *fixture = context;
    assert(index < 4);
    return fixture->words[index];
}

static void fixture_write(void *context, uint32_t index, uint32_t value) {
    ProgressFixture *fixture = context;
    assert(index < 4);
    assert(fixture->transcript_count + 2 <= 32);
    fixture->words[index] = value;
    fixture->transcript[fixture->transcript_count++] = index;
    fixture->transcript[fixture->transcript_count++] = value;
}

static void bind_fixture(ProgressFixture *fixture) {
    *fixture = (ProgressFixture){0};
    boot_progress_bind_transport((BootProgressTransport){
        .read_word = fixture_read,
        .write_word = fixture_write,
        .context = fixture,
    });
    boot_progress_reset_cache();
}

static void test_publish_and_suppression(void) {
    ProgressFixture fixture;
    bind_fixture(&fixture);
    platform_progress_publish_words(0x10, 0x20, 0x30, 0x40);
    assert(fixture.transcript_count == 8);
    assert(fixture.words[0] == 0x10);
    assert(fixture.words[1] == 0x20);
    assert(fixture.words[2] == 0x30);
    assert(fixture.words[3] == 0x40);
    platform_progress_publish_words(0x10, 0x20, 0xaa, 0xbb);
    assert(fixture.transcript_count == 8);
    platform_progress_publish_code(0x11);
    assert(fixture.transcript_count == 16);
    assert(fixture.words[0] == 0x11);
    assert(fixture.words[1] == 0);
    assert(fixture.words[2] == 0);
    assert(fixture.words[3] == 0);
    assert(platform_progress_read_code() == 0x11);
    platform_progress_clear();
    assert(platform_progress_read_code() == 0);
}

static void test_source_attempt_transcript(void) {
    ProgressFixture fixture;
    bind_fixture(&fixture);
    boot_progress_publish_source_attempt_state(7, 8, 9);
    assert(fixture.transcript_count == 16);
    assert(fixture.transcript[1] == BOOT_PROGRESS_SOURCE_ATTEMPT);
    assert(fixture.transcript[3] == 8);
    assert(fixture.transcript[5] == 7);
    assert(fixture.transcript[7] == 9);
    assert(fixture.transcript[9] == BOOT_PROGRESS_SOURCE_ATTEMPT_COMPLETE);
    assert(fixture.transcript[11] == 0);
    assert(fixture.transcript[13] == 0);
    assert(fixture.transcript[15] == 0);
}

static void test_dfu_state(void) {
    ProgressFixture fixture;
    bind_fixture(&fixture);
    boot_progress_publish_dfu_active_state(true);
    assert(fixture.words[0] == BOOT_PROGRESS_DFU_ACTIVE);
    assert(fixture.words[1] == 1);
    boot_progress_publish_dfu_active_state(false);
    assert(fixture.words[0] == BOOT_PROGRESS_DFU_ACTIVE);
    assert(fixture.words[1] == 0);
}

int main(void) {
    test_publish_and_suppression();
    test_source_attempt_transcript();
    test_dfu_state();
    puts("boot progress recovered tests passed");
    return 0;
}
