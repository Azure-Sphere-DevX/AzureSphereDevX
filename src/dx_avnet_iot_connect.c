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

// IoTConnect implementation variables
static bool _avnetConnected = false;
static avt_iotc_1_0_t _avt_1_0_properties;
static avt_iotc_2_1_t _avt_2_1_properties;
static avt_iotc_has_t _avt_has_properties;
static avt_iotc_api_ver_t _api_version = AVT_API_VERSION_2_1;

static DX_MESSAGE_PROPERTY *_telemetryMsgProps[AVT_TELEMETRY_PROP_COUNT];
static DX_MESSAGE_PROPERTY *_devIdMsgProps[AVT_DEV_ID_PROP_COUNT];
static DX_MESSAGE_CONTENT_PROPERTIES _ioTCContentProperties = {.contentEncoding = "utf-8", .contentType = "application/json"};

// char arrays for custom application message properties
char _vKey[] = "v";
char _vValue[DX_AVNET_IOT_CONNECT_MAX_MSG_PROPERTY_LEN] = {'\0'};
char _mtKey[] = "mt";
char _mtValue[] = "0";
char _cdKey[] = "cd";
char _cdValue[DX_AVNET_IOT_CONNECT_MAX_MSG_PROPERTY_LEN] = {'\0'};
char _diKey[] = "di";
char _diValue[] = "1";

// Define a pointer to a linked list of children devices/nodes for gateway implementations
gw_child_list_node_t* _gwChildrenListHead = NULL;

// Variables to support the avt_Debug() functionality
uint8_t _avt_debugLevel = AVT_DEBUG_LEVEL_ERROR;
#define AVT_DEBUG_LEVEL_BUFFER_SIZE 512
static char _avt_log_debug_buffer[AVT_DEBUG_LEVEL_BUFFER_SIZE] = {'\0'};
static size_t _avt_log_debug_buffer_size = AVT_DEBUG_LEVEL_BUFFER_SIZE;

// Wait for 15 seconds for IoT Connect to send the hello message
#define AVNET_IOT_DEFAULT_POLL_PERIOD_SECONDS 15

// Forward function declarations
static void MonitorAvnetConnectionHandler(EventLoopTimer *timer);
static void IoTCSend200HelloMessage(void);
static IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE, void *);
static const char *ErrorCodeToString(int iotConnectErrorCode);
static void IoTCrequestChildDeviceInfo(void);
static bool IoTCProcessDataFrequencyResponse(JSON_Object *dProperties);
static bool IoTCProcessHelloResponse(JSON_Object *dProperties);
static bool IoTCProcess204Response(JSON_Object *dProperties);
static void IoTCSend221AddChildMessage(const char* id, const char* tg, const char* displayName);
static bool IoTCProcess221Response(JSON_Object *dProperties);
static void IoTCSend222DeleteChildMessage(gw_child_list_node_t* childToDelete);
static bool IoTCProcess222Response(JSON_Object *dProperties);
static bool IoTCBuildCustomMessageProperties(void);
static void avt_Debug(uint8_t debugLevel, char *fmt, ...);

// Routines associated with gateway implementations and managing the linked list of children devices
bool IoTCListAddChild(const char* id, const char* tg);
void IoTClistDelete(void);
bool IoTCListDeleteNode(gw_child_list_node_t* nodeToRemove);
bool IoTCListDeleteNodeById(const char* id);
gw_child_list_node_t* IoTCListGetNewNode(const char* id, const char* tg);
gw_child_list_node_t* IoTCListInsertNode(const char* id, const char* tg);
gw_child_list_node_t* IoTCListFindNode(char* id);
gw_child_list_node_t* IoTCListFindNodeById(const char* id);

static DX_TIMER_BINDING monitorAvnetConnectionTimer = {.name = "monitorAvnetConnectionTimer", .handler = MonitorAvnetConnectionHandler};

static void AvnetReconnectCallback(bool connected) {
    // Since we're going to be connecting or re-connecting to Azure
    // Set the IoT Connected flag to false
    _avnetConnected = false;

    // Send the IoT Connect hello message to inform the platform that we're on-line!  We expect
    // to receive a hello response C2D message with connection details required to send telemetry
    // data.
    IoTCSend200HelloMessage();

    // Start the timer to make sure we see the IoT Connect "first response"
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
    if (!_avnetConnected) {

        if (dx_isAzureConnected()) {
            IoTCSend200HelloMessage();
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
    const unsigned char *buffer = NULL;
    size_t msgSize = 0;
    int ctVal = -1;

    if (IoTHubMessage_GetByteArray(message, &buffer, &msgSize) != IOTHUB_MESSAGE_OK) {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Failure performing IoTHubMessage_GetByteArray\n");
        return IOTHUBMESSAGE_REJECTED;
    }

    // 'buffer' is not null terminated, make a copy and null terminate it.
    unsigned char *str_msg = (unsigned char *)malloc(msgSize + 1);
    if (str_msg == NULL) {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Could not allocate buffer for incoming message\n");
        abort();
    }

    // Copy the buffer and null terminate it
    memcpy(str_msg, buffer, msgSize);
    str_msg[msgSize] = '\0';

    avt_Debug(AVT_DEBUG_LEVEL_INFO, "Received C2D message '%s'\n", str_msg);

    // Using the mesage string get a pointer to the rootMessage
    JSON_Value *rootMessage = NULL;
    rootMessage = json_parse_string(str_msg);
    if (rootMessage == NULL) {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Cannot parse the string as JSON content.\n");
        goto cleanup;
    }

    // Using the rootMessage pointer get a pointer to the rootObject
    JSON_Object *rootObject = json_value_get_object(rootMessage);

    // Using the root object get a pointer to the d object
    JSON_Object *dProperties = json_object_dotget_object(rootObject, "d");
    if (dProperties == NULL) {

        // Check if the root object contains a "ct" field that we can use to identify the response
        if (json_object_has_value(rootObject, "ct") != 0) {
            ctVal = (int)json_object_get_number(rootObject, "ct");

            switch(ctVal){
                case AVT_CT_DATA_FREQUENCY_CHANGE_105:
                    if(!IoTCProcessDataFrequencyResponse(rootObject)){
                        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Error processing IoTCProcessDataFrequencyResponse()\n");
                        goto cleanup;
                    }                   
                case AVT_CT_REFRESH_ATTRIBUTE_101:
                case AVT_CT_REFRESH_TWINS_102:
                case AVT_CT_REFRESH_EDGE_SETTINGS_103:
                case AVT_CT_REFRESH_CHILD_DEVICE_104:
                case AVT_CT_DEVICE_DELETED_106:
                case AVT_CT_DEVICE_DISABLED_107:
                case AVT_CT_DEVICE_RELEASED_108:
                case AVT_CT_STOP_OPERATION_109:
                case AVT_CT_START_HEARTBEAT_110:
                case AVT_CT_STOP_HEARTBEAT_111:
                    avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Message ID: %d handler not implemented!\n", ctVal);
                    break;
                default:
                    avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Error response code %d not handled\n", ctVal);
                    break;
            }
        }
    }
    else{ // There is a "d" object, drill into it and pull the data

        // The "d" object contains a "ct" field that we can use to identify the response
        if (json_object_has_value(dProperties, "ct") != 0) {
            ctVal = (int)json_object_get_number(dProperties, "ct");
            avt_Debug(AVT_DEBUG_LEVEL_INFO, "ct: %d\n", ctVal);

        }

        switch(ctVal){
            case AVT_CT_HELLO_200:
                if(!IoTCProcessHelloResponse(dProperties)){
                    avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Error processing IoTCProcessHelloResponse()\n");
                    goto cleanup;
                }                   
                break;
            case AVT_CT_CHILD_ATTRIBUTES_204:
                if(!IoTCProcess204Response(dProperties)){
                    avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Error processing IoTCProcess204Response()\n");
                    goto cleanup;
                }
                break;
            case AVT_CT_CREATE_GW_CHILD_RESPONSE_221: 
                if(!IoTCProcess221Response(dProperties)){
                    avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Error processing IoTCProcess221Response()\n");
                    goto cleanup;
                }
                break;
            case AVT_CT_DELETE_GW_CHILD_RESPONSE_222: 
                if(!IoTCProcess222Response(dProperties)){
                    avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Error processing IoTCProcess221Response()\n");
                    goto cleanup;
                }
                break;
            case AVT_CT_DEVICE_ATTRIBUTES_201:
            case AVT_CT_TWIN_SETTINGS_202:
            case AVT_CT_EDGE_RULES_203:
            case AVT_CT_OTA_ATTRIBUTES_205:
            case AVT_CT_ALL_DEVICE_INFO_210:
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "Message ID: %d handler not implemented!\n", ctVal);
                break;
            default:
                avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Error response code %d not handled\n", ctVal);
                break;
        }
    }

cleanup:
    // Release the allocated memory.
    json_value_free(rootMessage);
    free(str_msg);

    return IOTHUBMESSAGE_ACCEPTED;
}

static void IoTCSend200HelloMessage(void)
{

    DX_MESSAGE_PROPERTY *helloMessageProperties[] =  {&(DX_MESSAGE_PROPERTY){.key = "v", .value = "2.1"}, 
                                                        &(DX_MESSAGE_PROPERTY){.key = "di", .value = "1"}};

    char helloMessage_api_1[]   = "{\"mt\": 200, \"v\": 1}";
    char helloMessage_api_2_1[] = "{\"mt\": 200, \"sid\": \"\"}";
    char *selectedHelloMessage = NULL;

    if(_api_version == AVT_API_VERSION_1_0){
        selectedHelloMessage = helloMessage_api_1;

            // Send the 1.0 hello message
        if (!dx_azurePublish(selectedHelloMessage, strnlen(selectedHelloMessage, 42), NULL, 0, NULL)) {

            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "IoTCHello message send error: %s\n", "error");
            return;
        }
    }
    else if(_api_version == AVT_API_VERSION_2_1){
        selectedHelloMessage = helloMessage_api_2_1;

        // Send the hello message, always use the 2.1 message properties
        if (!dx_azurePublish(selectedHelloMessage, strnlen(selectedHelloMessage, 42), helloMessageProperties, 2, &_ioTCContentProperties)) {

            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "IoTCHello message send error: %s\n", "error");
            return;
        }        
    }
    else{
        // Terminate the application with a new exit code
    }
    avt_Debug(AVT_DEBUG_LEVEL_INFO, "TX: %s\n", selectedHelloMessage);
}

// Construct a new message that contains all the required IoTConnect data and the original telemetry
// message. Returns false if we have not received the first response from IoTConnect, if the
// target buffer is not large enough, or if the incoming data is not valid JSON.
bool dx_avnetJsonSerializePayload(const char *originalJsonMessage, char *modifiedJsonMessage, size_t modifiedBufferSize, gw_child_list_node_t* childDevice)
{

    bool result = true;

    size_t utcBufferSize = 32;
    char utcBuffer[utcBufferSize];

    // Verify that the incomming JSON is valid
    JSON_Value *rootProperties = NULL;
    rootProperties = json_parse_string(originalJsonMessage);
    if (rootProperties != NULL) {

        // Define the Json string format for sending telemetry to IoT Connect, note that the
        // actual telemetry data is inserted as the last string argument
        static const char IoTCTelemetryJson[] = "{\"sid\":\"%s\",\"dtg\":\"%s\",\"mt\":0,\"d\":[{\"d\":%s}]}";
        static const char IoTCTelemetryJson2_1[] = "{\"dt\":\"%s\",\"d\":[{\"dt\":\"%s\",\"d\":%s}]}";
        static const char IoTCGwTelemetryJson[] = "{\"sid\":\"%s\",\"dtg\":\"%s\",\"mt\":0,\"d\":[{\"id\":\"%s\",\"tg\":\"%s\",\"d\":%s}]}";
        static const char IoTCGwTelemetryJson2_1[] = "{\"dt\":\"%s\",\"d\":[{\"id\":\"%s\",\"tg\":\"%s\",\"dt\":\"%s\",\"d\":%s}]}";

        // Determine the largest message size needed.  We'll use this to validate the incoming target
        // buffer is large enough
        size_t maxModifiedMessageSize = strlen(originalJsonMessage) + DX_AVNET_IOT_CONNECT_METADATA;

        // Verify that the passed in buffer is large enough for the modified message
        if (maxModifiedMessageSize > modifiedBufferSize) {
            avt_Debug(AVT_DEBUG_LEVEL_ERROR,
                "\n[AVT IoTConnect] "
                "ERROR: dx_avnetJsonSerializePayload() modified buffer size can't hold modified "
                "message\n");
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "                 Original message size: %d\n", strlen(originalJsonMessage));
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Additional IoTConnect message overhead: %d\n", DX_AVNET_IOT_CONNECT_METADATA);
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "           Required target buffer size: %d\n", maxModifiedMessageSize);
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "              Actual target buffersize: %d\n\n", modifiedBufferSize);

            result = false;
            goto cleanup;
        }

        // Use the API version and the childDevice pointer to determine how to construct the IoTConnect
        // telemetry message.  
        if(AVT_API_VERSION_1_0 == _api_version){

            if(childDevice != NULL){
                snprintf(modifiedJsonMessage, maxModifiedMessageSize, IoTCGwTelemetryJson, 
                        _avt_1_0_properties.sid, _avt_1_0_properties.meta_dtg, childDevice->id, 
                        childDevice->tg, originalJsonMessage);
            }
            else{
                snprintf(modifiedJsonMessage, maxModifiedMessageSize, IoTCTelemetryJson, 
                        _avt_1_0_properties.sid, _avt_1_0_properties.meta_dtg, originalJsonMessage);
            }
        }
        else if(AVT_API_VERSION_2_1 == _api_version){
            
            dx_getCurrentUtc(utcBuffer, utcBufferSize);

            if(childDevice != NULL){
                snprintf(modifiedJsonMessage, maxModifiedMessageSize, IoTCGwTelemetryJson2_1, 
                        utcBuffer, childDevice->id, childDevice->tg, utcBuffer, originalJsonMessage);
            }
            else{                       
                snprintf(modifiedJsonMessage, maxModifiedMessageSize, IoTCTelemetryJson2_1, 
                        utcBuffer, utcBuffer, originalJsonMessage);
            }
        }
    }
    else{
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "ERROR: dx_avnetJsonSerializePayload was passed invalid JSON\n");
    }

cleanup:
    // Release the allocated memory.
    json_value_free(rootProperties);

    return result;
}

bool dx_avnetJsonSerialize(char *jsonMessageBuffer, size_t bufferSize, gw_child_list_node_t* childDevice, int key_value_pair_count, ...)
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
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "ERROR: not enough memory to send telemetry.");
        return false;
    }

    // We need to format the data as shown below
    // "{\"sid\":\"%s\",\"dtg\":\"%s\",\"mt\": 0,\"dt\": \"%s\",\"d\":[{\"d\":<new telemetry "key": value pairs>}]}";

    serializedJson = NULL;
    json_object_dotset_string(root_object, "sid", _avt_1_0_properties.sid);
    json_object_dotset_string(root_object, "dtg", _avt_1_0_properties.meta_dtg);
    json_object_dotset_number(root_object, "mt", 0);

    // If the telemetry is for a GW child device, then add the child id and tag strings
    // "{\"sid\":\"%s\",\"dtg\":\"%s\",\"mt\":0,\"d\":[{\"id\":\"%s\",\"tg\":\"%s\",\"d\":%s}]}";
    //                                                    ^^^^^^^^^^^^^^^^^^^^^^^^^
    if(childDevice != NULL){
        json_object_dotset_string(array_object, "id", childDevice->id);    
        json_object_dotset_string(array_object, "tg", childDevice->tg);    
    }

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
    return _avnetConnected;
}

static const char *response221CodeToString(int iotResponseCode)
{
    switch (iotResponseCode) {

        case AVT_RESPONSE_CODE_OK_CHILD_ADDED:
            return "Child device added";
        case AVT_RESPONSE_CODE_MISSING_CHILD_TAG:
            return "Child not added missing child tag";
        case AVT_RESPONSE_CODE_MISSING_CHILD_UID:
            return "Child not added missing UID";
        case AVT_RESPONSE_CODE_MISSING_CHILD_DISPLAY_NAME:
            return "Child not added missing display name";
        case AVT_RESPONSE_CODE_GW_DEVICE_NOT_FOUND:
            return "Child not added Gateway device not found";
        case AVT_RESPONSE_CODE_UNKNOWN_ERROR:
            return "Child not added, unknown error";
        case AVT_RESPONSE_CODE_INVALID_CHILD_TAG:
            return "Child not added invalid child tag";
        case AVT_RESPONSE_CODE_TAG_NAME_CAN_NOT_BE_THE_SAME_AS_GATEWAY_DEVICE:
            return "Child not added, tag can't be the same a gateway device";
        case AVT_RESPONSE_CODE_UID_ALREADY_EXISTS:
            return "Child not added, this child device already exists";
        default:
            return "Unknown response code";
    }
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

static const char *ErrorCodeToString_2_1(int iotConnectErrorCode)
{
    switch (iotConnectErrorCode) {

    case AVT_ERROR_2_1_CODE_OK:
        return "OK";
    case AVT_ERROR_2_1_CODE_DEV_NOT_FOUND:
        return "device not found";
    case AVT_ERROR_2_1_CODE_DEV_NOT_ACTIVE:
        return "Device not active";
    case AVT_ERROR_2_1_CODE_UNASSOCIATED_DEV:
        return "device not associated with template";
    case AVT_ERROR_2_1_CODE_DEV_NOT_AQUIRED:
        return "device not aquired";
    case AVT_ERROR_2_1_CODE_DEV_DISABLED:
        return "device disabled";
    case AVT_ERROR_2_1_CODE_CPID_NOT_FOUND:
        return "CPID not found";
    case AVT_ERROR_2_1_CODE_SUBSCRIPTION_EXPIRED:
        return "subscription expired";
    case AVT_ERROR_2_1_CODE_CONNECTION_NOT_ALLOWED:
        return "device connection not allowed";
    default:
        return "Unknown error code";
    }
}

static void IoTCrequestChildDeviceInfo(void)
{

    // Send the IoT Connect hello message to inform the platform that we're on-line!
    JSON_Value *rootValue = json_value_init_object();
    JSON_Object *rootObject = json_value_get_object(rootValue);

    if(AVT_API_VERSION_1_0 == _api_version){

        json_object_dotset_number(rootObject, "mt", 204);
        json_object_dotset_string(rootObject, "sid", _avt_1_0_properties.sid);
        
        char *serializedTelemetryUpload = json_serialize_to_string(rootValue);
        dx_azurePublish(serializedTelemetryUpload, strnlen(serializedTelemetryUpload, 128), NULL, 0, NULL);
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "TX: %s\n", serializedTelemetryUpload);
        json_free_serialized_string(serializedTelemetryUpload);  
    }
    else if(AVT_API_VERSION_2_1 == _api_version){

        json_object_dotset_number(rootObject, "mt", 204);
        
        char *serializedTelemetryUpload = json_serialize_to_string(rootValue);
        dx_azurePublish(serializedTelemetryUpload, strnlen(serializedTelemetryUpload, 128), _telemetryMsgProps, NELEMS(_telemetryMsgProps), &_ioTCContentProperties);
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "TX: %s\n", serializedTelemetryUpload);
        json_free_serialized_string(serializedTelemetryUpload);  
    }
 
    json_value_free(rootValue);
}

static bool IoTCProcessDataFrequencyResponse(JSON_Object *dProperties){

    // The meta properties should have a "df" key
    if (json_object_has_value(dProperties, "df") != 0) {
        _avt_2_1_properties.meta_df = (int)json_object_get_number(dProperties, "df");
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "df: %d\n", _avt_2_1_properties.meta_df);
    }
    else {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "df not found!\n");
        return false;
    }  
    
    return true;
}

/*

 Process the hello response message.  We're expecting a specific JSON structure from IoT Connect
 V1 message
 {
     "d": {
         "ec": 0,
         "ct": 200,
         "sid": "NDA5ZTMyMTcyNGMyNGExYWIzMTZhYzE0NTI2MTFjYTU=UTE6MTQ6MDMuMDA=",
         "meta": {
             "g": "0ac9b336-f3e7-4433-9f4e-67668117f2ec",
             "dtg": "9320fa22-ae64-473d-b6ca-aff78da082ed",
             "edge": 0,
             "gtw": "",
             "at": 1,
             "eg": "bdcaebec-d5f8-42a7-8391-b453ec230731"
         },
         "has": {
             "d": 0,
             "attr": 1,
             "set": 0,
             "r": 0,
             "ota": 0
         }
     }
 }

 V2.1 message
{
	"d": {
		"ec": 0,
		"ct": 200,
		"dt": "2022-04-06T16:25:54.9272761Z",
		"meta": {
			"df": 60,
			"cd": "138913Y",
			"gtw": {
				"tg": "spheregwdevice",
				"g": "8274249f-e31d-429b-b1dd-a43d537a5c04"
			},
			"edge": 0,
			"pf": 0,
			"hwv": "",
			"swv": "",
			"v": 2.1
		},
		"has": {
			"d": 0,
			"attr": 1,
			"set": 0,
			"r": 0,
			"ota": 0
		}
	}
}
*/

static bool IoTCProcessHelloResponse(JSON_Object* dProperties){
    
    // Use flags to track 1.0 required properties
    bool dtgFlag = false;
    bool sidFlag = false;

    // Use flags to track 2.1 required properties
    bool cdFlag = false;
    bool vFlag = false;

    // Check to see if the object contains a "meta" object, we use this object
    // further down in this function
    JSON_Object *metaProperties = json_object_dotget_object(dProperties, "meta");
    if (metaProperties == NULL) {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "metaProperties not found\n");
    }

    // Parse a 1.0
    if(AVT_API_VERSION_1_0 == _api_version){

        if (json_object_has_value(dProperties, "ec") != 0) {
            int ecVal = (int)json_object_get_number(dProperties, "ec");
            avt_Debug(AVT_DEBUG_LEVEL_INFO, "ec: %s\n", ErrorCodeToString(ecVal));
        }

        // The d properties should have a "sid" key
        if (json_object_has_value(dProperties, "sid") != 0) {
            strncpy(_avt_1_0_properties.sid, (char *)json_object_get_string(dProperties, "sid"), DX_AVNET_IOT_CONNECT_SID_LEN);
            sidFlag = true;
            avt_Debug(AVT_DEBUG_LEVEL_INFO, "sid: %s\n", _avt_1_0_properties.sid);

        } else {
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "sid not found!\n");
        }

        if(metaProperties != NULL){

            // The meta properties should have a "g" key
            if (json_object_has_value(metaProperties, "g") != 0) {
                strncpy(_avt_1_0_properties.meta_g, (char *)json_object_get_string(metaProperties, "g"), DX_AVNET_IOT_CONNECT_GUID_LEN);
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "g: %s\n", _avt_1_0_properties.meta_g);

            } else {
                avt_Debug(AVT_DEBUG_LEVEL_ERROR, "g not found!\n");
            }

            // The meta properties should have a "eg" key
            if (json_object_has_value(metaProperties, "eg") != 0) {
                strncpy(_avt_1_0_properties.meta_eg, (char *)json_object_get_string(metaProperties, "eg"), DX_AVNET_IOT_CONNECT_GUID_LEN);
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "eg: %s\n", _avt_1_0_properties.meta_eg);

            } else {
                avt_Debug(AVT_DEBUG_LEVEL_ERROR, "eg not found!\n");
            }

            // The meta properties should have a "dtg" key
            if (json_object_has_value(metaProperties, "dtg") != 0) {
                strncpy(_avt_1_0_properties.meta_dtg, (char *)json_object_get_string(metaProperties, "dtg"), DX_AVNET_IOT_CONNECT_GUID_LEN);
              
                // Verify that the new dtg is a valid GUID, if not then we just received an empty dtg.
                if(DX_AVNET_IOT_CONNECT_GUID_LEN-1 == strnlen(_avt_1_0_properties.meta_dtg, DX_AVNET_IOT_CONNECT_GUID_LEN)){ 
                    dtgFlag = true;
                    avt_Debug(AVT_DEBUG_LEVEL_ERROR, "dtg: %s\n", _avt_1_0_properties.meta_dtg);
                }
                else {
                    avt_Debug(AVT_DEBUG_LEVEL_ERROR, "dtg not found or empty!\n");
                }
            }
        }
    }
    else if(AVT_API_VERSION_2_1 == _api_version){

        if (json_object_has_value(dProperties, "ec") != 0) {
            int ecVal = (int)json_object_get_number(dProperties, "ec");
            avt_Debug(AVT_DEBUG_LEVEL_INFO, "ec: %s\n", ErrorCodeToString_2_1(ecVal));
        }

        // Check for invalid 1.0 response
        if (json_object_has_value(dProperties, "sid") != 0) {
            return true;
        }       

        if(metaProperties != NULL){

            // The meta properties should have a "cd" key
            if (json_object_has_value(metaProperties, "cd") != 0) {
                strncpy(_avt_2_1_properties.meta_cd, (char *)json_object_get_string(metaProperties, "cd"), DX_AVNET_IOT_CONNECT_CD_LEN);
                cdFlag = true;
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "cd: %s\n", _avt_2_1_properties.meta_cd);
            }
            else {
                avt_Debug(AVT_DEBUG_LEVEL_ERROR, "cd not found!\n");
            }

            // The meta properties should have a "v" key
            if (json_object_has_value(metaProperties, "v") != 0) {
                _avt_2_1_properties.meta_v = (float)json_object_get_number(metaProperties, "v");
                vFlag = true;
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "v: %.2f\n", _avt_2_1_properties.meta_v);
            }
            else {
                avt_Debug(AVT_DEBUG_LEVEL_ERROR, "v not found!\n");
            }

            // The meta properties should have a "df" key
            if (json_object_has_value(metaProperties, "df") != 0) {
                _avt_2_1_properties.meta_df = (int)json_object_get_number(metaProperties, "df");
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "df: %d\n", _avt_2_1_properties.meta_df);
            }
            else {
                avt_Debug(AVT_DEBUG_LEVEL_ERROR, "df not found!\n");
            }              

            // Check to see if the object contains a "gtw" object.  If so then this is a gateway
            // device and we need to extract data for the IoTConnect messages
            // further down in this function
            JSON_Object *gtwProperties = json_object_dotget_object(metaProperties, "gtw");
            if (gtwProperties == NULL) {

                avt_Debug(AVT_DEBUG_LEVEL_ERROR, "metaProperties not found\n");
            }
            else{

                // The meta properties should have a "tg" key
                if (json_object_has_value(gtwProperties, "tg") != 0) {
                    strncpy(_avt_2_1_properties.meta_gtw_tg, (char *)json_object_get_string(gtwProperties, "tg"), DX_AVNET_IOT_CONNECT_TG_LEN);
                    avt_Debug(AVT_DEBUG_LEVEL_INFO, "tg: %s\n", _avt_2_1_properties.meta_gtw_tg);
                }
                
                // The meta properties should have a "g" key
                if (json_object_has_value(gtwProperties, "g") != 0) {
                    strncpy(_avt_2_1_properties.meta_gtw_g, (char *)json_object_get_string(gtwProperties, "g"), DX_AVNET_IOT_CONNECT_GUID_LEN);
                    avt_Debug(AVT_DEBUG_LEVEL_INFO, "g: %s\n", _avt_2_1_properties.meta_gtw_g);
                }
            }
                    
        }
    }
    // The d object has a "has" object
    JSON_Object *hasProperties = json_object_dotget_object(dProperties, "has");
    if (hasProperties == NULL) {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "hasProperties == NULL\n");
    }

    // The "has" properties should have a "d" key
    if (json_object_has_value(hasProperties, "d") != 0) {
        _avt_has_properties.has_d = (uint8_t)json_object_dotget_number(hasProperties, "d");
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "has:d: %d\n", _avt_has_properties.has_d);
    } else {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "has:d not found!\n");
    }

    // Check to see if we received all the required data we need to interact with IoTConnect
    if((dtgFlag && sidFlag) /* 1.0 */ || (cdFlag && vFlag ) /* 2.1 */){


        if(_avnetConnected == false){

            if(AVT_API_VERSION_2_1 == _api_version){
           
            // Build out the IoTConnect application message properties using the data we just
            // received.          
                IoTCBuildCustomMessageProperties();
            }

            // If the hasDValue is greater than 0, then this is a gateway device and there are child
            // devices configured for this device.  Send the request for child details.  Note that we
            // don't set the _avnetConnected flag in this case.  When the child device information is 
            // received we'll set the flag true.
            if(_avt_has_properties.has_d > 0){
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "has:d: %d\n", _avt_has_properties.has_d);
                IoTCrequestChildDeviceInfo();
            }
            else{
                
                // We have all the data we need set the IoTConnect Connected flag to true
                _avnetConnected = true;
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "Set the IoTCConnected flag to true!\n");
            }
        }
    }
    else{

        // Set the IoTConnect Connected flag to false
        _avnetConnected = false;
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "Did not receive all the required data from IoTConnect\n");
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "Set the IoTCConnected flag to false!\n");

    }
    return true;
}
/* The 204 response contains detils for any child devices configured for this gateway application
  The JSON looks similar to this example.  The example show that there are 3 child devices configured
  for this device.

{
	"d": {
		"d": [{
			"tg": "sensor1",
			"id": "00000002"
		}, {
			"tg": "sensor1",
			"id": "00000003"
		}, {
			"tg": "sensor1",
			"id": "00000001"
		}],
		"ct": 204,
		"ec": 0
	}
}
*/
static bool IoTCProcess204Response(JSON_Object *dProperties){

    JSON_Array *gwArray = NULL;
    JSON_Object *childEntry;

    // The d properties should have a "d" array
    if (json_object_has_value(dProperties, "d") != 0) {

        gwArray = json_object_dotget_array(dProperties, "d");
        if(gwArray != NULL){

            // For each item in the array: Allocate a new gw_child_t structure, fill it in and add it to the 
            // list of children
            for(size_t i = 0; i < json_array_get_count(gwArray); i++){
                
                // Get a pointer to the next object in the array
                childEntry = json_array_get_object(gwArray, i);

                // Check to see if this id is already in the list.  If so,then remove it and add it back in
                // in case the tag changed
                if(IoTCListFindNodeById((char*)json_object_get_string(childEntry, "id"))!= NULL){
                    IoTCListDeleteNodeById((char*)json_object_get_string(childEntry,"id"));
                }

                avt_Debug(AVT_DEBUG_LEVEL_INFO, "RX child details\n");
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "tg: %s\n", json_object_get_string(childEntry, "tg"));
                avt_Debug(AVT_DEBUG_LEVEL_INFO, "id: %s\n", json_object_get_string(childEntry, "id"));

                // Add the child device to the linked list of children devices
                if(!IoTCListAddChild(json_object_get_string(childEntry, "id"), json_object_get_string(childEntry, "tg"))){

                    dx_terminate(DX_ExitCode_Avnet_Add_Child_Failed);
                }
            }
            
            // We have all the data we need set the IoTConnect Connected flag to true
            _avnetConnected = true;
            avt_Debug(AVT_DEBUG_LEVEL_INFO, "Set the IoTCConnected flag to true!\n");

        }
    } else {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "d array object not found!\n");
    }
    
    return true;
}

// This message will create a new child device instance on IoTConnect
static void IoTCSend221AddChildMessage(const char* id, const char* tg, const char* displayName){

    char timeBuffer[64] = {'\0'};
    //DX_MESSAGE_PROPERTY *telemetryMessageProperties[AVT_TELEMETRY_PROP_COUNT];
    char *serializedTelemetryUpload = NULL;


    JSON_Value *rootValue = json_value_init_object();
    JSON_Object *rootObject = json_value_get_object(rootValue);

    if(AVT_API_VERSION_1_0 == _api_version){

        // Construct and send the IoT Connect 221 message to dynamically add a child
        json_object_dotset_string(rootObject, "t", dx_getCurrentUtc(timeBuffer, sizeof(timeBuffer)));
        json_object_dotset_number(rootObject, "mt", 221);
        json_object_dotset_string(rootObject, "sid", _avt_1_0_properties.sid);

        json_object_dotset_string(rootObject, "d.g", _avt_1_0_properties.meta_g);
        json_object_dotset_string(rootObject, "d.dn", displayName);
        json_object_dotset_string(rootObject, "d.id", id);
        json_object_dotset_string(rootObject, "d.eg", _avt_1_0_properties.meta_eg);
        json_object_dotset_string(rootObject, "d.dtg", _avt_1_0_properties.meta_dtg);
        json_object_dotset_string(rootObject, "d.tg", tg);

        serializedTelemetryUpload = json_serialize_to_string(rootValue);
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "TX: %s\n", serializedTelemetryUpload);
        dx_azurePublish(serializedTelemetryUpload, strnlen(serializedTelemetryUpload, 512), NULL, 0, NULL);

    }
    else if(AVT_API_VERSION_2_1 == _api_version){
        
        // Construct and send the IoT Connect 221 message to dynamically add a child
        json_object_dotset_number(rootObject, "mt", 221);
        json_object_dotset_string(rootObject, "d.g", _avt_2_1_properties.meta_gtw_g);
        json_object_dotset_string(rootObject, "d.id", id);
        json_object_dotset_string(rootObject, "d.dn", displayName);
        json_object_dotset_string(rootObject, "d.tg", tg);

        serializedTelemetryUpload = json_serialize_to_string(rootValue);
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "TX: %s\n", serializedTelemetryUpload);
        dx_azurePublish(serializedTelemetryUpload, strnlen(serializedTelemetryUpload, 512), _telemetryMsgProps,
                        NELEMS(_telemetryMsgProps), &_ioTCContentProperties);
    }
   
    json_free_serialized_string(serializedTelemetryUpload);   
    json_value_free(rootValue);
}

/*
Send message to delete a child device on IoTConnect

{
    "t": "2021-05-13T13:20:33.6957862Z",
    "mt": 222,
    "sid": ""
    "d": {
        "g": "", //Device guid
        "id": "" //Child Unique Id
    }
}
*/
static void IoTCSend222DeleteChildMessage(gw_child_list_node_t* childToDelete){

    char timeBuffer[64] = {'\0'};

    // Construct and send the IoT Connect 222 message to dynamically delete a child
    JSON_Value *rootValue = json_value_init_object();
    JSON_Object *rootObject = json_value_get_object(rootValue);
    char *serializedTelemetryUpload = json_serialize_to_string(rootValue);

    if(AVT_API_VERSION_1_0 == _api_version){

        json_object_dotset_string(rootObject, "t", dx_getCurrentUtc(timeBuffer, sizeof(timeBuffer)));
        json_object_dotset_number(rootObject, "mt", 222);
        json_object_dotset_string(rootObject, "sid", _avt_1_0_properties.sid);

        json_object_dotset_string(rootObject, "d.g", _avt_1_0_properties.meta_g);
        json_object_dotset_string(rootObject, "d.id", childToDelete->id);

        serializedTelemetryUpload = json_serialize_to_string(rootValue);
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "TX: %s\n", serializedTelemetryUpload);
        dx_azurePublish(serializedTelemetryUpload, strnlen(serializedTelemetryUpload, 512), NULL, 0, NULL);

    }
    else if(AVT_API_VERSION_2_1 == _api_version){
        
        // Construct and send the IoT Connect 222 message to dynamically delete a child
        json_object_dotset_number(rootObject, "mt", 222);
        json_object_dotset_string(rootObject, "d.id", childToDelete->id);

        serializedTelemetryUpload = json_serialize_to_string(rootValue);
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "TX: %s\n", serializedTelemetryUpload);
        dx_azurePublish(serializedTelemetryUpload, strnlen(serializedTelemetryUpload, 512), _telemetryMsgProps,
                        NELEMS(_telemetryMsgProps), &_ioTCContentProperties);
    }

    json_free_serialized_string(serializedTelemetryUpload);   
    json_value_free(rootValue);

}

// Return a pointer to a node with the passed in id
gw_child_list_node_t *dx_avnetFindChild(const char* id){

    return IoTCListFindNodeById(id);

}

gw_child_list_node_t* dx_avnetGetFirstChild(void){
    
    return _gwChildrenListHead;
}

gw_child_list_node_t* dx_avnetGetNextChild(gw_child_list_node_t* currentChild){

    return currentChild->next;
}

void dx_avnetCreateChildOnIoTConnect(const char* id, const char* tg, const char* dn){

    IoTCSend221AddChildMessage(id, tg, dn);

}

void dx_avnetDeleteChildOnIoTConnect(const char* id){

    gw_child_list_node_t* nodeToDelete = IoTCListFindNodeById(id);
    if(nodeToDelete != NULL){

        IoTCSend222DeleteChildMessage(nodeToDelete);

    }
    else{
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Did not find list node for child to delete\n");
    }
    
}


bool IoTCListAddChild(const char* id, const char* tg){

    gw_child_list_node_t* newChildNode = IoTCListInsertNode(id, tg);
    if(newChildNode == NULL){
        return false;
    }

    return true;
}

/* 
The 221 response is received if we dynamically added a child from the application.
We're expecting a response in the form
{
	"d": {
		"ec": 0,
		"ct": 221,
		"d": {
			"tg": "sensor1",
			"id": "000000000000004",
			"s": 8
		}
	}
}
*/
static bool IoTCProcess221Response(JSON_Object *dProperties){

    char tagString[DX_AVNET_IOT_CONNECT_GW_FIELD_LEN] = {'\0'};
    char idString[DX_AVNET_IOT_CONNECT_GW_FIELD_LEN] = {'\0'};
    int sResponsCode = -1;

    // Using the passed in dProperties, find the embedded "d" object
    JSON_Object *childProperties = json_object_dotget_object(dProperties, "d");
    if (childProperties == NULL) {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "childProperties == NULL\n");
    }

    sResponsCode = (int)json_object_dotget_number(childProperties, "s");
    avt_Debug(AVT_DEBUG_LEVEL_INFO, "Add GW Child response: %s\n", response221CodeToString(sResponsCode));

    // Make sure we have a good response, if not then the tg and id fields may be NULL

    if(sResponsCode == 0){

        // The d properties should have a "tg" key
        if (json_object_has_value(childProperties, "tg") != 0) {
            strncpy(tagString, (char *)json_object_get_string(childProperties, "tg"), DX_AVNET_IOT_CONNECT_GW_FIELD_LEN);

        } else {
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "g not found!\n");
            return false;
        }

        // The d properties should have a "id" key
        if (json_object_has_value(childProperties, "id") != 0) {
            strncpy(idString, (char *)json_object_get_string(childProperties, "id"), DX_AVNET_IOT_CONNECT_GW_FIELD_LEN);

        } else {
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "id not found!\n");
            return false;
        }

        // The child was added on IoTConnect, now add it to the dynamic list of children on the device
        IoTCListAddChild(idString, tagString);

        avt_Debug(AVT_DEBUG_LEVEL_INFO, "Add GW Child id: %s, tag: %s\n", idString, tagString);
    }

    return true;
}

/* 
The 222 response is expected when the application removes a child from IoTConnect
We're expecting a response in the form
{
	"d": {
		"ec": 0,
		"ct": 222,
		"d": {
			"tg": "sensor1",
			"id": "000000000000004",
			"s": 8
		}
	}
}
*/
static bool IoTCProcess222Response(JSON_Object *dProperties){

    char tagString[DX_AVNET_IOT_CONNECT_GW_FIELD_LEN] = {'\0'};
    char idString[DX_AVNET_IOT_CONNECT_GW_FIELD_LEN] = {'\0'};
    int sResponsCode = -1;

    // Using the passed in dProperties, find the embedded "d" object
    JSON_Object *childProperties = json_object_dotget_object(dProperties, "d");
    if (childProperties == NULL) {
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "childProperties == NULL\n");
        return false;
    }

    sResponsCode = (int)json_object_dotget_number(childProperties, "s");
    avt_Debug(AVT_DEBUG_LEVEL_INFO, "Delete GW Child response: %s\n", (sResponsCode == 1)? "Child device not found": "Child device deleted");

    // Make sure we have a good response, if not then the tg and id fields may be NULL
    if(sResponsCode == 0){

        // The d properties should have a "tg" key
        if (json_object_has_value(childProperties, "tg") != 0) {
            strncpy(tagString, (char *)json_object_get_string(childProperties, "tg"), DX_AVNET_IOT_CONNECT_GW_FIELD_LEN);

        } else {
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "g not found!\n");
            return false;
        }

        // The d properties should have a "id" key
        if (json_object_has_value(childProperties, "id") != 0) {
            strncpy(idString, (char *)json_object_get_string(childProperties, "id"), DX_AVNET_IOT_CONNECT_GW_FIELD_LEN);

        } else {
            avt_Debug(AVT_DEBUG_LEVEL_ERROR, "id not found!\n");
            return false;
        }

        // The child was removed from IoTconnect remove it from our list
        IoTCListDeleteNodeById(idString);
        avt_Debug(AVT_DEBUG_LEVEL_INFO, "Delete GW Child id: %s, tag: %s\n", idString, tagString);
    }

    return true;
}

gw_child_list_node_t* IoTCListInsertNode(const char* id, const char* tg){

    // Create the new node on the heap
    gw_child_list_node_t* newNode = IoTCListGetNewNode(id, tg);

    // Check to see if the list is empty
    if(_gwChildrenListHead == NULL){

        _gwChildrenListHead = newNode;
    }
    else{

        // The list is not empty, find the last node in the list
        gw_child_list_node_t* lastNodePtr = _gwChildrenListHead;
        while(lastNodePtr->next != NULL){
            lastNodePtr = lastNodePtr->next;
        } 

        // We found the last node, add the new node to the end of the list    
        lastNodePtr->next = newNode;
        newNode->prev = lastNodePtr;    
    }
    
	return newNode;
}

void IoTClistDelete(void){

    gw_child_list_node_t* currentNode = _gwChildrenListHead;
    gw_child_list_node_t* nextNodePtr = NULL;

    // Traverse the list removing nodes as we go
    while(currentNode != NULL){
        
        nextNodePtr = currentNode->next;
        
        // Free the memory from the heap
        free(currentNode);
        currentNode = nextNodePtr;
    }

    // Reset the head pointer
    _gwChildrenListHead = NULL;
}

void dx_avnetPrintGwChildrenList(void){

    gw_child_list_node_t* currentNode = _gwChildrenListHead;
    uint8_t numChildren = 0;

    // Traverse the list printing node details as we go
    while(currentNode != NULL){

        avt_Debug(AVT_DEBUG_LEVEL_INFO, "Child node #%d, ID: %s, Tag: %s\n", ++numChildren, currentNode->id, currentNode->tg);
        currentNode = currentNode->next;
    }
}

bool IoTCListDeleteNodeById(const char* id){

    gw_child_list_node_t* nodePtr = IoTCListFindNodeById(id);
    return IoTCListDeleteNode(nodePtr);
}

gw_child_list_node_t* IoTCListFindNodeById(const char* id){

    // Check to see if the list is empty
    if(_gwChildrenListHead == NULL){

        return NULL;
    }
    else{

        // The list is not empty, traverse the list looking for the node 
        gw_child_list_node_t* nodePtr = _gwChildrenListHead;
        while(nodePtr != NULL){
            if(strncmp(nodePtr->id, id, DX_AVNET_IOT_CONNECT_GW_FIELD_LEN)==0){
                return nodePtr;
            }
            nodePtr = nodePtr->next;
        } 
    }

    // We did not find the node, return NULL    
	return NULL;
}

bool IoTCListDeleteNode(gw_child_list_node_t* nodeToRemove){

    // If the node was not found, then return
    if(nodeToRemove == NULL){
        return false;
    }

    // First check to see if the node is the head
    if(_gwChildrenListHead == nodeToRemove){

        // Check to see if there is a node after the head
        if(nodeToRemove->next != NULL){
            nodeToRemove->next->prev = NULL;
            _gwChildrenListHead = nodeToRemove->next;
        }
        // Else the list has one entry and it's the head node
        else{
            _gwChildrenListHead = NULL;
        }

        free(nodeToRemove);
        return true;
    }

    // Check if the node to remove is at the end of the list
    if(nodeToRemove->next == NULL){

        nodeToRemove->prev->next = NULL;
        free(nodeToRemove);
        return true;
    }

    // Else the node is not at the head or tail
    nodeToRemove->prev->next = nodeToRemove->next;
    nodeToRemove->next->prev = nodeToRemove->prev;
    free(nodeToRemove);
    return true;
}

//Creates a new Node and returns pointer to it, note the node is not inserted
// into a list, it's simply created on the heap.
gw_child_list_node_t* IoTCListGetNewNode(const char* id, const char* tg) {

	size_t nodeSize = sizeof(gw_child_list_node_t);

	gw_child_list_node_t* newNode = malloc(nodeSize);

	// Verify we were able to allocate memory for the new node, if not
	// then set exitCode to reflect the error.  The main loop will see this
	// change and exit.
	if(newNode == NULL){
		dx_terminate(DX_ExitCode_Add_List_Node_Malloc_Failed);
	}
	memset(newNode, 0,  nodeSize);

	newNode->prev = NULL;
	newNode->next = NULL;
	strncpy(newNode->id, id, DX_AVNET_IOT_CONNECT_GW_FIELD_LEN);
	strncpy(newNode->tg, tg, DX_AVNET_IOT_CONNECT_GW_FIELD_LEN);
	return newNode;
}

/// <summary>
/// Defines what IoTConnect API to use.  If this function is not called
/// then the default API will be 2.1
/// </summary>
/// <returns></returns>
void dx_avnetSetApiVersion(avt_iotc_api_ver_t version){

    static bool versionHasBeenSet = false;    

    if(versionHasBeenSet){
        avt_Debug(AVT_DEBUG_LEVEL_ERROR, "Avnet API version can only be set once!\n");
        dx_terminate(DX_ExitCode_IoTC_Set_Api_Version_Error);
    }

    // Verify the input
    if(version < AVT_API_MAX_VERSION){

        // Set the global version variable
        _api_version = version;

        // Toggle the been here done that flag
        versionHasBeenSet = true;
    }
}

/// Takes a properly formatted JSON telemetry message . . 
/// 1. Adds the required Avnet IoTConnect Header to the message
/// 2. Add the 2.1 API required application message properties to the incomming messageProperties list
/// 3. Sends the message to the IoTHub
/// </summary>
/// <returns></returns>
bool dx_avnetPublish(const void *message, 
                     size_t messageLength, 
                     DX_MESSAGE_PROPERTY **messageProperties, 
                     size_t messagePropertyCount,
                     DX_MESSAGE_CONTENT_PROPERTIES *messageContentProperties, 
                     gw_child_list_node_t* childDevice){

    // Declare an array large enough for the incomming message properties + 3 for the required IoTConnect
    // application message properties
    DX_MESSAGE_PROPERTY *telemetryMessageProperties[messagePropertyCount + AVT_TELEMETRY_PROP_COUNT];

    size_t modifiedPropertyCount = 0;

    // Allocate memory for this array of pointers
    for(int i = 0; i < messagePropertyCount + AVT_TELEMETRY_PROP_COUNT; i++){
        telemetryMessageProperties[i] = malloc(sizeof(DX_MESSAGE_PROPERTY));
        if(telemetryMessageProperties[i] == NULL){
            dx_terminate(DX_ExitCode_IoTC_Memory_Allocation_Failed);
            return false;
        }
        memset(telemetryMessageProperties[i], 0x00, sizeof(DX_MESSAGE_PROPERTY));
    }

    // Verify we have a valid connection to IoTConnect
    if(!dx_isAvnetConnected()){
        return false;
    }

    // Verify there's a message to send!
    if(message == NULL || messageLength == 0){
        return false;
    }

    // Declare a char buffer large enough for the incomming message plus the IoTConnect header overhead
    size_t max_msg_buffer_size = messageLength + DX_AVNET_IOT_CONNECT_METADATA;
    char *avt_msg_buffer = malloc(max_msg_buffer_size);
    if(avt_msg_buffer == NULL){
        dx_terminate(DX_ExitCode_IoTC_Memory_Allocation_Failed);
        return false;
    }
    memset(avt_msg_buffer, 0x00, max_msg_buffer_size);

    // Add the Avnet IoTConnect header/wrapper to the incomming telemetry JSON.  This routine will determine which
    // Avnet API version is being used and add the correct header/wrapper data.    
    if(dx_avnetJsonSerializePayload(message, avt_msg_buffer, max_msg_buffer_size, childDevice)){
    
        avt_Debug(AVT_DEBUG_LEVEL_INFO,"%s\n", avt_msg_buffer);

        // If we're using the 2.1 API, we need to add application message properties for IoTConnect routing
        if(_api_version == AVT_API_VERSION_2_1){

            // Copy the IoTConnect application message properties into the local array
            for (size_t i = 0; i < AVT_TELEMETRY_PROP_COUNT ; i++) {
                if (!dx_isStringNullOrEmpty(_telemetryMsgProps[i]->key) && !dx_isStringNullOrEmpty(_telemetryMsgProps[i]->value)) {
                    telemetryMessageProperties[i]->key = _telemetryMsgProps[i]->key;
                    telemetryMessageProperties[i]->value = _telemetryMsgProps[i]->value; 
                    modifiedPropertyCount++;
                }
            }

            // Determine if there are passed in message properties.  If so, we need to add the
            // message properties to our array of pointers. Start right after the IoTConnect
            // properties we just added
            if ((messageProperties != NULL) && (messagePropertyCount > 0)) {
                for (size_t i = 0; i < messagePropertyCount ; i++) {
                    if (!dx_isStringNullOrEmpty(messageProperties[i]->key) && !dx_isStringNullOrEmpty(messageProperties[i]->value)) {
                        telemetryMessageProperties[i + AVT_TELEMETRY_PROP_COUNT]->key = messageProperties[i]->key;
                        telemetryMessageProperties[i + AVT_TELEMETRY_PROP_COUNT]->value = messageProperties[i]->value; 
                        modifiedPropertyCount++;
                    }
                }
            }

            // Send the modified message!
            dx_azurePublish(avt_msg_buffer, 
                        strnlen(avt_msg_buffer, max_msg_buffer_size),
                        telemetryMessageProperties, 
                        NELEMS(telemetryMessageProperties),
                        messageContentProperties);
        }
        else if(AVT_API_VERSION_1_0 == _api_version){
                        // Send the modified message!
            dx_azurePublish(avt_msg_buffer, 
                        strnlen(avt_msg_buffer, max_msg_buffer_size), 
                        messageProperties, 
                        messagePropertyCount,
                        messageContentProperties);
        }
    }

    // Free the memory for the array of pointers
    for(int i = 0; i < messagePropertyCount + AVT_TELEMETRY_PROP_COUNT; i++){
        free(telemetryMessageProperties[i]);
    }

    free(avt_msg_buffer);
    return true;
 }

/// <summary>
/// Returns the IoTConnect data frequency sent to the device.
/// It's up to the developer to use and honor this value
/// </summary>
/// <returns></returns>
int dx_avnetGetDataFrequency(void){

    if(0 == _avt_2_1_properties.meta_df){

        return 60;
    }
    else{

        return _avt_2_1_properties.meta_df;
    }
}

static bool IoTCBuildCustomMessageProperties(void){
    
    // Protect against multiple entries/memory allocations
    static bool memoryIsAllocated = false;

    // Check for the case where we received a stale C2D message
//    if(dx_isAvnetConnected()){

        if(!memoryIsAllocated){
            // Allocate memory for this array of pointers
            for(size_t i = 0; i < AVT_TELEMETRY_PROP_COUNT; i++){
                _telemetryMsgProps[i] = malloc(sizeof(DX_MESSAGE_PROPERTY));
                _devIdMsgProps[i] =  malloc(sizeof(DX_MESSAGE_PROPERTY));
                if((_telemetryMsgProps[i] == NULL) || (_devIdMsgProps[i] == NULL)){
                    dx_terminate(DX_ExitCode_IoTC_Memory_Allocation_Failed);
                    return false;
                }
                memset(_telemetryMsgProps[i], 0x00, sizeof(DX_MESSAGE_PROPERTY));
                memset(_devIdMsgProps[i], 0x00, sizeof(DX_MESSAGE_PROPERTY));

            }
            memoryIsAllocated = true;
        }

        // Populate the telemetry Message Properties
        _telemetryMsgProps[AVT_TELEMETRY_PROP_CD]->key = _cdKey;
        strncpy(_cdValue, _avt_2_1_properties.meta_cd, DX_AVNET_IOT_CONNECT_MAX_MSG_PROPERTY_LEN);
        _telemetryMsgProps[AVT_TELEMETRY_PROP_CD]->value = _cdValue;

        snprintf(_vValue, 4, "%.1f", _avt_2_1_properties.meta_v);
        _telemetryMsgProps[AVT_TELEMETRY_PROP_V]->key = _vKey;
        _telemetryMsgProps[AVT_TELEMETRY_PROP_V]->value = _vValue;

        _telemetryMsgProps[AVT_TELEMETRY_PROP_MT]->key = _mtKey;
        _telemetryMsgProps[AVT_TELEMETRY_PROP_MT]->value = _mtValue;

        // Populate the Device ID Message Properties
        _devIdMsgProps[AVT_DEV_ID_PROP_CD]->key = _cdKey;
        _devIdMsgProps[AVT_DEV_ID_PROP_CD]->value = _cdValue;

        _devIdMsgProps[AVT_DEV_ID_PROP_V]->key = _vKey;
        _devIdMsgProps[AVT_DEV_ID_PROP_V]->value = _vValue;

        _devIdMsgProps[AVT_DEV_ID_PROP_DI]->key = _diKey;
        _devIdMsgProps[AVT_DEV_ID_PROP_DI]->value = _diValue;

        avt_Debug(AVT_DEBUG_LEVEL_INFO, "AVT custom message properties ready!\n");
        
        return true;
//    }
  //  return false;
}

/// <summary>
/// Returns the number of gateway child devices 
/// </summary>
/// <returns></returns>
int dx_avnetGwGetNumChildren(void){
    
    int numChildren = 0;
    gw_child_list_node_t* nodePtr = dx_avnetGetFirstChild();
    
    while(nodePtr != NULL){

        numChildren++;
        nodePtr = dx_avnetGetNextChild(nodePtr);
    }
    return numChildren;
}

void avt_Debug(uint8_t debugLevel, char *fmt, ...)
{

    if(debugLevel >= AVT_DEBUG_LEVEL_LEVEL_MAX){
        return;
    }

    if(debugLevel <= _avt_debugLevel){
        
        Log_Debug("[AVT IoTConnect] ");
        va_list args;
        va_start(args, fmt);
        vsnprintf(_avt_log_debug_buffer, _avt_log_debug_buffer_size, fmt, args);
        va_end(args);
        Log_Debug(_avt_log_debug_buffer);
    }
}

/// <summary>
/// Sets the debug level for the Avnet IoTConnect implementation 
/// </summary>
/// <param name="debugLevel"></param>
/// <returns></returns>
void dx_avnetSetDebugLevel(uint8_t debugLevel){

    if(debugLevel < AVT_DEBUG_LEVEL_LEVEL_MAX){
       
        _avt_debugLevel = debugLevel;

    }
}
