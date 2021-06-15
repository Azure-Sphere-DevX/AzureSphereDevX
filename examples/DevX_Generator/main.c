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

#include "declarations.h"

#include "applibs_versions.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define APPLICATION_VERSION "1.0"
#define PNP_MODEL_ID "dtmi:com:example:application;1"
#define NETWORK_INTERFACE "wlan0"
#define OneMS 1000000		// used to simplify timer defn.

#define Log_Debug(f_, ...) dx_Log_Debug((f_), ##__VA_ARGS__)
static char Log_Debug_buffer[128];

/****************************************************************************************
 * Watchdog support
 ****************************************************************************************/
const struct itimerspec watchdogInterval = { { 60, 0 },{ 60, 0 } };
timer_t watchdogTimer;

/****************************************************************************************
 * Telemetry message templates and property sets
 ****************************************************************************************/

DX_USER_CONFIG dx_config;   // cmd args from app_manifest.json

// Number of bytes to allocate for the JSON telemetry message for IoT Hub/Central
#define JSON_MESSAGE_BYTES 256
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

static const char *msgTemplate =
    "{ \"Temperature\":%3.2f, \"Humidity\":%3.1f, \"Pressure\":%3.1f }";

static DX_MESSAGE_PROPERTY *messageProperties[] = {
    &(DX_MESSAGE_PROPERTY){.key = "appid", .value = "hvac"},
    &(DX_MESSAGE_PROPERTY){.key = "type", .value = "telemetry"},
    &(DX_MESSAGE_PROPERTY){.key = "schema", .value = "1"}};

static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8", .contentType = "application/json"};


static DX_DIRECT_METHOD_RESPONSE_CODE FanControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static DX_DIRECT_METHOD_RESPONSE_CODE LightControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void AlertLed_handler(EventLoopTimer *eventLoopTimer);
static void AzureIotConnectedLed_handler(EventLoopTimer *eventLoopTimer);
static void ButtonA_handler(EventLoopTimer *eventLoopTimer);
static void DesiredMeasurementRate_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static void DesiredTemperature_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static void MeasureCarbonMonoxide_handler(EventLoopTimer *eventLoopTimer);
static void MeasureTemperature_handler(EventLoopTimer *eventLoopTimer);
static void PublishTelemetry_handler(EventLoopTimer *eventLoopTimer);
static void Watchdog_handler(EventLoopTimer *eventLoopTimer);

/****************************************************************************************
* Azure IoT Device Twin Bindings
****************************************************************************************/
static DX_DEVICE_TWIN_BINDING dt_DesiredMeasurementRate = { .twinProperty = "DesiredMeasurementRate", .twinType = DX_TYPE_DOUBLE, .handler = DesiredMeasurementRate_handler};
static DX_DEVICE_TWIN_BINDING dt_DesiredTemperature = { .twinProperty = "DesiredTemperature", .twinType = DX_TYPE_FLOAT, .handler = DesiredTemperature_handler};
static DX_DEVICE_TWIN_BINDING dt_ReportedDeviceStartTime = { .twinProperty = "ReportedDeviceStartTime", .twinType = DX_TYPE_STRING};
static DX_DEVICE_TWIN_BINDING dt_ReportedTemperature = { .twinProperty = "ReportedTemperature", .twinType = DX_TYPE_FLOAT};

// All device twins listed in device_twin_bindings will be subscribed to in the InitPeripheralsAndHandlers function
static DX_DEVICE_TWIN_BINDING* device_twin_bindings[] = { &dt_DesiredMeasurementRate, &dt_DesiredTemperature, &dt_ReportedDeviceStartTime, &dt_ReportedTemperature };

/****************************************************************************************
* Azure IoT Direct Method Bindings
****************************************************************************************/
static DX_DIRECT_METHOD_BINDING dm_FanControl = { .methodName = "FanControl", .handler = FanControl_handler };
static DX_DIRECT_METHOD_BINDING dm_LightControl = { .methodName = "LightControl", .handler = LightControl_handler };

// All direct methods referenced in direct_method_bindings will be subscribed to in the InitPeripheralsAndHandlers function
static DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = { &dm_FanControl, &dm_LightControl };

/****************************************************************************************
* Timer Bindings
****************************************************************************************/
static DX_TIMER_BINDING  tmr_AlertLed = {.period = {4, 0},  .name = "AlertLed", .handler = AlertLed_handler };
static DX_TIMER_BINDING  tmr_AzureIotConnectedLed = {.period = {4, 0},  .name = "AzureIotConnectedLed", .handler = AzureIotConnectedLed_handler };
static DX_TIMER_BINDING  tmr_ButtonA = {.period = {0, 200000000},  .name = "ButtonA", .handler = ButtonA_handler };
static DX_TIMER_BINDING  tmr_MeasureCarbonMonoxide = { .name = "MeasureCarbonMonoxide", .handler = MeasureCarbonMonoxide_handler };
static DX_TIMER_BINDING  tmr_MeasureTemperature = { .period = {5, 0}, .name = "MeasureTemperature", .handler = MeasureTemperature_handler };
static DX_TIMER_BINDING  tmr_PublishTelemetry = {.period = {5, 0}, .name = "PublishTelemetry", .handler = PublishTelemetry_handler };
static DX_TIMER_BINDING  tmr_Watchdog = {.period = {15, 0}, .name = "Watchdog", .handler = Watchdog_handler };

// All timers referenced in timer_bindings with be opened in the InitPeripheralsAndHandlers function
static DX_TIMER_BINDING *timer_bindings[] = { &tmr_AlertLed, &tmr_AzureIotConnectedLed, &tmr_ButtonA, &tmr_MeasureCarbonMonoxide, &tmr_MeasureTemperature, &tmr_PublishTelemetry, &tmr_Watchdog };

/****************************************************************************************
* GPIO Bindings
****************************************************************************************/
static DX_GPIO_BINDING gpio_AlertLed = { .pin = ALERT_LED, .name = "AlertLed", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true };
static DX_GPIO_BINDING gpio_AzureIotConnectedLed = { .pin = NETWORK_CONNECTED_LED, .name = "AzureIotConnectedLed", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true };
static DX_GPIO_BINDING gpio_ButtonA = { .pin = BUTTON_A, .name = "ButtonA", .direction = DX_INPUT };

// All GPIOs referenced in gpio_bindings with be opened in the InitPeripheralsAndHandlers function
static DX_GPIO_BINDING *gpio_bindings[] = { &gpio_AlertLed, &gpio_AzureIotConnectedLed, &gpio_ButtonA };



/****************************************************************************************
* Implement your device twins code
****************************************************************************************/

