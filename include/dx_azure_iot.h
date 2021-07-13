/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "azure_prov_client/prov_client_const.h"
#include "azure_prov_client/prov_device_ll_client.h"
#include "azure_prov_client/prov_security_factory.h"
#include "azure_prov_client/prov_transport.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#include "dx_config.h"
#include "dx_device_twins.h"
#include "dx_direct_methods.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include "iothubtransportmqtt.h"
#include <applibs/log.h>
#include <azure_prov_client/iothub_security_factory.h>
#include <azure_sphere_provisioning.h>
#include <errno.h>
#include <iothub_client_options.h>
#include <iothub_device_client_ll.h>
#include <iothubtransportmqtt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "iothub_client_core_common.h"

#ifndef IOT_HUB_POLL_TIME_SECONDS
#define IOT_HUB_POLL_TIME_SECONDS 0
#endif

#ifndef IOT_HUB_POLL_TIME_NANOSECONDS
#define IOT_HUB_POLL_TIME_NANOSECONDS 100000000
#endif

typedef struct DX_MESSAGE_PROPERTY
{
	const char* key;
	const char* value;
} DX_MESSAGE_PROPERTY;

typedef struct DX_MESSAGE_CONTENT_PROPERTIES
{
	const char* contentEncoding;
	const char* contentType;
} DX_MESSAGE_CONTENT_PROPERTIES;

/// <summary>
/// Check if there is a network connection and an authenticated connection to Azure IoT Hub/Central
/// </summary>
/// <param name=""></param>
/// <returns></returns>
bool dx_isAzureConnected(void);

/// <summary>
/// Send message to Azure IoT Hub/Central with application and content properties.
/// Application and content properties can be NULL if not required.
/// </summary>
/// <param name="msg"></param>
/// <param name="messageProperties"></param>
/// <param name="messagePropertyCount"></param>
/// <param name="messageContentProperties"></param>
/// <returns></returns>
bool dx_azurePublish(const void* message, size_t messageLength, DX_MESSAGE_PROPERTY** messageProperties, size_t messagePropertyCount, DX_MESSAGE_CONTENT_PROPERTIES* messageContentProperties);

/// <summary>
/// Exposed for Device Twins. Not for general use.
/// </summary>
/// <param name=""></param>
/// <returns></returns>
IOTHUB_DEVICE_CLIENT_LL_HANDLE dx_azureClientHandleGet(void);

/// <summary>
/// Initialise Azure IoT Hub/Connection connection, passing in network interface for connecting testing and IoT Plug and Play model id.
/// Cloud to device messages is also enabled. For information on Plug and Play see https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
/// </summary>
/// <param name="dx_config"></param>
/// <param name="networkInterface"></param>
/// <param name="plugAndPlayModelId"></param>
void dx_azureConnect(DX_USER_CONFIG* userConfig, const char* networkInterface, const char* plugAndPlayModelId);

/// <summary>
/// Stop Cloud to device messaging. Device twins and direct method messages will not be recieved or processed.
/// </summary>
/// <param name=""></param>
void dx_azureToDeviceStop(void);
