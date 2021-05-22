/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_azure_iot.h"
#include "parson.h"
#include "dx_gpio.h"
#include <iothub_device_client_ll.h>

typedef enum {
	DX_TYPE_UNKNOWN = 0,
	DX_TYPE_BOOL = 1,
	DX_TYPE_FLOAT = 2,
	DX_TYPE_DOUBLE = 3,
	DX_TYPE_INT = 4,
	DX_TYPE_STRING = 5
} DX_DEVICE_TWIN_TYPE;

struct _deviceTwinBinding {
	const char* twinProperty;
	void* twinState;
	int twinVersion;
	bool twinStateUpdated;
	DX_DEVICE_TWIN_TYPE twinType;
	void (*handler)(struct _deviceTwinBinding* deviceTwinBinding);
};

typedef enum
{
	DX_DEVICE_TWIN_COMPLETED = 200,
	DX_DEVICE_TWIN_ERROR = 500,
	DX_DEVICE_TWIN_INVALID = 404
} DX_DEVICE_TWIN_RESPONSE_CODE;

typedef struct _deviceTwinBinding DX_DEVICE_TWIN_BINDING;

/// <summary>
/// Acknowledge receipt of a device twin message with new state and status code.
/// </summary>
/// <param name="deviceTwinBinding"></param>
/// <param name="state"></param>
/// <param name="statusCode"></param>
/// <returns></returns>
bool dx_deviceTwinAckDesiredState(DX_DEVICE_TWIN_BINDING* deviceTwinBinding, void* state, DX_DEVICE_TWIN_RESPONSE_CODE statusCode);

/// <summary>
/// Update device twin state.
/// </summary>
/// <param name="deviceTwinBinding"></param>
/// <param name="state"></param>
/// <returns></returns>
bool dx_deviceTwinReportState(DX_DEVICE_TWIN_BINDING* deviceTwinBinding, void* state);

/// <summary>
/// Close all device twins, deallocate backing storage for each twin, and stop inbound and outbound device twin updates.
/// </summary>
/// <param name=""></param>
void dx_deviceTwinUnsubscribe(void);

/// <summary>
/// Open device twins and start processing of device twins.
/// </summary>
/// <param name="deviceTwins"></param>
/// <param name="deviceTwinCount"></param>
void dx_deviceTwinSubscribe(DX_DEVICE_TWIN_BINDING* deviceTwins[], size_t deviceTwinCount);

/// <summary>
/// Callback used by IoT Hub client to process inbound device twin messages.
/// </summary>
void dx__deviceTwinCallbackHandler(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char* payload, size_t payloadSize, void* userContextCallback);