#include "runtime_provider_typed_recovered_private.h"

/* These outliners only reshuffle already validated register tuples. */
#define RUNTIME_PROVIDER_RECOVERED_OUTLINER(name)                              \
    RuntimeProviderRecoveredValue name(RuntimeProviderRecoveredValue value) {  \
        return value;                                                          \
    }
#include "avp/runtime/ipc/runtime_provider_recovered_outliners.inc"
#undef RUNTIME_PROVIDER_RECOVERED_OUTLINER
