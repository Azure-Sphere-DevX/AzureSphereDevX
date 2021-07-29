/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 *   DISCLAIMER
 *
 *   The DevX library supports the Azure Sphere Developer Learning Path:
 *
 *	   1. are built from the Azure Sphere SDK Samples at
 *          https://github.com/Azure/azure-sphere-samples
 *	   2. are not intended as a substitute for understanding the Azure Sphere SDK Samples.
 *	   3. aim to follow best practices as demonstrated by the Azure Sphere SDK Samples.
 *	   4. are provided as is and as a convenience to aid the Azure Sphere Developer Learning
 *          experience.
 *
 *   DEVELOPER BOARD SELECTION
 *
 *   The following developer boards are supported.
 *
 *	   1. AVNET Azure Sphere Starter Kit.
 *     2. AVNET Azure Sphere Starter Kit Revision 2.
 *	   3. Seeed Studio Azure Sphere MT3620 Development Kit aka Reference Design Board or rdb.
 *	   4. Seeed Studio Seeed Studio MT3620 Mini Dev Board.
 *
 *   ENABLE YOUR DEVELOPER BOARD
 *
 *   Each Azure Sphere developer board manufacturer maps pins differently. You need to select the
 *      configuration that matches your board.
 *
 *   Follow these steps:
 *
 *	   1. Open CMakeLists.txt.
 *	   2. Uncomment the set command that matches your developer board.
 *	   3. Click File, then Save to save the CMakeLists.txt file which will auto generate the
 *          CMake Cache.
 *
 ************************************************************************************************/

#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_json_serializer.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include "dx_version.h"
#include <applibs/log.h>

// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:com:example:azuresphere:labmonitor;1"
#define NETWORK_INTERFACE "wlan0"
#define SAMPLE_VERSION_NUMBER "1.0"
#define ONE_MS 1000000

// Forward declarations
static DX_DIRECT_METHOD_RESPONSE_CODE LightControlHandler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void publish_message_handler(EventLoopTimer *eventLoopTimer);
static void report_properties_handler(EventLoopTimer *eventLoopTimer);

DX_USER_CONFIG dx_config;

/****************************************************************************************
 * Telemetry message buffer property sets
 ****************************************************************************************/

// Number of bytes to allocate for the JSON telemetry message for IoT Hub/Central
#define JSON_MESSAGE_BYTES 256
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

static DX_MESSAGE_PROPERTY *messageProperties[] = {&(DX_MESSAGE_PROPERTY){.key = "appid", .value = "hvac"}, &(DX_MESSAGE_PROPERTY){.key = "type", .value = "telemetry"},
                                                   &(DX_MESSAGE_PROPERTY){.key = "schema", .value = "1"}};

static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8", .contentType = "application/json"};

// declare all bindings
static DX_DEVICE_TWIN_BINDING dt_desired_sample_rate = {.propertyName = "DesiredSampleRate", .twinType = DX_DEVICE_TWIN_INT, .handler = dt_desired_sample_rate_handler};
static DX_DEVICE_TWIN_BINDING dt_deviceConnectUtc = {.propertyName = "DeviceConnectUtc", .twinType = DX_DEVICE_TWIN_STRING};
static DX_DEVICE_TWIN_BINDING dt_deviceStartUtc = {.propertyName = "DeviceStartUtc", .twinType = DX_DEVICE_TWIN_STRING};
static DX_DEVICE_TWIN_BINDING dt_reported_humidity = {.propertyName = "ReportedHumidity", .twinType = DX_DEVICE_TWIN_DOUBLE};
static DX_DEVICE_TWIN_BINDING dt_reported_temperature = {.propertyName = "ReportedTemperature", .twinType = DX_DEVICE_TWIN_FLOAT};
static DX_DEVICE_TWIN_BINDING dt_reported_utc = {.propertyName = "ReportedUTC", .twinType = DX_DEVICE_TWIN_STRING};
static DX_DEVICE_TWIN_BINDING dt_softwareVersion = {.propertyName = "SoftwareVersion", .twinType = DX_DEVICE_TWIN_STRING};

static DX_DIRECT_METHOD_BINDING dm_light_control = {.methodName = "LightControl", .handler = LightControlHandler};
static DX_GPIO_BINDING gpio_led = {.pin = LED2, .name = "gpio_led", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true};

static DX_GPIO_BINDING gpio_network_led = {.pin = NETWORK_CONNECTED_LED, .name = "gpio_network_led", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true};

static DX_TIMER_BINDING tmr_publish_message = {.period = {4, 0}, .name = "tmr_publish_message", .handler = publish_message_handler};
static DX_TIMER_BINDING tmr_report_properties = {.period = {5, 0}, .name = "tmr_report_properties", .handler = report_properties_handler};

// All bindings referenced in the folowing binding sets are initialised in the InitPeripheralsAndHandlers function
DX_DEVICE_TWIN_BINDING *device_twin_bindings[] = {&dt_deviceStartUtc,    &dt_softwareVersion, &dt_desired_sample_rate, &dt_reported_temperature,
                                                  &dt_reported_humidity, &dt_reported_utc,    &dt_deviceConnectUtc};
DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {&dm_light_control};
DX_GPIO_BINDING *gpio_bindings[] = {&gpio_network_led, &gpio_led};
DX_TIMER_BINDING *timer_bindings[] = {&tmr_publish_message, &tmr_report_properties};

/****************************************************************************************
 * Implementation
 ****************************************************************************************/

static void publish_message_handler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    double temperature = 36.0;
    double humidity = 55.0;
    double pressure = 1100;
    static int msgId = 0;

    if (dx_isAzureConnected()) {

        // Serialize telemetry as JSON
        bool serialization_result = dx_jsonSerialize(msgBuffer, sizeof(msgBuffer), 4, 
            DX_JSON_INT, "MsgId", msgId++, 
            DX_JSON_DOUBLE, "Temperature", temperature, 
            DX_JSON_DOUBLE, "Humidity", humidity, 
            DX_JSON_DOUBLE, "Pressure", pressure);

        if (serialization_result) {

            Log_Debug("%s\n", msgBuffer);

            dx_azurePublish(msgBuffer, strlen(msgBuffer), messageProperties, NELEMS(messageProperties), &contentProperties);

        } else {
            Log_Debug("JSON Serialization failed: Buffer too small\n");
        }
    }
}

static void report_properties_handler(EventLoopTimer *eventLoopTimer)
{
    float temperature = 25.05f;
    double humidity = 60.25;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    if (dx_isAzureConnected()) {

        // Update twin with current UTC (Universal Time Coordinate) in ISO format
        dx_deviceTwinReportValue(&dt_reported_utc, dx_getCurrentUtc(msgBuffer, sizeof(msgBuffer)));

        // The type passed in must match the Divice Twin Type DX_DEVICE_TWIN_FLOAT
        dx_deviceTwinReportValue(&dt_reported_temperature, &temperature);

        // The type passed in must match the Divice Twin Type DX_DEVICE_TWIN_DOUBLE
        dx_deviceTwinReportValue(&dt_reported_humidity, &humidity);
    }
}

static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    int sample_rate_seconds = *(int *)deviceTwinBinding->propertyValue;

    // validate data is sensible range before applying
    if (sample_rate_seconds >= 0 && sample_rate_seconds <= 120) {

        dx_timerChange(&tmr_publish_message, &(struct timespec){sample_rate_seconds, 0});

        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_COMPLETED);

    } else {
        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_ERROR);
    }

    /*	Casting device twin state examples

            float value = *(float*)deviceTwinBinding->propertyValue;
            double value = *(double*)deviceTwinBinding->propertyValue;
            int value = *(int*)deviceTwinBinding->propertyValue;
            bool value = *(bool*)deviceTwinBinding->propertyValue;
            char* value = (char*)deviceTwinBinding->propertyValue;
    */
}

// Direct method name = LightControl, json payload = {"State": true }
static DX_DIRECT_METHOD_RESPONSE_CODE LightControlHandler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    char state_str[] = "State";
    bool requested_state;

    JSON_Object *jsonObject = json_value_get_object(json);
    if (jsonObject == NULL) {
        return DX_METHOD_FAILED;
    }

    requested_state = (bool)json_object_get_boolean(jsonObject, state_str);

    dx_gpioStateSet(&gpio_led, requested_state);

    return DX_METHOD_SUCCEEDED;
}

static void NetworkConnectionState(bool connected)
{
    static bool first_time = true;

    if (first_time && connected) {
        first_time = false;

        // This is the first connect so update device start time UTC and software version
        dx_deviceTwinReportValue(&dt_deviceStartUtc, dx_getCurrentUtc(msgBuffer, sizeof(msgBuffer)));
        snprintf(msgBuffer, sizeof(msgBuffer), "Sample version: %s, DevX version: %s", SAMPLE_VERSION_NUMBER, AZURE_SPHERE_DEVX_VERSION);
        dx_deviceTwinReportValue(&dt_softwareVersion, msgBuffer);
    }

    if (connected) {
        dx_deviceTwinReportValue(&dt_deviceConnectUtc, dx_getCurrentUtc(msgBuffer, sizeof(msgBuffer)));
    }

    dx_gpioStateSet(&gpio_network_led, connected);
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timer_bindings.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, IOT_PLUG_AND_PLAY_MODEL_ID);
    dx_gpioSetOpen(gpio_bindings, NELEMS(gpio_bindings));
    dx_timerSetStart(timer_bindings, NELEMS(timer_bindings));
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));
    dx_directMethodSubscribe(direct_method_bindings, NELEMS(direct_method_bindings));

    dx_azureRegisterConnectionChangedNotification(NetworkConnectionState);
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timer_bindings, NELEMS(timer_bindings));
    dx_deviceTwinUnsubscribe();
    dx_directMethodUnsubscribe();
    dx_gpioSetClose(gpio_bindings, NELEMS(gpio_bindings));
    dx_timerEventLoopStop();
}

int main(int argc, char *argv[])
{
    dx_registerTerminationHandler();

    if (!dx_configParseCmdLineArguments(argc, argv, &dx_config)) {
        return dx_getTerminationExitCode();
    }

    InitPeripheralsAndHandlers();

    // Main loop
    while (!dx_isTerminationRequired()) {
        int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == -1 && errno != EINTR) {
            dx_terminate(DX_ExitCode_Main_EventLoopFail);
        }
    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return dx_getTerminationExitCode();
}