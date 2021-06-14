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

/****************************************************************************************
 * Forward declarations
 ****************************************************************************************/
static void AzureIotConnectedLed_handler(EventLoopTimer *eventLoopTimer);
static void ButtonA_handler(EventLoopTimer *eventLoopTimer);

/****************************************************************************************
* Timer Bindings
****************************************************************************************/
static DX_TIMER_BINDING  tmr_AzureIotConnectedLed = { .period = {4,0}, .name = "AzureIotConnectedLed", .handler = AzureIotConnectedLed_handler };
static DX_TIMER_BINDING  tmr_ButtonA = { .period = {0,200000000}, .name = "ButtonA", .handler = ButtonA_handler };

// All timers referenced in timer_bindings with be opened in the InitPeripheralsAndHandlers function
static DX_TIMER_BINDING *timer_bindings[] = {  &AzureIotConnectedLed, &ButtonA };

/****************************************************************************************
* Azure IoT Device Twin Bindings
****************************************************************************************/

/****************************************************************************************
* Azure IoT Direct Method Bindings
****************************************************************************************/

/****************************************************************************************
* GPIO Bindings
****************************************************************************************/
static DX_GPIO_BINDING gpio_AzureIotConnectedLed = { .pin = NETWORK_CONNECTED_LED, .name = "AzureIotConnectedLed", .direction = "DX_GPIO_OUTPUT", .initialState = GPIO_Value_Low, .invertPin = true };
static DX_GPIO_BINDING gpio_ButtonA = { .pin = BUTTON_A, .name = "ButtonA", direction = "DX_GPIO_INPUT" };

// All GPIOs referenced in gpio_bindings with be opened in the InitPeripheralsAndHandlers function
static DX_GPIO_BINDING *gpio_bindings[] = {  &AzureIotConnectedLed, &ButtonA };


/****************************************************************************************
* Implement your gpio code
****************************************************************************************/

/// <summary>
/// Implement your timer function
/// </summary>
static void AzureIotConnectedLed_handler(EventLoopTimer *eventLoopTimer) {
    static bool gpio_state = true;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }
    
    dx_gpioStateSet(&gpio_AzureIotConnectedLed, gpio_state);    
    gpio_state = !gpio_state;
}

/// <summary>
/// Implement your GPIO input timer function
/// </summary>
static void ButtonA_handler(EventLoopTimer *eventLoopTimer) {
	static GPIO_Value_Type gpio_ButtonANewState;

	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	if (dx_gpioStateGet(&gpio_ButtonA, &gpio_ButtonANewState)) {
		Log_Debug("gpio_ButtonA: %d\n", gpio_ButtonANewState);
	}
}

