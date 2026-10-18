#include "avp/runtime/memory/allocator/runtime_allocator.h"
#include "avp/runtime/memory/runtime_memory.h"


#if defined(__ELF__)
#define RUNTIME_ALLOCATOR_RECOVERED \
    __attribute__((used, retain, noinline, section(".text.runtime_support_recovered")))
#else
#define RUNTIME_ALLOCATOR_RECOVERED __attribute__((noinline))
#endif

#define RUNTIME_ALLOCATOR_PAGE_SIZE ((uintptr_t)0x4000)
#define RUNTIME_ALLOCATOR_ADDRESS_MASK UINT64_C(0x00ffffffffffffff)

static RuntimeAllocatorRoutes current_routes;
static RuntimeAllocatorMappingProfile current_mapping_profile;

static uintptr_t address_of(const void *pointer) {
    return (uintptr_t)pointer;
}

static bool address_is_canonical(uintptr_t address) {
    const uintptr_t sign = (address >> 55) & 1;
    const uintptr_t high = address >> 42;
    return high == (sign == 0 ? 0 : UINT64_C(0x3fffff));
}

static bool route_owns_static_address(
    const RuntimeAllocatorRoute *route,
    uintptr_t address
) {
    const uintptr_t lower = route->owned_lower_bound &
        RUNTIME_ALLOCATOR_ADDRESS_MASK;
    const uintptr_t upper = route->owned_upper_bound &
        RUNTIME_ALLOCATOR_ADDRESS_MASK;
    const uintptr_t candidate = address & RUNTIME_ALLOCATOR_ADDRESS_MASK;
    return lower <= candidate && candidate <= upper;
}

static RuntimeAllocatorNode **find_allocation_link(
    RuntimeAllocatorRoute *route,
    uintptr_t address
) {
    RuntimeAllocatorNode **link = &route->allocations;
    while (*link != 0 && (*link)->lower_bound != address) {
        link = &(*link)->next;
    }
    return link;
}

static const RuntimeAllocatorNode *find_allocation(
    const RuntimeAllocatorRoute *route,
    uintptr_t address
) {
    const RuntimeAllocatorNode *node = route->allocations;
    while (node != 0 && node->lower_bound != address) {
        node = node->next;
    }
    return node;
}

void runtime_allocator_publish_routes(RuntimeAllocatorRoutes routes) {
    current_routes = routes;
}

RUNTIME_ALLOCATOR_RECOVERED
void runtime_allocator_copy_routes(
    RuntimeAllocatorRoute **primary,
    RuntimeAllocatorRoute **fallback
) {
    if (primary != 0) {
        *primary = current_routes.primary;
    }
    if (fallback != 0) {
        *fallback = current_routes.fallback;
    }
}

RUNTIME_ALLOCATOR_RECOVERED
RuntimeAllocatorRoute *runtime_allocator_select_route(bool primary) {
    RuntimeAllocatorRoute *selected = 0;
    runtime_allocator_copy_routes(
        primary ? &selected : 0,
        primary ? 0 : &selected
    );
    return selected;
}

RUNTIME_ALLOCATOR_RECOVERED
RuntimeAllocatorRoute *runtime_allocator_select_available_route(void) {
    RuntimeAllocatorRoute *primary = 0;
    RuntimeAllocatorRoute *fallback = 0;
    runtime_allocator_copy_routes(&primary, &fallback);
    return primary != 0 ? primary : fallback;
}

RUNTIME_ALLOCATOR_RECOVERED
void runtime_allocator_route_initialize(
    RuntimeAllocatorRoute *route,
    uintptr_t owned_lower_bound,
    uintptr_t owned_upper_bound
) {
    if (route == 0 || owned_lower_bound > owned_upper_bound) {
        runtime_firebloom_panic();
    }
    runtime_bzero(route, sizeof(*route));
    route->owned_lower_bound = owned_lower_bound;
    route->owned_upper_bound = owned_upper_bound;
}

RUNTIME_ALLOCATOR_RECOVERED
void runtime_allocator_route_track(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorNode *node,
    void *allocation,
    size_t size,
    void *free_range_owner
) {
    const uintptr_t lower = address_of(allocation);
    if (route == 0 || node == 0 || allocation == 0 || size == 0 ||
        UINTPTR_MAX - lower < size ||
        find_allocation(route, lower) != 0) {
        runtime_firebloom_panic();
    }
    node->next = route->allocations;
    node->free_range_owner = free_range_owner;
    node->lower_bound = lower;
    node->upper_bound = lower + size;
    route->allocations = node;
}

RUNTIME_ALLOCATOR_RECOVERED
bool runtime_allocator_route_unlink_allocation(
    RuntimeAllocatorRoute *route,
    void *allocation,
    RuntimeAllocatorUnlinkedAllocation *result
) {
    if (route == 0 || allocation == 0 || result == 0) {
        return false;
    }
    const uintptr_t address = address_of(allocation);
    RuntimeAllocatorNode **link = find_allocation_link(route, address);
    RuntimeAllocatorNode *node = *link;
    if (node == 0) {
        return false;
    }
    if (node->upper_bound < node->lower_bound) {
        runtime_firebloom_panic();
    }
    *link = node->next;
    node->next = 0;
    *result = (RuntimeAllocatorUnlinkedAllocation){
        .route = route,
        .free_range_owner = node->free_range_owner,
        .node = node,
        .allocation = allocation,
    };
    return true;
}

RUNTIME_ALLOCATOR_RECOVERED
void runtime_allocator_recycle_unlinked_allocation(
    RuntimeAllocatorUnlinkedAllocation *allocation
) {
    if (allocation == 0 || allocation->route == 0 || allocation->node == 0 ||
        allocation->allocation == 0 ||
        allocation->node->upper_bound < allocation->node->lower_bound) {
        runtime_firebloom_panic();
    }
    RuntimeAllocatorRoute *route = allocation->route;
    RuntimeAllocatorNode *node = allocation->node;
    runtime_bzero(
        allocation->allocation,
        node->upper_bound - node->lower_bound
    );
    node->next = route->recycled_nodes;
    route->recycled_nodes = node;
    *allocation = (RuntimeAllocatorUnlinkedAllocation){0};
}

RUNTIME_ALLOCATOR_RECOVERED
bool runtime_allocator_route_release(
    RuntimeAllocatorRoute *route,
    void *allocation
) {
    if (allocation == 0) {
        return true;
    }
    if (route == 0) {
        return false;
    }
    if (route_owns_static_address(route, address_of(allocation))) {
        return true;
    }
    RuntimeAllocatorUnlinkedAllocation unlinked;
    if (!runtime_allocator_route_unlink_allocation(
            route, allocation, &unlinked)) {
        return false;
    }
    runtime_allocator_recycle_unlinked_allocation(&unlinked);
    return true;
}

RUNTIME_ALLOCATOR_RECOVERED
size_t runtime_allocator_route_allocation_size(
    const RuntimeAllocatorRoute *route,
    const void *allocation
) {
    if (route == 0 || allocation == 0) {
        return 0;
    }
    const RuntimeAllocatorNode *node = find_allocation(
        route,
        address_of(allocation)
    );
    return node == 0 ? 0 : node->upper_bound - node->lower_bound;
}

RUNTIME_ALLOCATOR_RECOVERED
void runtime_allocator_release_address(void *allocation) {
    if (allocation == 0) {
        return;
    }
    RuntimeAllocatorRoute *primary = 0;
    RuntimeAllocatorRoute *fallback = 0;
    runtime_allocator_copy_routes(&primary, &fallback);
    if ((primary != 0 &&
         runtime_allocator_route_release(primary, allocation)) ||
        (fallback != 0 &&
         runtime_allocator_route_release(fallback, allocation))) {
        return;
    }
    runtime_firebloom_panic();
}

void runtime_allocator_publish_mapping_profile(
    RuntimeAllocatorMappingProfile profile
) {
    current_mapping_profile = profile;
}

static size_t allocation_size(const void *allocation) {
    RuntimeAllocatorRoute *primary = 0;
    RuntimeAllocatorRoute *fallback = 0;
    runtime_allocator_copy_routes(&primary, &fallback);
    size_t size = runtime_allocator_route_allocation_size(
        primary,
        allocation
    );
    if (size == 0) {
        size = runtime_allocator_route_allocation_size(
            fallback,
            allocation
        );
    }
    return size;
}

RUNTIME_ALLOCATOR_RECOVERED
void runtime_allocator_release_bounded(RuntimeFirebloomSpan allocation) {
    const uintptr_t address = address_of(allocation.current);
    const uintptr_t lower = address_of(allocation.lower_bound);
    const uintptr_t upper = address_of(allocation.upper_bound);
    if (!address_is_canonical(address)) {
        runtime_firebloom_panic();
    }
    if (address != 0 && address != lower &&
        (address < lower || address >= upper)) {
        runtime_firebloom_panic();
    }
    const RuntimeAllocatorMappingProfile profile = current_mapping_profile;
    if (address != 0 &&
        (address & (RUNTIME_ALLOCATOR_PAGE_SIZE - 1)) == 0 &&
        profile.unmap != 0 &&
        profile.lower_bound < address && address < profile.upper_bound &&
        (profile.forbidden_mask & address) == 0) {
        const size_t size = allocation_size(allocation.current);
        if (size == 0 ||
            (size & (RUNTIME_ALLOCATOR_PAGE_SIZE - 1)) != 0) {
            runtime_firebloom_panic();
        }
        profile.unmap(address, size);
    }
    runtime_allocator_release_address(allocation.current);
}
