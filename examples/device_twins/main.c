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
#include "dx_config.h"
#include "dx_device_twins.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"

#include <applibs/log.h>
#include <time.h>

// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:com:example:azuresphere:labmonitor;1"

#define NETWORK_INTERFACE "wlan0"

// Number of bytes to allocate for the JSON telemetry message for IoT Central
#define JSON_MESSAGE_BYTES 256
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

#define LOCAL_DEVICE_TWIN_PROPERTY 64
static char copy_of_property_value[LOCAL_DEVICE_TWIN_PROPERTY];

DX_USER_CONFIG dx_config;

// Forward declarations
static void report_now_handler(EventLoopTimer *eventLoopTimer);
static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void dt_copy_string_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);
static void dt_gpio_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);

static DX_MESSAGE_PROPERTY *sensorErrorProperties[] = {
    &(DX_MESSAGE_PROPERTY){.key = "appid", .value = "hvac"},
    &(DX_MESSAGE_PROPERTY){.key = "type", .value = "SensorError"},
    &(DX_MESSAGE_PROPERTY){.key = "schema", .value = "1"}};

static DX_MESSAGE_CONTENT_PROPERTIES contentProperties = {.contentEncoding = "utf-8",
                                                          .contentType = "application/json"};

/****************************************************************************************
 * Bindings
 ****************************************************************************************/
static DX_TIMER_BINDING report_now_timer = {
    .period = {5, 0}, .name = "report_now_timer", .handler = report_now_handler};

static DX_GPIO_BINDING network_connected_led = {.pin = NETWORK_CONNECTED_LED,
                              .name = "network connected led",
                              .direction = DX_OUTPUT,
                              .initialState = GPIO_Value_Low,
                              .invertPin = true};

static DX_GPIO_BINDING userLedRed =   {.pin = LED_RED,   
                              .name = "userLedRed",   
                              .direction = DX_OUTPUT,  
                              .initialState = GPIO_Value_Low, 
                              .invertPin = true};

static DX_GPIO_BINDING userLedGreen = {.pin = LED_GREEN, 
                              .name = "userLedGreen", 
                              .direction = DX_OUTPUT,  
                              .initialState = GPIO_Value_Low, 
                              .invertPin = true};

static DX_GPIO_BINDING userLedBlue =  {.pin = LED_BLUE,  
                              .name = "userLedBlue",  
                              .direction = DX_OUTPUT,  
                              .initialState = GPIO_Value_Low, 
                              .invertPin = true};

// All bindings referenced in the bindings sets will be initialised in the InitPeripheralsAndHandlers function
DX_TIMER_BINDING *timer_binding_set[] = {&report_now_timer};
DX_GPIO_BINDING *gpio_binding_set[] = {&network_connected_led, &userLedRed, 
                                      &userLedGreen, &userLedBlue};

/****************************************************************************************
 * Azure IoT Device Twin Bindings
 ****************************************************************************************/
static DX_DEVICE_TWIN_BINDING dt_desired_sample_rate = {.propertyName = "DesiredSampleRate",
                                                        .twinType = DX_DEVICE_TWIN_INT,
                                                        .handler = dt_desired_sample_rate_handler};

static DX_DEVICE_TWIN_BINDING dt_sample_string = {.propertyName = "SampleString",
                                                  .twinType = DX_DEVICE_TWIN_STRING,
                                                  .handler = dt_copy_string_handler};

static DX_DEVICE_TWIN_BINDING dt_reported_temperature = {.propertyName = "ReportedTemperature",
                                                         .twinType = DX_DEVICE_TWIN_FLOAT};

static DX_DEVICE_TWIN_BINDING dt_reported_humidity = {.propertyName = "ReportedHumidity",
                                                      .twinType = DX_DEVICE_TWIN_DOUBLE};

static DX_DEVICE_TWIN_BINDING dt_reported_utc = {.propertyName = "ReportedUTC",
                                                 .twinType = DX_DEVICE_TWIN_STRING};

static DX_DEVICE_TWIN_BINDING dt_user_led_red = {.propertyName = "userLedRed",
                                                 .twinType = DX_DEVICE_TWIN_BOOL,  
                                                 .handler = dt_gpio_handler, 
                                                 .context = &userLedRed};	

static DX_DEVICE_TWIN_BINDING dt_user_led_green = {.propertyName = "userLedGreen",
                                                 .twinType = DX_DEVICE_TWIN_BOOL,
                                                 .handler = dt_gpio_handler, 
                                                 .context = &userLedGreen};

static DX_DEVICE_TWIN_BINDING dt_user_led_blue = {.propertyName = "userLedBlue",
                                                 .twinType = DX_DEVICE_TWIN_BOOL,
                                                 .handler = dt_gpio_handler,
                                                 .context = &userLedBlue};

// All device twins listed in device_twin_bindings will be subscribed to in
// the InitPeripheralsAndHandlers function
DX_DEVICE_TWIN_BINDING *device_twin_bindings[] = {&dt_desired_sample_rate, &dt_reported_temperature,
                                                  &dt_reported_humidity, &dt_reported_utc,
                                                  &dt_sample_string, &dt_user_led_red,
                                                  &dt_user_led_green, &dt_user_led_blue};


// Validate sensor readings and report device twins
static void report_now_handler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    if (!dx_isAzureConnected()) {
        return;
    }

    float temperature = 25.05f;
    // Add some random value to humidity to trigger data out of range error
    double humidity = 50.00 + (rand() % 70);

    // Validate sensor data to check within expected range
    // Is temperature between -20 and 60, is humidity between 0 and 100
    if (-20.0f < temperature && 60.0f > temperature && 0.0 <= humidity && 100.0 >= humidity) {

        // Update twin with current UTC (Universal Time Coordinate) in ISO format
        dx_deviceTwinReportValue(&dt_reported_utc, dx_getCurrentUtc(msgBuffer, sizeof(msgBuffer)));

        // The type passed in must match the Divice Twin Type DX_DEVICE_TWIN_FLOAT
        dx_deviceTwinReportValue(&dt_reported_temperature, &temperature);

        // The type passed in must match the Divice Twin Type DX_DEVICE_TWIN_DOUBLE
        dx_deviceTwinReportValue(&dt_reported_humidity, &humidity);

    } else { 
        // Report data not in range        
        if (dx_jsonSerialize(msgBuffer, sizeof(msgBuffer), 4, 
                            DX_JSON_STRING, "Sensor", "Environment", 
                            DX_JSON_STRING, "ErrorMessage", "Telemetry out of range", 
                            DX_JSON_FLOAT, "Temperature", temperature,
                            DX_JSON_DOUBLE, "Humidity", humidity)) {

            Log_Debug("%s\n", msgBuffer);

            // Publish sensor out of range error message.
            // The message metadata type property is set to SensorError.
            // Using IoT Hub Message Routing you would route the SensorError messages to a maintainance system.
            dx_azurePublish(msgBuffer, strlen(msgBuffer), sensorErrorProperties,
                            NELEMS(sensorErrorProperties), &contentProperties);
        }
    }
}

static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    // validate data is sensible range before applying
    if (deviceTwinBinding->twinType == DX_DEVICE_TWIN_INT &&
        *(int *)deviceTwinBinding->propertyValue >= 0 &&
        *(int *)deviceTwinBinding->propertyValue <= 120) {

        dx_timerChange(&report_now_timer,
                       &(struct timespec){*(int *)deviceTwinBinding->propertyValue, 0});

        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue,
                                     DX_DEVICE_TWIN_RESPONSE_COMPLETED);

    } else {
        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue,
                                     DX_DEVICE_TWIN_RESPONSE_ERROR);
    }

    /*	Casting device twin state examples

            float value = *(float*)deviceTwinBinding->property_value;
            double value = *(double*)deviceTwinBinding->property_value;
            int value = *(int*)deviceTwinBinding->property_value;
            bool value = *(bool*)deviceTwinBinding->property_value;
            char* value = (char*)deviceTwinBinding->property_value;
    */
}

// check string contain only printable characters
// ! " # $ % & ' ( ) * + , - . / 0 1 2 3 4 5 6 7 8 9 : ; < = > ? @ A B C D E F G H I J K L M N O P Q
// R S T U V W X Y Z [ \ ] ^ _ ` a b c d e f g h i j k l m n o p q r s t u v w x y z { | } ~
bool IsDataValid(char *data)
{
    while (isprint(*data)) {
        data++;
    }
    return 0x00 == *data;
}

// Sample device twin handler that demonstrates how to manage string device twin types.  When an
// application uses a string device twin, the application must make a local copy of the string on
// any device twin update. This gives you memory control as strings can be of arbitrary length.
static void dt_copy_string_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    char *property_value = (char *)deviceTwinBinding->propertyValue;

    // Validate data. Is data type string, size less than destination buffer and printable characters
    if (deviceTwinBinding->twinType == DX_DEVICE_TWIN_STRING &&
        strlen(property_value) < sizeof(copy_of_property_value) && IsDataValid(property_value)) {

        strncpy(copy_of_property_value, property_value, sizeof(copy_of_property_value));

        // Output the new string to debug
        Log_Debug("Rx device twin update for twin: %s, local value: %s\n",
                  deviceTwinBinding->propertyName, copy_of_property_value);

        dx_deviceTwinAckDesiredValue(deviceTwinBinding, (char *)deviceTwinBinding->propertyValue,
                                     DX_DEVICE_TWIN_RESPONSE_COMPLETED);

    } else {
        Log_Debug("Local copy failed. String too long or invalid data\n");
        dx_deviceTwinAckDesiredValue(deviceTwinBinding, (char *)deviceTwinBinding->propertyValue,
                                     DX_DEVICE_TWIN_RESPONSE_ERROR);
    }
}

// Set Network Connected state LED
static void ConnectionStatus(bool connection_state) {
    dx_gpioStateSet(&network_connected_led, connection_state);
}

static void dt_gpio_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    // Verify that the context pointer is non-null
    if(deviceTwinBinding->context != NULL){
        
        // Cast the context pointer so we can access the GPIO Binding details
        DX_GPIO_BINDING *gpio = (DX_GPIO_BINDING*)deviceTwinBinding->context;
        
        bool gpio_level = *(bool *)deviceTwinBinding->propertyValue;
    
        if(gpio_level){
            dx_gpioOn(gpio);
        }
        else{
            dx_gpioOff(gpio);
        }
        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_COMPLETED);
    }
    else{
        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_ERROR);
    }
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, IOT_PLUG_AND_PLAY_MODEL_ID);
    dx_timerSetStart(timer_binding_set, NELEMS(timer_binding_set));
    dx_gpioSetOpen(gpio_binding_set, NELEMS(gpio_binding_set));
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));

    dx_azureRegisterConnectionChangedNotification(ConnectionStatus);
    // Init the random number generator. Used to synthesis humidity telemetry
    srand((unsigned int)time(NULL));
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_deviceTwinUnsubscribe();
    dx_timerSetStop(timer_binding_set, NELEMS(timer_binding_set));
    dx_gpioSetClose(gpio_binding_set, NELEMS(gpio_binding_set));
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
    return dx_getTerminationExitCode();
}