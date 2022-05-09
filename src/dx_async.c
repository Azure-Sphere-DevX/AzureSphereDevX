/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "dx_async.h"

static DX_ASYNC_BINDING **_asyncSet = NULL;
static size_t _asyncCount = 0;
volatile bool asyncEventReady = false;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void dx_asyncInit(DX_ASYNC_BINDING *binding)
{
    binding->triggered = false;
    binding->data = NULL;
}

void dx_asyncSend(DX_ASYNC_BINDING *binding, void *data)
{
    pthread_mutex_lock(&mutex);
    binding->data = data;
    binding->triggered = true;
    asyncEventReady = true;
    pthread_mutex_unlock(&mutex);
}

void dx_asyncSetInit(DX_ASYNC_BINDING *asyncSet[], size_t asyncCount)
{
    if (asyncCount < 0) {
        return;
    }

    _asyncCount = asyncCount;
    _asyncSet = asyncSet;

    for (int i = 0; i < asyncCount; i++) {
        dx_asyncInit(asyncSet[i]);
    }
}

void dx_asyncRunEvents(void)
{
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < _asyncCount; i++) {
        if (_asyncSet[i]->triggered && _asyncSet[i]->handler) {
            _asyncSet[i]->handler(_asyncSet[i]);
            _asyncSet[i]->triggered = false;
        }
    }

    asyncEventReady = false;
    pthread_mutex_unlock(&mutex);
}