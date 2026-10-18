#include "avp/runtime/ipc/runtime_endpoint_recovered.h"

#include <string.h>

static void runtime_endpoint_queue_append(
    RuntimeEndpointRequest **head,
    RuntimeEndpointRequest **tail,
    RuntimeEndpointRequest *request
) {
    request->next = NULL;
    if (*tail != NULL) {
        (*tail)->next = request;
    } else {
        *head = request;
    }
    *tail = request;
}

static RuntimeEndpointRequest *runtime_endpoint_queue_take(
    RuntimeEndpointRequest **head,
    RuntimeEndpointRequest **tail
) {
    RuntimeEndpointRequest *request = *head;
    if (request == NULL) {
        return NULL;
    }
    *head = request->next;
    if (*head == NULL) {
        *tail = NULL;
    }
    request->next = NULL;
    return request;
}

static void runtime_endpoint_wake_request(
    RuntimeEndpoint *endpoint,
    RuntimeEndpointRequest *request,
    RuntimeEndpointStatus status
) {
    request->state = RUNTIME_ENDPOINT_REQUEST_COMPLETE;
    if (request->owner != NULL) {
        request->owner->frame.x[0] = status;
        request->owner->state = RUNTIME_TASK_STATE_READY;
        if (endpoint->wake != NULL) {
            endpoint->wake(endpoint->wake_context, request->owner, status);
        }
    }
}

RuntimeEndpointStatus runtime_endpoint_create(
    RuntimeEndpoint *endpoint,
    RuntimeEndpointMessage *storage,
    size_t capacity,
    RuntimeEndpointWake wake,
    void *wake_context
) {
    if (endpoint == NULL || storage == NULL || capacity == 0u) {
        return RUNTIME_ENDPOINT_INVALID;
    }
    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->storage = storage;
    endpoint->capacity = capacity;
    endpoint->wake = wake;
    endpoint->wake_context = wake_context;
    return RUNTIME_ENDPOINT_OK;
}

RuntimeEndpointRequest *runtime_task_endpoint_request_acquire(
    RuntimeEndpoint *endpoint,
    RuntimeTask *task
) {
    if (endpoint == NULL || task == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < RUNTIME_ENDPOINT_REQUEST_LIMIT; ++index) {
        RuntimeEndpointRequest *request = &endpoint->requests[index];
        if (request->owner == task && request->state != RUNTIME_ENDPOINT_REQUEST_FREE) {
            if (request->state == RUNTIME_ENDPOINT_REQUEST_COMPLETE) {
                *request = (RuntimeEndpointRequest){.owner = task};
            }
            return request;
        }
    }
    for (size_t index = 0; index < RUNTIME_ENDPOINT_REQUEST_LIMIT; ++index) {
        RuntimeEndpointRequest *request = &endpoint->requests[index];
        if (request->state == RUNTIME_ENDPOINT_REQUEST_FREE) {
            *request = (RuntimeEndpointRequest){.owner = task};
            return request;
        }
    }
    return NULL;
}

bool runtime_endpoint_validate_pending_request(
    const RuntimeEndpoint *endpoint,
    const RuntimeEndpointRequest *request,
    const RuntimeTask *owner,
    RuntimeEndpointRequestState state
) {
    if (endpoint == NULL || request == NULL || owner == NULL ||
        request < &endpoint->requests[0] ||
        request >= &endpoint->requests[RUNTIME_ENDPOINT_REQUEST_LIMIT]) {
        return false;
    }
    return request->owner == owner && request->state == state &&
           request->message.count <= RUNTIME_ENDPOINT_VALUE_LIMIT;
}

bool runtime_endpoint_outlined_prepare_blocked_request_validation(
    const RuntimeEndpoint *endpoint,
    const RuntimeEndpointRequest *request,
    const RuntimeTask *owner,
    RuntimeEndpointRequestState state
) {
    return runtime_endpoint_validate_pending_request(
        endpoint,
        request,
        owner,
        state
    );
}

RuntimeEndpointRequestBounds
runtime_endpoint_outlined_load_task_pending_request_bounds(
    RuntimeEndpoint *endpoint,
    RuntimeEndpointRequest *request
) {
    if (endpoint == NULL) {
        return (RuntimeEndpointRequestBounds){0};
    }
    return (RuntimeEndpointRequestBounds){
        .current = request,
        .lower_bound = &endpoint->requests[0],
        .upper_bound = &endpoint->requests[RUNTIME_ENDPOINT_REQUEST_LIMIT],
    };
}

RuntimeEndpointStatus runtime_endpoint_complete_send_request(
    RuntimeEndpoint *endpoint,
    RuntimeEndpointRequest *request
) {
    if (endpoint == NULL || request == NULL ||
        !runtime_endpoint_validate_pending_request(
            endpoint, request, request->owner, RUNTIME_ENDPOINT_REQUEST_SEND)) {
        return RUNTIME_ENDPOINT_INVALID;
    }
    RuntimeEndpointRequest *receiver = runtime_endpoint_queue_take(
        &endpoint->receive_head,
        &endpoint->receive_tail
    );
    if (receiver != NULL) {
        receiver->message = request->message;
        runtime_endpoint_wake_request(endpoint, receiver, RUNTIME_ENDPOINT_OK);
        runtime_endpoint_wake_request(endpoint, request, RUNTIME_ENDPOINT_OK);
        return RUNTIME_ENDPOINT_OK;
    }
    if (endpoint->count == endpoint->capacity) {
        return RUNTIME_ENDPOINT_FULL;
    }
    endpoint->storage[endpoint->write_index] = request->message;
    endpoint->write_index = (endpoint->write_index + 1u) % endpoint->capacity;
    ++endpoint->count;
    runtime_endpoint_wake_request(endpoint, request, RUNTIME_ENDPOINT_OK);
    return RUNTIME_ENDPOINT_OK;
}

RuntimeEndpointStatus runtime_endpoint_complete_receive_request(
    RuntimeEndpoint *endpoint,
    RuntimeEndpointRequest *request
) {
    if (endpoint == NULL || request == NULL ||
        !runtime_endpoint_validate_pending_request(
            endpoint, request, request->owner, RUNTIME_ENDPOINT_REQUEST_RECEIVE)) {
        return RUNTIME_ENDPOINT_INVALID;
    }
    if (endpoint->count == 0u) {
        return RUNTIME_ENDPOINT_EMPTY;
    }
    request->message = endpoint->storage[endpoint->read_index];
    endpoint->read_index = (endpoint->read_index + 1u) % endpoint->capacity;
    --endpoint->count;
    runtime_endpoint_wake_request(endpoint, request, RUNTIME_ENDPOINT_OK);

    RuntimeEndpointRequest *sender = runtime_endpoint_queue_take(
        &endpoint->send_head,
        &endpoint->send_tail
    );
    if (sender != NULL) {
        (void)runtime_endpoint_complete_send_request(endpoint, sender);
    }
    return RUNTIME_ENDPOINT_OK;
}

RuntimeEndpointStatus runtime_endpoint_send(
    RuntimeEndpoint *endpoint,
    RuntimeTask *task,
    const uint64_t *values,
    size_t count,
    uint64_t deadline
) {
    if (endpoint == NULL || task == NULL || values == NULL || count == 0u ||
        count > RUNTIME_ENDPOINT_VALUE_LIMIT) {
        return RUNTIME_ENDPOINT_INVALID;
    }
    RuntimeEndpointRequest *request =
        runtime_task_endpoint_request_acquire(endpoint, task);
    if (request == NULL || request->state != RUNTIME_ENDPOINT_REQUEST_FREE) {
        return RUNTIME_ENDPOINT_INVALID;
    }
    request->state = RUNTIME_ENDPOINT_REQUEST_SEND;
    request->deadline = deadline;
    request->message.count = count;
    memcpy(request->message.values, values, count * sizeof(values[0]));
    const RuntimeEndpointStatus status =
        runtime_endpoint_complete_send_request(endpoint, request);
    if (status != RUNTIME_ENDPOINT_FULL) {
        return status;
    }
    if (deadline == 0u) {
        request->state = RUNTIME_ENDPOINT_REQUEST_COMPLETE;
        return RUNTIME_ENDPOINT_TIMEOUT;
    }
    task->state = RUNTIME_TASK_STATE_IPC_WAIT;
    runtime_endpoint_queue_append(
        &endpoint->send_head,
        &endpoint->send_tail,
        request
    );
    return RUNTIME_ENDPOINT_BLOCKED;
}

RuntimeEndpointStatus runtime_endpoint_receive(
    RuntimeEndpoint *endpoint,
    RuntimeTask *task,
    uint64_t *values,
    size_t capacity,
    size_t *received_count,
    uint64_t deadline
) {
    if (endpoint == NULL || task == NULL || values == NULL ||
        received_count == NULL || capacity == 0u) {
        return RUNTIME_ENDPOINT_INVALID;
    }
    RuntimeEndpointRequest *request =
        runtime_task_endpoint_request_acquire(endpoint, task);
    if (request == NULL || request->state != RUNTIME_ENDPOINT_REQUEST_FREE) {
        return RUNTIME_ENDPOINT_INVALID;
    }
    request->state = RUNTIME_ENDPOINT_REQUEST_RECEIVE;
    request->deadline = deadline;
    const RuntimeEndpointStatus status =
        runtime_endpoint_complete_receive_request(endpoint, request);
    if (status == RUNTIME_ENDPOINT_OK) {
        if (request->message.count > capacity) {
            return RUNTIME_ENDPOINT_INVALID;
        }
        memcpy(
            values,
            request->message.values,
            request->message.count * sizeof(values[0])
        );
        *received_count = request->message.count;
        return RUNTIME_ENDPOINT_OK;
    }
    if (status != RUNTIME_ENDPOINT_EMPTY) {
        return status;
    }
    if (deadline == 0u) {
        request->state = RUNTIME_ENDPOINT_REQUEST_COMPLETE;
        return RUNTIME_ENDPOINT_TIMEOUT;
    }
    task->state = RUNTIME_TASK_STATE_IPC_WAIT;
    runtime_endpoint_queue_append(
        &endpoint->receive_head,
        &endpoint->receive_tail,
        request
    );
    return RUNTIME_ENDPOINT_BLOCKED;
}
