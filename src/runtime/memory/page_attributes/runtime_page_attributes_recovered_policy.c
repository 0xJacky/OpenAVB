#include "runtime_page_attributes_recovered_private.h"

void runtime_page_attribute_policy_initialize(
    RuntimePageAttributeRegistry *registry,
    uintptr_t allowed_lower,
    uintptr_t allowed_upper,
    uintptr_t forbidden_lower,
    uintptr_t forbidden_upper
) {
    const uintptr_t page_mask = RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE - 1;
    if (registry == 0 || !registry->initialized ||
        allowed_lower >= allowed_upper ||
        ((allowed_lower | allowed_upper) & page_mask) != 0 ||
        ((forbidden_lower | forbidden_upper) & page_mask) != 0 ||
        forbidden_lower > forbidden_upper ||
        (forbidden_lower != forbidden_upper &&
         (forbidden_lower < allowed_lower ||
          forbidden_upper > allowed_upper))) {
        runtime_page_attributes_fail();
    }
    registry->allowed_lower = allowed_lower;
    registry->allowed_upper = allowed_upper;
    registry->forbidden_lower = forbidden_lower;
    registry->forbidden_upper = forbidden_upper;
    registry->policy_active = true;
}

bool runtime_page_attribute_address_allowed(
    RuntimePageAttributeRegistry *registry,
    uintptr_t address
) {
    if (registry == 0 || !registry->initialized) {
        return false;
    }
    if (!registry->policy_active) {
        return true;
    }
    if (address < registry->allowed_lower ||
        address >= registry->allowed_upper) {
        return false;
    }
    return registry->forbidden_lower == registry->forbidden_upper ||
           address < registry->forbidden_lower ||
           address >= registry->forbidden_upper;
}

RuntimePageAttributeByteView
runtime_page_attribute_policy_active_flag_bounded_view(
    RuntimePageAttributeRegistry *registry) {
    if (registry == 0) {
        runtime_page_attributes_fail();
    }
    return (RuntimePageAttributeByteView){
        .current = &registry->policy_active,
        .lower_bound = &registry->policy_active,
        .upper_bound = (uint8_t *)&registry->policy_active +
                       sizeof(registry->policy_active),
    };
}

uint8_t runtime_page_attribute_policy_active_flag_to_w8(
    const RuntimePageAttributeRegistry *registry) {
    return registry != 0 && registry->policy_active ? 1 : 0;
}
