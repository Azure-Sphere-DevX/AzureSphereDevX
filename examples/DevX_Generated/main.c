/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 *   DISCLAIMER
 *
 *   The DevX library supports the Azure Sphere Developer Learning Path:
 *
 *	   1. are built from the Azure Sphere SDK Samples at https://github.com/Azure/azure-sphere-samples
 *	   2. are not intended as a substitute for understanding the Azure Sphere SDK Samples.
 *	   3. aim to follow best practices as demonstrated by the Azure Sphere SDK Samples.
 *	   4. are provided as is and as a convenience to aid the Azure Sphere Developer Learning experience.
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
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_version.h"

#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define APPLICATION_VERSION "1.0"
#define PNP_MODEL_ID "dtmi:com:example:application;1"
#define NETWORK_INTERFACE "wlan0"
#define OneMS 1000000 // used to simplify timer defn.

#define Log_Debug(f_, ...) dx_Log_Debug((f_), ##__VA_ARGS__)
static char Log_Debug_buffer[128];

/****************************************************************************************
 * Watchdog support
 ****************************************************************************************/
const struct itimerspec watchdogInterval = {{60, 0}, {60, 0}};
timer_t watchdogTimer;

/****************************************************************************************
 * Telemetry message templates and property sets
 ****************************************************************************************/

DX_USER_CONFIG dx_config; // cmd args from app_manifest.json

// Number of bytes to allocate for the JSON telemetry message for IoT Hub/Central
#define JSON_MESSAGE_BYTES 256
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

static const char *msgTemplate = "{ \"Temperature\":%3.2f, \"Humidity\":%3.1f, \"Pressure\":%3.1f }";

static DX_MESSAGE_PROPERTY *messageProperties[] = {&(DX_MESSAGE_PROPERTY){.key = "appid", .value = "hvac"},
                                                   &(DX_MESSAGE_PROPERTY){.key = "type", .value = "telemetry"},
                                                   &(DX_MESSAGE_PROPERTY){.key = "schema", .value = "1"}};

static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8", .contentType = "application/json"};

/****************************************************************************************
 * Forward declarations
 ****************************************************************************************/
static DX_DIRECT_METHOD_RESPONSE_CODE FanControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static DX_DIRECT_METHOD_RESPONSE_CODE LightControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void AzureIotConnectedLed_handler(EventLoopTimer *eventLoopTimer);
static void DesiredMeasurementRate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void DesiredTemperature_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void PublishTelemetry_handler(EventLoopTimer *eventLoopTimer);
static void Watchdog_handler(EventLoopTimer *eventLoopTimer);

/****************************************************************************************
 * Azure IoT Device Twin Bindings
 ****************************************************************************************/
static DX_DEVICE_TWIN_BINDING dt_DesiredMeasurementRate = {
    .twinProperty = "DesiredMeasurementRate", .twinType = DX_TYPE_DOUBLE, .handler = DesiredMeasurementRate_handler};
static DX_DEVICE_TWIN_BINDING dt_DesiredTemperature = {.twinProperty = "DesiredTemperature", .twinType = DX_TYPE_FLOAT, .handler = DesiredTemperature_handler};
static DX_DEVICE_TWIN_BINDING dt_ReportedDeviceStartTime = {.twinProperty = "ReportedDeviceStartTime", .twinType = DX_TYPE_STRING};
static DX_DEVICE_TWIN_BINDING dt_ReportedTemperature = {.twinProperty = "ReportedTemperature", .twinType = DX_TYPE_FLOAT};

// All device twins listed in device_twin_bindings will be subscribed to in the InitPeripheralsAndHandlers function
static DX_DEVICE_TWIN_BINDING *device_twin_bindings[] = {&dt_DesiredMeasurementRate, &dt_DesiredTemperature, &dt_ReportedDeviceStartTime,
                                                         &dt_ReportedTemperature};

/****************************************************************************************
 * Azure IoT Direct Method Bindings
 ****************************************************************************************/
static DX_DIRECT_METHOD_BINDING dm_FanControl = {.methodName = "FanControl", .handler = FanControl_handler};
static DX_DIRECT_METHOD_BINDING dm_LightControl = {.methodName = "LightControl", .handler = LightControl_handler};

// All direct methods referenced in direct_method_bindings will be subscribed to in the InitPeripheralsAndHandlers function
static DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {&dm_FanControl, &dm_LightControl};

/****************************************************************************************
 * Timer Bindings
 ****************************************************************************************/
static DX_TIMER_BINDING tmr_AzureIotConnectedLed = {.period = {4, 0}, .name = "AzureIotConnectedLed", .handler = AzureIotConnectedLed_handler};
static DX_TIMER_BINDING tmr_PublishTelemetry = {.period = {5, 0}, .name = "PublishTelemetry", .handler = PublishTelemetry_handler};
static DX_TIMER_BINDING tmr_Watchdog = {.period = {15, 0}, .name = "Watchdog", .handler = Watchdog_handler};

// All timers referenced in timer_bindings with be opened in the InitPeripheralsAndHandlers function
static DX_TIMER_BINDING *timer_bindings[] = {&tmr_AzureIotConnectedLed, &tmr_PublishTelemetry, &tmr_Watchdog};

/****************************************************************************************
 * GPIO Bindings
 ****************************************************************************************/
static DX_GPIO_BINDING gpio_AzureIotConnectedLed = {
    .pin = NETWORK_CONNECTED_LED, .name = "AzureIotConnectedLed", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true};

// All GPIOs referenced in gpio_bindings with be opened in the InitPeripheralsAndHandlers function
static DX_GPIO_BINDING *gpio_bindings[] = {&gpio_AzureIotConnectedLed};

/****************************************************************************************
 * Implement your device twins code
 ****************************************************************************************/

/// <summary>
/// What is the purpose of this device twin handler function
/// </summary>
/// <param name="deviceTwinBinding"></param>
static void DesiredMeasurementRate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    Log_Debug("Device Twin Property Name: %s\n", deviceTwinBinding->twinProperty);

    // Checking the twinStateUpdated here will always be true.
    // But it's useful property for other areas of your code
    Log_Debug("Device Twin state updated %s\n", deviceTwinBinding->twinStateUpdated ? "true" : "false");

    double device_twin_value = *(double *)deviceTwinBinding->twinState;

    if (device_twin_value > 0.0 && device_twin_value < 100.0) {
        Log_Debug("Device twin value: %f\n", device_twin_value);
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
    } else {
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_ERROR);
    }
}

/// <summary>
/// What is the purpose of this device twin handler function
/// </summary>
/// <param name="deviceTwinBinding"></param>
static void DesiredTemperature_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    Log_Debug("Device Twin Property Name: %s\n", deviceTwinBinding->twinProperty);

    // Checking the twinStateUpdated here will always be true.
    // But it's useful property for other areas of your code
    Log_Debug("Device Twin state updated %s\n", deviceTwinBinding->twinStateUpdated ? "true" : "false");

    float device_twin_value = *(float *)deviceTwinBinding->twinState;

    if (device_twin_value > 0.0f && device_twin_value < 100.0f) {
        Log_Debug("Device twin value: %f\n", device_twin_value);
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
    } else {
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_ERROR);
    }
}

/****************************************************************************************
 * Implement your direct method code
 ****************************************************************************************/

// Direct method JSON payload example {"Duration":2}:
static DX_DIRECT_METHOD_RESPONSE_CODE FanControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    char duration_str[] = "Duration";
    int requested_duration_seconds;

    JSON_Object *jsonObject = json_value_get_object(json);
    if (jsonObject == NULL) {
        return DX_METHOD_FAILED;
    }

    // check JSON properties sent through are the correct type
    if (!json_object_has_value_of_type(jsonObject, duration_str, JSONNumber)) {
        return DX_METHOD_FAILED;
    }

    requested_duration_seconds = (int)json_object_get_number(jsonObject, duration_str);
    Log_Debug("Duration %d \n", requested_duration_seconds);

    if (requested_duration_seconds < 0 || requested_duration_seconds > 120) {
        return DX_METHOD_FAILED;
    }

    // Action the duration request

    return DX_METHOD_SUCCEEDED;
}

// Direct method JSON payload example {"Duration":2}:
static DX_DIRECT_METHOD_RESPONSE_CODE LightControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    char duration_str[] = "Duration";
    int requested_duration_seconds;

    JSON_Object *jsonObject = json_value_get_object(json);
    if (jsonObject == NULL) {
        return DX_METHOD_FAILED;
    }

    // check JSON properties sent through are the correct type
    if (!json_object_has_value_of_type(jsonObject, duration_str, JSONNumber)) {
        return DX_METHOD_FAILED;
    }

    requested_duration_seconds = (int)json_object_get_number(jsonObject, duration_str);
    Log_Debug("Duration %d \n", requested_duration_seconds);

    if (requested_duration_seconds < 0 || requested_duration_seconds > 120) {
        return DX_METHOD_FAILED;
    }

    // Action the duration request

    return DX_METHOD_SUCCEEDED;
}

/****************************************************************************************
 * Implement your timer code
 ****************************************************************************************/

/// <summary>
/// Implement your timer function
/// </summary>
static void AzureIotConnectedLed_handler(EventLoopTimer *eventLoopTimer)
{
    static bool gpio_state = true;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    dx_gpioStateSet(&gpio_AzureIotConnectedLed, gpio_state);
    gpio_state = !gpio_state;
}

/// <summary>
/// Publish environment sensor telemetry to IoT Hub/Central
/// </summary>
/// <param name="eventLoopTimer"></param>
static void PublishTelemetry_handler(EventLoopTimer *eventLoopTimer)
{

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    float DesiredTemperature_value = 10.0f;
    double DesiredMeasurementRate_value = 10.0;
    float ReportedTemperature_value = 10.0f;
    char ReportedDeviceStartTime_value[] = "hello, world";

    if (dx_isAzureConnected()) {
        // Create JSON telemetry message
        if (snprintf(msgBuffer, sizeof(msgBuffer), msgTemplate, 30.0, 60.0, 1010.0) > 0) {
            Log_Debug("%s\n", msgBuffer);
            dx_azurePublish(msgBuffer, strlen(msgBuffer), messageProperties, NELEMS(messageProperties), &contentProperties);
        }

        dx_deviceTwinReportState(&dt_DesiredTemperature, &DesiredTemperature_value);
        dx_deviceTwinReportState(&dt_DesiredMeasurementRate, &DesiredMeasurementRate_value);
        dx_deviceTwinReportState(&dt_ReportedTemperature, &ReportedTemperature_value);
        dx_deviceTwinReportState(&dt_ReportedDeviceStartTime, ReportedDeviceStartTime_value);
    }
}

/// <summary>
/// This timer extends the app level lease watchdog timer
/// </summary>
/// <param name="eventLoopTimer"></param>
static void Watchdog_handler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }
    timer_settime(watchdogTimer, 0, &watchdogInterval, NULL);
}

/// <summary>
/// Set up watchdog timer - the lease is extended via the Watchdog_handler function
/// </summary>
/// <param name=""></param>
void StartWatchdog(void)
{
    struct sigevent alarmEvent;
    alarmEvent.sigev_notify = SIGEV_SIGNAL;
    alarmEvent.sigev_signo = SIGALRM;
    alarmEvent.sigev_value.sival_ptr = &watchdogTimer;

    if (timer_create(CLOCK_MONOTONIC, &alarmEvent, &watchdogTimer) == 0) {
        if (timer_settime(watchdogTimer, 0, &watchdogInterval, NULL) == -1) {
            Log_Debug("Issue setting watchdog timer. %s %d\n", strerror(errno), errno);
        }
    }
}

/// <summary>
///  Initialize gpios, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralAndHandlers(void)
{
    dx_Log_Debug_Init(Log_Debug_buffer, sizeof(Log_Debug_buffer));
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, PNP_MODEL_ID);
    dx_gpioSetOpen(gpio_bindings, NELEMS(gpio_bindings));
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));
    dx_directMethodSubscribe(direct_method_bindings, NELEMS(direct_method_bindings));
    dx_timerSetStart(timer_bindings, NELEMS(timer_bindings));

    // Uncomment the StartWatchdog when ready for production
    // StartWatchdog();
}

/// <summary>
///     Close Timers, GPIOs, Device Twins, Direct Methods
/// </summary>
static void ClosePeripheralAndHandlers(void)
{
    dx_timerSetStop(timer_bindings, NELEMS(timer_bindings));
    dx_azureToDeviceStop();
    dx_gpioSetClose(gpio_bindings, NELEMS(gpio_bindings));
    dx_deviceTwinUnsubscribe();
    dx_directMethodUnsubscribe();
    dx_timerEventLoopStop();
}

/// <summary>
///  Main event loop for the app
/// </summary>
int main(int argc, char *argv[])
{
    dx_registerTerminationHandler();

    if (!dx_configParseCmdLineArguments(argc, argv, &dx_config)) {
        return dx_getTerminationExitCode();
    }

    InitPeripheralAndHandlers();

    // Main loop
    while (!dx_isTerminationRequired()) {
        int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == -1 && errno != EINTR) {
            dx_terminate(DX_ExitCode_Main_EventLoopFail);
        }
    }

    ClosePeripheralAndHandlers();
    return dx_getTerminationExitCode();
}