// Use main.h to define all your application definitions, message properties/contentProperties,
// bindings and binding sets.

// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define IOT_PLUG_AND_PLAY_MODEL_ID "" //TODO insert your PnP model ID if your application supports PnP

// Details on how to connect your application using an ethernet adaptor
// https://docs.microsoft.com/en-us/azure-sphere/network/connect-ethernet
#define NETWORK_INTERFACE "wlan0"

#define SAMPLE_VERSION_NUMBER "1.0"
#define ONE_MS 1000000

DX_USER_CONFIG dx_config;

/****************************************************************************************
 * Avnet IoTConnect Support
 ****************************************************************************************/
// TODO: If the application will connect to Avnet's IoTConnect platform enable the 
// #define below
//#define USE_AVNET_IOTCONNECT

/****************************************************************************************
 * Application defines
 ****************************************************************************************/
// TODO: Add any application constants

/****************************************************************************************
 * Forward declarations
 ****************************************************************************************/
// TODO: Add all Forward declarations here or in main.c

/****************************************************************************************
 * Telemetry message buffer property sets
 ****************************************************************************************/

// Number of bytes to allocate for the JSON telemetry message for IoT Hub/Central
// TODO: Remove comments to use the global message buffer for sending telemetry
//#define JSON_MESSAGE_BYTES 256
//static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

// TODO: Define telemetry message properties here, for example . . . 
//static DX_MESSAGE_PROPERTY *messageProperties[] = {&(DX_MESSAGE_PROPERTY){.key = "appid", .value = "hvac"}, 
//                                                   &(DX_MESSAGE_PROPERTY){.key = "type", .value = "telemetry"},
//                                                   &(DX_MESSAGE_PROPERTY){.key = "schema", .value = "1"}};

// TODO: Remove comments to define contentProperties for sending telemetry
//static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8", .contentType = "application/json"};

/****************************************************************************************
 * Bindings
 ****************************************************************************************/
// TODO: Declare all bindings here, for example . . . 

//static DX_DEVICE_TWIN_BINDING dt_desired_sample_rate = {.propertyName = "DesiredSampleRate", .twinType = DX_DEVICE_TWIN_INT, .handler = dt_desired_sample_rate_handler};
//static DX_GPIO_BINDING gpio_led = {.pin = LED2, .name = "gpio_led", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true};
//static DX_TIMER_BINDING tmr_publish_message = {.period = {4, 0}, .name = "tmr_publish_message", .handler = publish_message_handler};

/****************************************************************************************
 * Binding sets
 ****************************************************************************************/
// TODO: Update each binding set below with the bindings defined above.  Add bindings by reference, i.e., &dt_desired_sample_rate
// These sets are used by the initailization code.

DX_DEVICE_TWIN_BINDING *device_twin_bindings[] = {};
DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {};
DX_GPIO_BINDING *gpio_bindings[] = {};
DX_TIMER_BINDING *timer_bindings[] = {};
