/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * This example is built on the Azure Sphere DevX library.
 *   1. DevX is an Open Source community-maintained implementation of the Azure Sphere SDK samples.
 *   2. DevX is a modular library that simplifies common development scenarios.
 *        - You can focus on your solution, not the plumbing.
 *   3. DevX documentation is maintained at https://github.com/gloveboxes/AzureSphereDevX/wiki
 *	 4. The DevX library is not a substitute for understanding the Azure Sphere SDK Samples.
 *          - https://github.com/Azure/azure-sphere-samples
 *
 * DEVELOPER BOARD SELECTION
 *
 * The following developer boards are supported.
 *
 *	 1. AVNET Azure Sphere Starter Kit.
 *   2. AVNET Azure Sphere Starter Kit Revision 2.
 *	 3. Seeed Studio Azure Sphere MT3620 Development Kit aka Reference Design Board or rdb.
 *	 4. Seeed Studio Seeed Studio MT3620 Mini Dev Board.
 *
 * ENABLE YOUR DEVELOPER BOARD
 *
 * Each Azure Sphere developer board manufacturer maps pins differently. You need to select the
 *    configuration that matches your board.
 *
 * Follow these steps:
 *
 *	 1. Open CMakeLists.txt.
 *	 2. Uncomment the set command that matches your developer board.
 *	 3. Click File, then Save to auto-generate the CMake Cache.
 *
 ************************************************************************************************/

#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "app_exit_codes.h"
#include "dx_azure_iot.h"
#include "dx_azure_iot.h"
#include "dx_config.h"
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

static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8", .contentType = "application/json"};

/****************************************************************************************
 * Timer Bindings
 ****************************************************************************************/
static DX_TIMER_BINDING publish_message = {.period = {5, 0}, .name = "publish_message", .handler = publish_message_handler};

// All timers referenced in timers with be opened in the InitPeripheralsAndHandlers function
DX_TIMER_BINDING *timers[] = {&publish_message};

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
            dx_terminate(APP_ExitCode_Telemetry_Buffer_Too_Small);
        }
    }
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, IOT_PLUG_AND_PLAY_MODEL_ID);
    dx_timerSetStart(timers, NELEMS(timers));
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timers, NELEMS(timers));
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