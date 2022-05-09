/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_terminate.h"
#include <pthread.h>
#include <stdbool.h>

extern volatile sig_atomic_t terminationRequired;

#define DX_ASYNC_HANDLER(name, handle)  \
    void name(DX_ASYNC_BINDING *handle) \
    {

#define DX_ASYNC_HANDLER_END }

#define DX_DECLARE_ASYNC_HANDLER(name) void name(DX_ASYNC_BINDING *handle)

typedef struct _asyncBinding {
    bool triggered;
    void *data;
    void (*handler)(struct _asyncBinding *handle);
} DX_ASYNC_BINDING;

void dx_asyncInit(DX_ASYNC_BINDING *async);
void dx_asyncSend(DX_ASYNC_BINDING *binding, void *data);
void dx_asyncSetInit(DX_ASYNC_BINDING *asyncSet[], size_t asyncCount);
void dx_asyncRunEvents(void);