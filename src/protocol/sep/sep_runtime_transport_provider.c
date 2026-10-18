#include "avp/protocol/sep/sep_runtime_transport.h"


enum {
    SEP_RUNTIME_TRANSPORT_PROVIDER_TYPE = 2,
    SEP_RUNTIME_TRANSPORT_BASE = 0x30250000,
};

/* These constants are part of the fixed vphone600 provider profile. */
static const SEPRuntimeTransportLayout runtime_transport_layout
    __attribute__((used, retain)) = {
    0x20,
    0x1c,
    0x200,
    0x100,
    0x18,
    0x18,
    0,
};

static const SEPRuntimeTransportProvider runtime_transport_provider
    __attribute__((used, retain)) = {
    .type = SEP_RUNTIME_TRANSPORT_PROVIDER_TYPE,
    .register_base = (volatile uint8_t *)SEP_RUNTIME_TRANSPORT_BASE,
};

static const SEPRuntimeTransportProfile runtime_transport_profile
    __attribute__((used, retain)) = {
    .providers = &runtime_transport_provider,
    .provider_count = 1,
    .layout = {
        .pointer = &runtime_transport_layout,
        .lower_bound = &runtime_transport_layout,
        .upper_bound = (const uint8_t *)&runtime_transport_layout +
            sizeof(runtime_transport_layout),
    },
};

const SEPRuntimeTransportProfile *runtime_profile_sep_transport(void) {
    return &runtime_transport_profile;
}

static const SEPRuntimeTransportProvider *provider_lookup(
    const SEPRuntimeTransportProfile *profile,
    uint16_t type
) {
    for (size_t index = 0; index < profile->provider_count; ++index) {
        if ((uint16_t)profile->providers[index].type == type) {
            return &profile->providers[index];
        }
    }
    return 0;
}

bool sep_runtime_transport_prepare_view(
    const SEPRuntimeTransportProfile *profile,
    uint16_t provider_type,
    SEPRuntimeTransportView *view
) {
    const SEPRuntimeTransportProvider *provider = provider_lookup(
        profile,
        provider_type
    );
    if (provider == 0) {
        return false;
    }
    *view = (SEPRuntimeTransportView) {
        .register_base = provider->register_base,
        .provider_auxiliary = provider->auxiliary,
        .layout = profile->layout,
    };
    return true;
}

bool sep_runtime_transport_view_is_valid(
    const SEPRuntimeTransportView *view
) {
    const uintptr_t pointer = (uintptr_t)view->layout.pointer;
    const uintptr_t lower = (uintptr_t)view->layout.lower_bound;
    const uintptr_t upper = (uintptr_t)view->layout.upper_bound;
    return view->register_base != 0 &&
        pointer >= lower &&
        upper >= lower &&
        upper - lower >= sizeof(SEPRuntimeTransportLayout) &&
        pointer - lower <= upper - lower -
            sizeof(SEPRuntimeTransportLayout);
}

const SEPRuntimeTransportLayout *sep_runtime_transport_view_layout(
    const SEPRuntimeTransportView *view
) {
    return sep_runtime_transport_view_is_valid(view)
        ? view->layout.pointer
        : 0;
}
