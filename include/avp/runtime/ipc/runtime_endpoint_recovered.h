#pragma once

#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__ELF__) && __has_attribute(retain)
#define RUNTIME_ENDPOINT_RECOVERED_PUBLIC \
    __attribute__((used, retain, noinline, section(".text.runtime_support_recovered")))
#elif defined(__ELF__)
#define RUNTIME_ENDPOINT_RECOVERED_PUBLIC \
    __attribute__((used, noinline, section(".text.runtime_support_recovered")))
#else
#define RUNTIME_ENDPOINT_RECOVERED_PUBLIC __attribute__((used, noinline))
#endif

enum {
    RUNTIME_ENDPOINT_VALUE_LIMIT = 4,
    RUNTIME_ENDPOINT_REQUEST_LIMIT = 16,
};

typedef enum {
    RUNTIME_ENDPOINT_OK = 0,
    RUNTIME_ENDPOINT_BLOCKED = 1,
    RUNTIME_ENDPOINT_TIMEOUT = 2,
    RUNTIME_ENDPOINT_INVALID = 3,
    RUNTIME_ENDPOINT_FULL = 4,
    RUNTIME_ENDPOINT_EMPTY = 5,
} RuntimeEndpointStatus;

typedef struct {
    uint64_t values[RUNTIME_ENDPOINT_VALUE_LIMIT];
    size_t count;
} RuntimeEndpointMessage;

typedef enum {
    RUNTIME_ENDPOINT_REQUEST_FREE = 0,
    RUNTIME_ENDPOINT_REQUEST_SEND = 1,
    RUNTIME_ENDPOINT_REQUEST_RECEIVE = 2,
    RUNTIME_ENDPOINT_REQUEST_COMPLETE = 3,
} RuntimeEndpointRequestState;

typedef struct RuntimeEndpointRequest {
    RuntimeTask *owner;
    RuntimeEndpointRequestState state;
    RuntimeEndpointMessage message;
    uint64_t deadline;
    struct RuntimeEndpointRequest *next;
} RuntimeEndpointRequest;

typedef void (*RuntimeEndpointWake)(void *context,
                                    RuntimeTask *task,
                                    RuntimeEndpointStatus status);

typedef struct {
    RuntimeEndpointMessage *storage;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t count;
    RuntimeEndpointRequest requests[RUNTIME_ENDPOINT_REQUEST_LIMIT];
    RuntimeEndpointRequest *send_head;
    RuntimeEndpointRequest *send_tail;
    RuntimeEndpointRequest *receive_head;
    RuntimeEndpointRequest *receive_tail;
    RuntimeEndpointWake wake;
    void *wake_context;
} RuntimeEndpoint;

typedef struct {
    RuntimeEndpointRequest *current;
    RuntimeEndpointRequest *lower_bound;
    RuntimeEndpointRequest *upper_bound;
} RuntimeEndpointRequestBounds;

RUNTIME_ENDPOINT_RECOVERED_PUBLIC RuntimeEndpointStatus
runtime_endpoint_create(RuntimeEndpoint *endpoint,
                        RuntimeEndpointMessage *storage,
                        size_t capacity,
                        RuntimeEndpointWake wake,
                        void *wake_context);
RUNTIME_ENDPOINT_RECOVERED_PUBLIC RuntimeEndpointStatus
runtime_endpoint_send(RuntimeEndpoint *endpoint,
                      RuntimeTask *task,
                      const uint64_t *values,
                      size_t count,
                      uint64_t deadline);
RUNTIME_ENDPOINT_RECOVERED_PUBLIC RuntimeEndpointRequest *
runtime_task_endpoint_request_acquire(RuntimeEndpoint *endpoint,
                                      RuntimeTask *task);
RUNTIME_ENDPOINT_RECOVERED_PUBLIC RuntimeEndpointStatus
runtime_endpoint_complete_send_request(RuntimeEndpoint *endpoint,
                                       RuntimeEndpointRequest *request);
RUNTIME_ENDPOINT_RECOVERED_PUBLIC bool
runtime_endpoint_validate_pending_request(const RuntimeEndpoint *endpoint,
                                          const RuntimeEndpointRequest *request,
                                          const RuntimeTask *owner,
                                          RuntimeEndpointRequestState state);
RUNTIME_ENDPOINT_RECOVERED_PUBLIC RuntimeEndpointStatus
runtime_endpoint_complete_receive_request(RuntimeEndpoint *endpoint,
                                          RuntimeEndpointRequest *request);
RUNTIME_ENDPOINT_RECOVERED_PUBLIC RuntimeEndpointStatus
runtime_endpoint_receive(RuntimeEndpoint *endpoint,
                         RuntimeTask *task,
                         uint64_t *values,
                         size_t capacity,
                         size_t *received_count,
                         uint64_t deadline);
RUNTIME_ENDPOINT_RECOVERED_PUBLIC RuntimeEndpointRequestBounds
runtime_endpoint_outlined_load_task_pending_request_bounds(
    RuntimeEndpoint *endpoint,
    RuntimeEndpointRequest *request);
RUNTIME_ENDPOINT_RECOVERED_PUBLIC bool
runtime_endpoint_outlined_prepare_blocked_request_validation(
    const RuntimeEndpoint *endpoint,
    const RuntimeEndpointRequest *request,
    const RuntimeTask *owner,
    RuntimeEndpointRequestState state);
