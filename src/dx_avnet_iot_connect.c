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

#include "dx_avnet_iot_connect.h"

// This file implements the logic required to connect and interface with Avnet's IoTConnect cloud platform
// https://www.avnet.com/wps/portal/us/solutions/iot/software/iot-platform/

// Variables used to track IoTConnect connections details
static char dtgGUID[DX_AVNET_IOT_CONNECT_GUID_LEN + 1];
static char sidString[DX_AVNET_IOT_CONNECT_SID_LEN + 1];
static bool avnetConnected = false;

// static EventLoopTimer *IoTCTimer = NULL;
static const int AVNET_IOT_DEFAULT_POLL_PERIOD_SECONDS = 15; // Wait for 15 seconds for IoT Connect to send first response

// Forward function declarations
static void MonitorAvnetConnectionHandler(EventLoopTimer *timer);
static void AvnetSendHelloTelemetry(void);
static IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE, void *);
static const char *ErrorCodeToString(int iotConnectErrorCode);

static DX_TIMER_BINDING monitorAvnetConnectionTimer = {.name = "monitorAvnetConnectionTimer", .handler = MonitorAvnetConnectionHandler};

static void AvnetReconnectCallback(bool connected) {
    // Since we're going to be connecting or re-connecting to Azure
    // Set the IoT Connected flag to false
    avnetConnected = false;

    // Send the IoT Connect hello message to inform the platform that we're on-line!  We expect
    // to receive a hello response C2D message with connection details we need to send telemetry
    // data.
    AvnetSendHelloTelemetry();

    // Start the timer to make sure we see the IoT Connect "first response"
    // const struct timespec IoTCHelloPeriod = {.tv_sec = AVNET_IOT_DEFAULT_POLL_PERIOD_SECONDS, .tv_nsec = 0};
    // SetEventLoopTimerPeriod(IoTCTimer, &IoTCHelloPeriod);
    dx_timerChange(&monitorAvnetConnectionTimer, &(struct timespec){.tv_sec = AVNET_IOT_DEFAULT_POLL_PERIOD_SECONDS, .tv_nsec = 0});
}


// Call from the main init function to setup periodic timer and handler
void dx_avnetConnect(DX_USER_CONFIG *userConfig, const char *networkInterface)
{
    // Create the timer to monitor the IoTConnect hello response status
    if (!dx_timerStart(&monitorAvnetConnectionTimer)) {
        dx_terminate(DX_ExitCode_Init_IoTCTimer);
    }

    // Register to receive updates when the application receives an Azure IoTHub connection update
    // and C2D messages
    dx_azureRegisterConnectionChangedNotification(AvnetReconnectCallback);
    dx_azureRegisterMessageReceivedNotification(ReceiveMessageCallback);
    
    dx_azureConnect(userConfig, networkInterface, NULL);
}

/// <summary>
/// IoTConnect timer event:  Check for response status and send hello message
/// </summary>
static void MonitorAvnetConnectionHandler(EventLoopTimer *timer)
{
    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        dx_terminate(DX_ExitCode_IoTCTimer_Consume);
        return;
    }

    // If we're not connected to IoTConnect, then fall through to re-send the hello message
    if (!avnetConnected) {

        if (dx_isAzureConnected()) {
            AvnetSendHelloTelemetry();
        }
    }
}

/// <summary>
///     Callback function invoked when a C2D message is received from IoT Hub.
/// </summary>
/// <param name="message">The handle of the received message</param>
/// <param name="context">The user context specified at IoTHubDeviceClient_LL_SetMessageCallback()
/// invocation time</param>
/// <returns>Return value to indicates the message procession status (i.e. accepted, rejected,
/// abandoned)</returns>
static IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *context)
{
    Log_Debug("[AVT IoTConnect] Received C2D message\n");

    // Use a flag to track if we rx the dtg value
    bool dtgFlag = false;

    const unsigned char *buffer = NULL;
    size_t size = 0;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK) {
        Log_Debug("[AVT IoTConnect] Failure performing IoTHubMessage_GetByteArray\n");
        return IOTHUBMESSAGE_REJECTED;
    }

    // 'buffer' is not null terminated, make a copy and null terminate it.
    unsigned char *str_msg = (unsigned char *)malloc(size + 1);
    if (str_msg == NULL) {
        Log_Debug("[AVT IoTConnect] Could not allocate buffer for incoming message\n");
        abort();
    }

    memcpy(str_msg, buffer, size);
    str_msg[size] = '\0';

    Log_Debug("[AVT IoTConnect] Received message '%s' from IoT Hub\n", str_msg);

    // Process the message.  We're expecting a specific JSON structure from IoT Connect
    //{
    //    "d": {
    //        "ec": 0,
    //        "ct": 200,
    //        "sid": "NDA5ZTMyMTcyNGMyNGExYWIzMTZhYzE0NTI2MTFjYTU=UTE6MTQ6MDMuMDA=",
    //        "meta": {
    //            "g": "0ac9b336-f3e7-4433-9f4e-67668117f2ec",
    //            "dtg": "9320fa22-ae64-473d-b6ca-aff78da082ed",
    //            "edge": 0,
    //            "gtw": "",
    //            "at": 1,
    //            "eg": "bdcaebec-d5f8-42a7-8391-b453ec230731"
    //        },
    //        "has": {
    //            "d": 0,
    //            "attr": 1,
    //            "set": 0,
    //            "r": 0,
    //            "ota": 0
    //        }
    //    }
    //}
    //
    // The code below will drill into the structure and pull out each piece of data and store it
    // into variables

    // Using the mesage string get a pointer to the rootMessage
    JSON_Value *rootMessage = NULL;
    rootMessage = json_parse_string(str_msg);
    if (rootMessage == NULL) {
        Log_Debug("[AVT IoTConnect] Cannot parse the string as JSON content.\n");
        goto cleanup;
    }

    // Using the rootMessage pointer get a pointer to the rootObject
    JSON_Object *rootObject = json_value_get_object(rootMessage);

    // Using the root object get a pointer to the d object
    JSON_Object *dProperties = json_object_dotget_object(rootObject, "d");
    if (dProperties != NULL) {

        // The d properties should have a "ec" (error code) key
        if (json_object_has_value(dProperties, "ec") != 0) {
            int ec = (int)json_object_dotget_number(dProperties, "ec");
            Log_Debug("[AVT IoTConnect] ec: %s\n", ErrorCodeToString(ec));
        }

        // The d properties should have a "sid" key
        if (json_object_has_value(dProperties, "sid") != 0) {
            strncpy(sidString, (char *)json_object_get_string(dProperties, "sid"), DX_AVNET_IOT_CONNECT_SID_LEN);
            Log_Debug("[AVT IoTConnect] sid: %s\n", sidString);
        }

        // The object should contain a "meta" object
        JSON_Object *metaProperties = json_object_dotget_object(dProperties, "meta");
        if (metaProperties != NULL) {

            // The meta object should have a "dtg" key
            if (json_object_has_value(metaProperties, "dtg") != 0) {
                strncpy(dtgGUID, (char *)json_object_get_string(metaProperties, "dtg"), DX_AVNET_IOT_CONNECT_GUID_LEN);
                dtgFlag = true;
                Log_Debug("[AVT IoTConnect] dtg: %s\n", dtgGUID);
            }
        }

        // Check to see if we received all the required data we need to interact with IoTConnect
        if (dtgFlag) {

            // Verify that the new dtg is a valid GUID, if not then we just received an empty dtg.
            if (DX_AVNET_IOT_CONNECT_GUID_LEN == strnlen(dtgGUID, DX_AVNET_IOT_CONNECT_GUID_LEN + 1)) {

                // Set the IoTConnect Connected flag to true
                avnetConnected = true;
                Log_Debug("[AVT IoTConnect] IoTCConnected\n");
            }
        } else {

            // Set the IoTConnect Connected flag to false
            avnetConnected = false;
            Log_Debug("[AVT IoTConnect] Did not receive all the required data from IoTConnect\n");
            Log_Debug("[AVT IoTConnect] !IoTCConnected\n");
        }
    }

cleanup:
    // Release the allocated memory.
    json_value_free(rootMessage);
    free(str_msg);

    return IOTHUBMESSAGE_ACCEPTED;
}

static void AvnetSendHelloTelemetry(void)
{

    // Send the IoT Connect hello message to inform the platform that we're on-line!
    JSON_Value *rootValue = json_value_init_object();
    JSON_Object *rootObject = json_value_get_object(rootValue);

    json_object_dotset_number(rootObject, "mt", 200);
    json_object_dotset_number(rootObject, "v", 2.0F);

    char *serializedTelemetryUpload = json_serialize_to_string(rootValue);

    if (!dx_azurePublish(serializedTelemetryUpload, strnlen(serializedTelemetryUpload, 64), NULL, 0, NULL)) {

        Log_Debug("[AVT IoTConnect] IoTCHello message send error: %s\n", "error");
    }

    Log_Debug("[AVT IoTConnect] TX: %s\n", serializedTelemetryUpload);
    json_free_serialized_string(serializedTelemetryUpload);
    json_value_free(rootValue);
}


// Construct a new message that contains all the required IoTConnect data and the original telemetry
// message. Returns false if we have not received the first response from IoTConnect, if the
// target buffer is not large enough, or if the incoming data is not valid JSON.
bool dx_avnetJsonSerializePayload(const char *originalJsonMessage, char *modifiedJsonMessage, size_t modifiedBufferSize)
{

    bool result = false;

    // Verify that the incomming JSON is valid
    JSON_Value *rootProperties = NULL;
    rootProperties = json_parse_string(originalJsonMessage);
    if (rootProperties != NULL) {

        // Define the Json string format for sending telemetry to IoT Connect, note that the
        // actual telemetry data is inserted as the last string argument
        static const char IoTCTelemetryJson[] = "{\"sid\":\"%s\",\"dtg\":\"%s\",\"mt\":0,\"d\":[{\"d\":%s}]}";

        // Determine the largest message size needed.  We'll use this to validate the incoming target
        // buffer is large enough
        size_t maxModifiedMessageSize = strlen(originalJsonMessage) + DX_AVNET_IOT_CONNECT_METADATA;

        // Verify that the passed in buffer is large enough for the modified message
        if (maxModifiedMessageSize > modifiedBufferSize) {
            Log_Debug(
                "\n[AVT IoTConnect] "
                "ERROR: dx_avnetJsonSerializePayload() modified buffer size can't hold modified "
                "message\n");
            Log_Debug("                 Original message size: %d\n", strlen(originalJsonMessage));
            Log_Debug("Additional IoTConnect message overhead: %d\n", DX_AVNET_IOT_CONNECT_METADATA);
            Log_Debug("           Required target buffer size: %d\n", maxModifiedMessageSize);
            Log_Debug("            Actual target buffersize: %d\n\n", modifiedBufferSize);

            result = false;
            goto cleanup;
        }

        // Build up the IoTC message and insert the telemetry JSON
        snprintf(modifiedJsonMessage, maxModifiedMessageSize, IoTCTelemetryJson, sidString, dtgGUID, originalJsonMessage);
        result = true;

    }
    else{
        Log_Debug("[AVT IoTConnect] ERROR: dx_avnetJsonSerializePayload was passed invalid JSON\n");
    }

cleanup:
    // Release the allocated memory.
    json_value_free(rootProperties);

    return result;
}

bool dx_avnetJsonSerialize(char *jsonMessageBuffer, size_t bufferSize, int key_value_pair_count, ...)
{
    bool result = false;
    char *serializedJson = NULL;
    char *keyString = NULL;
    int dataType;

    // Prepare the JSON object for the telemetry data
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);

    // Create a Json_Array
    JSON_Value *myArrayValue = json_value_init_array();
    JSON_Array *myArray = json_value_get_array(myArrayValue);

    // Prepare the JSON object for the telemetry data
    JSON_Value *array_value_object = json_value_init_object();
    JSON_Object *array_object = json_value_get_object(array_value_object);

    // Allocate a buffer that we used to dynamically create the list key names d.<keyname>
    char *keyBuffer = (char *)malloc(DX_AVNET_IOT_CONNECT_JSON_BUFFER_SIZE);
    if (keyBuffer == NULL) {
        Log_Debug("[AVT IoTConnect] ERROR: not enough memory to send telemetry.");
        return false;
    }

    // We need to format the data as shown below
    // "{\"sid\":\"%s\",\"dtg\":\"%s\",\"mt\": 0,\"dt\": \"%s\",\"d\":[{\"d\":<new telemetry "key": value pairs>}]}";

    serializedJson = NULL;
    json_object_dotset_string(root_object, "sid", sidString);
    json_object_dotset_string(root_object, "dtg", dtgGUID);
    json_object_dotset_number(root_object, "mt", 0);

    // Prepare the argument list
    va_list inputList;
    va_start(inputList, key_value_pair_count);

    // Consume the data in the argument list and build out the json
    while (key_value_pair_count--) {

        // Pull the data type from the list
        dataType = va_arg(inputList, int);

        // Pull the current "key"
        keyString = va_arg(inputList, char *);

        // "d.<newKey>: <value>"
        snprintf(keyBuffer, DX_AVNET_IOT_CONNECT_JSON_BUFFER_SIZE, "d.%s", keyString);
        switch (dataType) {

            // report current device twin data as reported properties to IoTHub
        case DX_JSON_BOOL:
            json_object_dotset_boolean(array_object, keyBuffer, va_arg(inputList, int));
            break;
        case DX_JSON_FLOAT:
        case DX_JSON_DOUBLE:
            json_object_dotset_number(array_object, keyBuffer, va_arg(inputList, double));
            break;
        case DX_JSON_INT:
            json_object_dotset_number(array_object, keyBuffer, va_arg(inputList, int));
            break;
        case DX_JSON_STRING:
            json_object_dotset_string(array_object, keyBuffer, va_arg(inputList, char *));
            break;
        default:
            result = false;
            goto cleanup;
        }
    }

    // Clean up the argument list
    va_end(inputList);

    // Add the telemetry data to the Json
    json_array_append_value(myArray, array_value_object);
    json_object_dotset_value(root_object, "d", myArrayValue);

    // Serialize the structure
    serializedJson = json_serialize_to_string(root_value);

    // Copy the new JSON into the buffer the calling routine passed in
    if (strlen(serializedJson) < bufferSize) {
        strncpy(jsonMessageBuffer, serializedJson, bufferSize);
        result = true;
    }

cleanup:
    // Clean up
    json_free_serialized_string(serializedJson);
    json_value_free(root_value);
    free(keyBuffer);

    return result;
}

bool dx_isAvnetConnected(void)
{
    return avnetConnected;
}

static const char *ErrorCodeToString(int iotConnectErrorCode)
{
    switch (iotConnectErrorCode) {

    case AVT_ERROR_CODE_OK:
        return "OK";
    case AVT_ERROR_CODE_DEV_NOT_REGISTERED:
        return "device not registered";
    case AVT_ERROR_CODE_DEV_AUTO_REGISTERED:
        return "Device auto registered";
    case AVT_ERROR_CODE_DEV_NOT_FOUND:
        return "device not Found";
    case AVT_ERROR_CODE_DEV_INACTIVE:
        return "Inactive device";
    case AVT_ERROR_CODE_OBJ_MOVED:
        return "object moved";
    case AVT_ERROR_CODE_CPID_NOT_FOUND:
        return "CPID not found";
    case AVT_ERROR_CODE_COMPANY_NOT_FOUND:
        return "Company not found";
    default:
        return "Unknown error code";
    }
}