/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_azure_iot.h"
#include "dx_gpio.h"

#define DX_DIRECT_METHOD_HANDLER(name, json, directMethodBinding, responseMsg)                                         \
    DX_DIRECT_METHOD_RESPONSE_CODE name(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg) \
    {

#define DX_DIRECT_METHOD_HANDLER_END \
    }

#define DX_DECLARE_DIRECT_METHOD_HANDLER(name) \
    DX_DIRECT_METHOD_RESPONSE_CODE name(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);

typedef enum { DX_METHOD_SUCCEEDED = 200, DX_METHOD_FAILED = 500, DX_METHOD_NOT_FOUND = 404 } DX_DIRECT_METHOD_RESPONSE_CODE;

typedef struct _directMethodBinding {
    const char *methodName;
    DX_DIRECT_METHOD_RESPONSE_CODE (*handler)(JSON_Value *json, struct _directMethodBinding *peripheral, char **responseMsg);
    void *context;
} DX_DIRECT_METHOD_BINDING;

void dx_directMethodUnsubscribe(void);
void dx_directMethodSubscribe(DX_DIRECT_METHOD_BINDING *directMethods[], size_t directMethodCount);
