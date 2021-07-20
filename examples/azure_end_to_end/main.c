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
#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_json_serializer.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <applibs/log.h>

// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:com:example:azuresphere:labmonitor;1"
#define NETWORK_INTERFACE "wlan0"

// Forward declarations
static void publish_message_handler(EventLoopTimer *eventLoopTimer);
static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void report_now_handler(EventLoopTimer *eventLoopTimer);
static DX_DIRECT_METHOD_RESPONSE_CODE LightControlHandler(JSON_Value *json,
                                                          DX_DIRECT_METHOD_BINDING *directMethodBinding,
                                                          char **responseMsg);
;

DX_USER_CONFIG dx_config;

/****************************************************************************************
 * Telemetry message buffer property sets
 ****************************************************************************************/

// Number of bytes to allocate for the JSON telemetry message for IoT Hub/Central
#define JSON_MESSAGE_BYTES 256
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

static DX_MESSAGE_PROPERTY *messageProperties[] = {&(DX_MESSAGE_PROPERTY){.key = "appid", .value = "hvac"},
                                                   &(DX_MESSAGE_PROPERTY){.key = "type", .value = "telemetry"},
                                                   &(DX_MESSAGE_PROPERTY){.key = "schema", .value = "1"}};

static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8",
                                                          .contentType = "application/json"};

static DX_GPIO_BINDING led = {
    .pin = LED2, .name = "led", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true};
static DX_GPIO_BINDING network_led = {.pin = NETWORK_CONNECTED_LED,
                                      .name = "network_led",
                                      .direction = DX_OUTPUT,
                                      .initialState = GPIO_Value_Low,
                                      .invertPin = true};

// All GPIOs added to gpio_set will be opened in InitPeripheralsAndHandlers
DX_GPIO_BINDING *gpio_set[] = {&network_led, &led};

/****************************************************************************************
 * Timer Bindings
 ****************************************************************************************/
static DX_TIMER_BINDING publish_message = {
    .period = {0, 250000000}, .name = "publish_message", .handler = publish_message_handler};
static DX_TIMER_BINDING report_now_timer = {
    .period = {5, 0}, .name = "report_now_timer", .handler = report_now_handler};

// All timers referenced in timers with be opened in the InitPeripheralsAndHandlers function
DX_TIMER_BINDING *timers[] = {&publish_message, &report_now_timer};

/****************************************************************************************
 * Azure IoT Device Twin Bindings
 ****************************************************************************************/
static DX_DEVICE_TWIN_BINDING dt_desired_sample_rate = {
    .twinProperty = "DesiredSampleRate", .twinType = DX_DEVICE_TWIN_INT, .handler = dt_desired_sample_rate_handler};

static DX_DEVICE_TWIN_BINDING dt_reported_temperature = {.twinProperty = "ReportedTemperature",
                                                         .twinType = DX_DEVICE_TWIN_FLOAT};

static DX_DEVICE_TWIN_BINDING dt_reported_humidity = {.twinProperty = "ReportedHumidity",
                                                      .twinType = DX_DEVICE_TWIN_DOUBLE};

static DX_DEVICE_TWIN_BINDING dt_reported_utc = {.twinProperty = "ReportedUTC", .twinType = DX_DEVICE_TWIN_STRING};

// All device twins listed in device_twin_bindings will be subscribed to in
// the InitPeripheralsAndHandlers function
DX_DEVICE_TWIN_BINDING *device_twin_bindings[] = {&dt_desired_sample_rate, &dt_reported_temperature,
                                                  &dt_reported_humidity, &dt_reported_utc};

/****************************************************************************************
 * Azure IoT Direct Method Bindings
 ****************************************************************************************/
static DX_DIRECT_METHOD_BINDING dm_light_control = {.methodName = "LightControl", .handler = LightControlHandler};

// All direct methods referenced in direct_method_bindings will be subscribed to in
// the InitPeripheralsAndHandlers function
DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {&dm_light_control};

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
        bool serialization_result = dx_jsonSerialize(msgBuffer, sizeof(msgBuffer), 4, DX_JSON_INT, "MsgId", msgId++,
                                                     DX_JSON_DOUBLE, "Temperature", temperature, DX_JSON_DOUBLE,
                                                     "Humidity", humidity, DX_JSON_DOUBLE, "Pressure", pressure);

        if (serialization_result) {

            Log_Debug("%s\n", msgBuffer);

            dx_azurePublish(msgBuffer, strlen(msgBuffer), messageProperties, NELEMS(messageProperties),
                            &contentProperties);

        } else {
            Log_Debug("JSON Serialization failed: Buffer too small\n");
        }
    }
}

static void report_now_handler(EventLoopTimer *eventLoopTimer)
{
    float temperature = 25.05f;
    double humidity = 60.25;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    // Update twin with current UTC (Universal Time Coordinate) in ISO format
    dx_deviceTwinReportState(&dt_reported_utc, dx_getCurrentUtc(msgBuffer, sizeof(msgBuffer)));

    // The type passed in must match the Divice Twin Type DX_DEVICE_TWIN_FLOAT
    dx_deviceTwinReportState(&dt_reported_temperature, &temperature);

    // The type passed in must match the Divice Twin Type DX_DEVICE_TWIN_DOUBLE
    dx_deviceTwinReportState(&dt_reported_humidity, &humidity);
}

static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    // validate data is sensible range before applying
    if (deviceTwinBinding->twinType == DX_DEVICE_TWIN_INT && *(int *)deviceTwinBinding->twinState >= 0 &&
        *(int *)deviceTwinBinding->twinState <= 120) {

        dx_timerChange(&report_now_timer, &(struct timespec){*(int *)deviceTwinBinding->twinState, 0});

        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState,
                                     DX_DEVICE_TWIN_RESPONSE_COMPLETED);

    } else {
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_RESPONSE_ERROR);
    }

    /*	Casting device twin state examples

            float value = *(float*)deviceTwinBinding->twinState;
            double value = *(double*)deviceTwinBinding->twinState;
            int value = *(int*)deviceTwinBinding->twinState;
            bool value = *(bool*)deviceTwinBinding->twinState;
            char* value = (char*)deviceTwinBinding->twinState;
    */
}

// Direct method name = LightControl, json payload = {"State": true }
static DX_DIRECT_METHOD_RESPONSE_CODE LightControlHandler(JSON_Value *json,
                                                          DX_DIRECT_METHOD_BINDING *directMethodBinding,
                                                          char **responseMsg)
{
    char state_str[] = "State";
    bool requested_state;

    JSON_Object *jsonObject = json_value_get_object(json);
    if (jsonObject == NULL) {
        return DX_METHOD_FAILED;
    }

    requested_state = (bool)json_object_get_boolean(jsonObject, state_str);

    dx_gpioStateSet(&led, requested_state);

    return DX_METHOD_SUCCEEDED;
}

static void NetworkConnectionState(bool connected)
{
    dx_gpioStateSet(&network_led, connected);
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, IOT_PLUG_AND_PLAY_MODEL_ID);
    dx_gpioSetOpen(gpio_set, NELEMS(gpio_set));
    dx_timerSetStart(timers, NELEMS(timers));
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));
    dx_directMethodSubscribe(direct_method_bindings, NELEMS(direct_method_bindings));

    dx_azureRegisterConnectionChangedNotification(NetworkConnectionState);
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timers, NELEMS(timers));
    dx_deviceTwinUnsubscribe();
    dx_directMethodUnsubscribe();
    dx_gpioSetClose(gpio_set, NELEMS(gpio_set));
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