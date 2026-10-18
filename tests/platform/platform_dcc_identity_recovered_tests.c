#include "avp/platform/platform_dcc_identity_recovered.h"

#include <assert.h>
#include <stdint.h>


static unsigned query_count;

static uint64_t query(void) {
    ++query_count;
    return query_count == 1 ? UINT64_C(0x000000000000ab34)
                            : UINT64_C(0x000000000000cd56);
}

int main(void) {
    platform_dcc_identity_bind_query(query);
    platform_dcc_identity_reset_cache();
    assert(platform_dcc_get_high_byte() == 0xab);
    assert(platform_dcc_get_high_byte() == 0xab);
    assert(query_count == 1);
    assert(platform_dcc_get_low_byte() == 0x56);
    assert(platform_dcc_get_low_byte() == 0x56);
    assert(query_count == 2);

    platform_dcc_identity_reset_cache();
    assert(platform_dcc_query_high_byte() == 0xcd);
    assert(platform_dcc_query_low_byte() == 0x56);
    assert(query_count == 4);
    return 0;
}
