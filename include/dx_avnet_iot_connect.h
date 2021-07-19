#pragma once

/*
MIT License
Copyright (c) Avnet Corporation. All rights reserved.
Author: Brian Willess
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <applibs/log.h>

#include "applibs_versions.h"
#include "iothub_device_client_ll.h"
#include "parson.h"

#include "dx_exit_codes.h"
#include "dx_azure_iot.h"
#include "dx_timer.h"
#include "dx_json_serializer.h" // for DX_JSON_TYPE enum
#include "dx_utilities.h"
#include "dx_azure_iot.h"
#include "dx_config.h"

#define DX_AVNET_IOT_CONNECT_GUID_LEN 36
#define DX_AVNET_IOT_CONNECT_SID_LEN 64
#define DX_AVNET_IOT_CONNECT_METADATA 256
#define DX_AVNET_IOT_CONNECT_JSON_BUFFER_SIZE 512

typedef enum
{
	AVT_ERROR_CODE_OK = 0,
	AVT_ERROR_CODE_DEV_NOT_REGISTERED = 1,
    AVT_ERROR_CODE_DEV_AUTO_REGISTERED = 2,
    AVT_ERROR_CODE_DEV_NOT_FOUND = 3,
    AVT_ERROR_CODE_DEV_INACTIVE = 4,
    AVT_ERROR_CODE_OBJ_MOVED = 5,
    AVT_ERROR_CODE_CPID_NOT_FOUND = 6,
    AVT_ERROR_CODE_COMPANY_NOT_FOUND = 7
} AVT_IOTC_ERROR_CODES;

/// <summary>
/// Takes properly formatted JSON telemetry data and wraps it with ToTConnect
/// metaData.  Returns false if the application has not received the IoTConnect hello
/// response, if the passed in buffer is too small for the modified JSON document, or
/// if the passed in JSON is malformed.
/// </summary>
/// <param name="originalJsonMessage"></param>
/// <param name="modifiedJsonMessage"></param>
/// <param name="modifiedBufferSize"></param>
/// <returns></returns>
bool dx_avnetJsonSerializePayload(const char *originalJsonMessage, char *modifiedJsonMessage, size_t modifiedBufferSize);

/// <summary>
/// Returns a JSON document containing passed in <type, key, value> triples.  The JSON document
/// will have the IoTConnect metadata header prepended to the telemetry data.
/// </summary>
/// <param name="jsonMessageBuffer"></param>
/// <param name="bufferSize"></param>
/// <param name="key_value_pair_count"></param>
/// <param name="(type, key, value) triples"></parm>
/// <returns></returns>
bool dx_avnetJsonSerialize(char * jsonMessageBuffer, size_t bufferSize, int key_value_pair_count, ...);

/// <summary>
/// Initializes the IoTConnect timer.  This routine should be called on application init
/// </summary>
/// <returns></returns>
void dx_avnetConnect(DX_USER_CONFIG *userConfig, const char *networkInterface);

/// <summary>
/// This routine returns the status of the IoTConnect connection.  
/// </summary>
/// <returns></returns>
bool dx_isAvnetConnected(void);