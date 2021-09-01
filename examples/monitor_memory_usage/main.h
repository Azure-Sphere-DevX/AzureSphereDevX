#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "app_exit_codes.h"
#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_json_serializer.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include "dx_version.h"

#include <applibs/log.h>
#include <applibs/applications.h>

// Use main.h to define all your application definitions, message properties/contentProperties,
// bindings and binding sets.

// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define IOT_PLUG_AND_PLAY_MODEL_ID "" //TODO insert your PnP model ID if your application supports PnP
#define NETWORK_INTERFACE "wlan0"
#define SAMPLE_VERSION_NUMBER "1.0"
#define ONE_MS 1000000

DX_USER_CONFIG dx_config;

/****************************************************************************************
 * Avnet IoTConnect Support
 ****************************************************************************************/
// TODO: If the application will connect to Avnet's IoTConnect platform enable the 
// #define below
// #define USE_AVNET_IOTCONNECT

/****************************************************************************************
 * Application defines
 ****************************************************************************************/
#define MIN_MEMORY_LEAK 1
#define MAX_MEMORY_LEAK 50000
#define MONITOR_PERIOD 30

/****************************************************************************************
 * Forward declarations
 ****************************************************************************************/
static DX_DIRECT_METHOD_RESPONSE_CODE MemoryLeakHandler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void monitor_memory_handler(EventLoopTimer *eventLoopTimer);

/****************************************************************************************
 * Telemetry message buffer property sets
 ****************************************************************************************/

// Number of bytes to allocate for the JSON telemetry message for IoT Hub/Central
#ifdef USE_AVNET_IOTCONNECT
#define JSON_MESSAGE_BYTES 256+128 // Add additional buffer for the IoTConnect header data
#else
#define JSON_MESSAGE_BYTES 256
#endif 
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

static DX_MESSAGE_PROPERTY *memoryMessageProperties[] = {&(DX_MESSAGE_PROPERTY){.key = "appid", .value = "hvac"}, 
                                                        &(DX_MESSAGE_PROPERTY){.key = "type", .value = "memoryEvent"},
                                                        &(DX_MESSAGE_PROPERTY){.key = "schema", .value = "1"}};

static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8", .contentType = "application/json"};

/****************************************************************************************
 * DevX Bindings
 ****************************************************************************************/
static DX_DIRECT_METHOD_BINDING dm_memory_leak = {.methodName = "MemoryLeak", .handler = MemoryLeakHandler};
static DX_TIMER_BINDING tmr_monitor_memory = {.period = {MONITOR_PERIOD, 0}, .name = "tmr_monitor_memory", .handler = monitor_memory_handler};

/****************************************************************************************
 * Binding sets
 ****************************************************************************************/
// These sets are used by the initailization code.

DX_DEVICE_TWIN_BINDING *device_twin_bindings[] = {};
DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {&dm_memory_leak};
DX_GPIO_BINDING *gpio_bindings[] = {};
DX_TIMER_BINDING *timer_bindings[] = {&tmr_monitor_memory};
