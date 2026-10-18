#pragma once

#include "avp/runtime/ipc/runtime_domain.h"

#include <stdbool.h>
#include <stdint.h>


bool runtime_domain_query_locality(uint8_t requested_domain);
