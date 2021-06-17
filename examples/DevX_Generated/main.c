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
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define APPLICATION_VERSION "1.0"
const char PNP_MODEL_ID[] = "dtmi:com:example:application;1";
const char NETWORK_INTERFACE[] = "wlan0";



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

    dx_gpioStateSet(&gpio_AzureIotConnectedLed, gpio_state = !gpio_state);
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



/****************************************************************************************
* Implement your device twins code
****************************************************************************************/

/// <summary>
/// What is the purpose of this device twin handler function?
/// </summary>
/// <param name="deviceTwinBinding"></param>
static void DesiredAlertlevel_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
    Log_Debug("Device Twin Property Name: %s\n", deviceTwinBinding->twinProperty);

    // Checking the twinStateUpdated here will always be true.
    // But it's useful property for other areas of your code.
    Log_Debug("Device Twin state updated %s\n", deviceTwinBinding->twinStateUpdated ? "true" : "false");

    double device_twin_value = *(double*)deviceTwinBinding->twinState;

    if (device_twin_value > 0.0 && device_twin_value < 100.0){
        Log_Debug("Device twin value: %f\n", device_twin_value);
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
    } else {
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_ERROR);
    }
}

/// <summary>
/// What is the purpose of this device twin handler function?
/// </summary>
/// <param name="deviceTwinBinding"></param>
static void DesiredHumidity_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
    Log_Debug("Device Twin Property Name: %s\n", deviceTwinBinding->twinProperty);

    // Checking the twinStateUpdated here will always be true.
    // But it's useful property for other areas of your code.
    Log_Debug("Device Twin state updated %s\n", deviceTwinBinding->twinStateUpdated ? "true" : "false");

    float device_twin_value = *(float*)deviceTwinBinding->twinState;

    if (device_twin_value > 0.0f && device_twin_value < 100.0f){
        Log_Debug("Device twin value: %f\n", device_twin_value);
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
    } else {
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_ERROR);
    }
}

/// <summary>
/// What is the purpose of this device twin handler function?
/// </summary>
/// <param name="deviceTwinBinding"></param>
static void DesiredTemperature_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
    Log_Debug("Device Twin Property Name: %s\n", deviceTwinBinding->twinProperty);

    // Checking the twinStateUpdated here will always be true.
    // But it's useful property for other areas of your code.
    Log_Debug("Device Twin state updated %s\n", deviceTwinBinding->twinStateUpdated ? "true" : "false");

    float device_twin_value = *(float*)deviceTwinBinding->twinState;

    if (device_twin_value > 0.0f && device_twin_value < 100.0f){
        Log_Debug("Device twin value: %f\n", device_twin_value);
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
    } else {
        dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_ERROR);
    }
}



/****************************************************************************************
* Implement your direct method code
****************************************************************************************/

/// <summary>
/// What is the purpose of this direct method handler function?
/// </summary>
// Direct method JSON payload example {"Duration":2}:
static DX_DIRECT_METHOD_RESPONSE_CODE FanOff_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg) {
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

    if (requested_duration_seconds < 0 || requested_duration_seconds > 120 ) {
        return DX_METHOD_FAILED;
    }

    // Action the duration request

    return DX_METHOD_SUCCEEDED;
}

/// <summary>
/// What is the purpose of this direct method handler function?
/// </summary>
// Direct method JSON payload example {"Duration":2}:
static DX_DIRECT_METHOD_RESPONSE_CODE LightControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg) {
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

    if (requested_duration_seconds < 0 || requested_duration_seconds > 120 ) {
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
static void MeasureCarbonMonoxide_handler(EventLoopTimer *eventLoopTimer) {
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    Log_Debug("Periodic timer MeasureCarbonMonoxide_handler called\n");

    // Implement your timer function
}

/// <summary>
/// Implement your timer function
/// </summary>
static void MeasureTemperature_handler(EventLoopTimer *eventLoopTimer) {
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    Log_Debug("Periodic timer MeasureTemperature_handler called\n");

    // Implement your timer function
}



/****************************************************************************************
* Implement general code
****************************************************************************************/

/// <summary>
/// Publish environment sensor telemetry to IoT Hub/Central
/// </summary>
/// <param name="eventLoopTimer"></param>
static void PublishTelemetry_handler(EventLoopTimer *eventLoopTimer) {

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    float temperature = 30.0f;
    float humidity = 60.0f;
    float pressure = 1010.0f;

    float DesiredTemperature_value = 30.0f;
    float DesiredHumidity_value = 30.0f;
    double DesiredAlertlevel_value = 10.0;
    float ReportedTemperature_value = 30.0f;

    if (dx_isAzureConnected()) {
        // Create JSON telemetry message
        if (snprintf(msgBuffer, sizeof(msgBuffer), msgTemplate, temperature, humidity, pressure) > 0) {
            Log_Debug("%s\n", msgBuffer);
            dx_azurePublish(msgBuffer, strlen(msgBuffer), messageProperties, NELEMS(messageProperties), &contentProperties);
        }
        dx_deviceTwinReportState(&dt_DesiredTemperature, &DesiredTemperature_value);     // DX_TYPE_FLOAT
        dx_deviceTwinReportState(&dt_DesiredHumidity, &DesiredHumidity_value);     // DX_TYPE_FLOAT
        dx_deviceTwinReportState(&dt_DesiredAlertlevel, &DesiredAlertlevel_value);     // DX_TYPE_DOUBLE
        dx_deviceTwinReportState(&dt_ReportedTemperature, &ReportedTemperature_value);     // DX_TYPE_FLOAT
    }
}


/// <summary>
/// This timer extends the app level lease watchdog timer
/// </summary>
/// <param name="eventLoopTimer"></param>
static void Watchdog_handler(EventLoopTimer *eventLoopTimer) {
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
void StartWatchdog(void) {
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
static void InitPeripheralAndHandlers(void) {{
	dx_Log_Debug_Init(Log_Debug_buffer, sizeof(Log_Debug_buffer));
	dx_initPeripheralAndHandlers();

	// Uncomment the StartWatchdog when ready for production
	// StartWatchdog();
}}

/// <summary>
///     Close Timers, GPIOs, Device Twins, Direct Methods
/// </summary>
static void ClosePeripheralAndHandlers(void) {{
	dx_azureToDeviceStop();
	dx_closePeripheralAndHandlers();
	dx_timerEventLoopStop();
}}

/// <summary>
///  Main event loop for the app
/// </summary>
int main(int argc, char* argv[]) {{
	dx_registerTerminationHandler();
	if (!dx_configParseCmdLineArguments(argc, argv, &dx_config)) {{
		return dx_getTerminationExitCode();
	}}

	InitPeripheralAndHandlers();

	// Main loop
	while (!dx_isTerminationRequired()) {{
		int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
		// Continue if interrupted by signal, e.g. due to breakpoint being set.
		if (result == -1 && errno != EINTR) {{
			dx_terminate(DX_ExitCode_Main_EventLoopFail);
		}}
	}}

	ClosePeripheralAndHandlers();
	return dx_getTerminationExitCode();
}}