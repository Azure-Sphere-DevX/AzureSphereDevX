#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_version.h"

#include <time.h>

DX_USER_CONFIG dx_config;   // cmd args from app_manifest.json

extern const char PNP_MODEL_ID[];
extern const char NETWORK_INTERFACE[];

#define ONE_MS 1000000		// used to simplify timer defn.
#define Log_Debug(f_, ...) dx_Log_Debug((f_), ##__VA_ARGS__)
static char Log_Debug_buffer[128];

/****************************************************************************************
 * Watchdog support
 ****************************************************************************************/
const struct itimerspec watchdogInterval = { { 60, 0 },{ 60, 0 } };
timer_t watchdogTimer;

/****************************************************************************************
 * Forward declarations
 ****************************************************************************************/
static void AzureIotConnectedLed_handler(EventLoopTimer *eventLoopTimer);
static void ButtonA_handler(EventLoopTimer *eventLoopTimer);
static void DesiredAlertlevel_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static void DesiredHumidity_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static void DesiredTemperature_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static DX_DIRECT_METHOD_RESPONSE_CODE FanOff_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static DX_DIRECT_METHOD_RESPONSE_CODE LightControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void MeasureCarbonMonoxide_handler(EventLoopTimer *eventLoopTimer);
static void MeasureTemperature_handler(EventLoopTimer *eventLoopTimer);
static void PublishTelemetry_handler(EventLoopTimer *eventLoopTimer);
static void Watchdog_handler(EventLoopTimer *eventLoopTimer);

/****************************************************************************************
* General declaratons
****************************************************************************************/
/****************************************************************************************
 * Telemetry message templates and property sets
 ****************************************************************************************/
#define DX_PUBLISH_TELEMETRY TRUE

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
* Timer Bindings
****************************************************************************************/
static DX_TIMER_BINDING  tmr_AzureIotConnectedLed = { .period = {0,250 * ONE_MS}, .name = "AzureIotConnectedLed", .handler = AzureIotConnectedLed_handler };
static DX_TIMER_BINDING  tmr_ButtonA = { .period = {0,200000000}, .name = "ButtonA", .handler = ButtonA_handler };
static DX_TIMER_BINDING  tmr_MeasureCarbonMonoxide = { .period = { 5, 0 }, .name = "MeasureCarbonMonoxide", .handler = MeasureCarbonMonoxide_handler };
static DX_TIMER_BINDING  tmr_MeasureTemperature = { .period = { 5, 0 }, .name = "MeasureTemperature", .handler = MeasureTemperature_handler };
static DX_TIMER_BINDING  tmr_PublishTelemetry = { .period = {15,0}, .name = "PublishTelemetry", .handler = PublishTelemetry_handler };
static DX_TIMER_BINDING  tmr_Watchdog = { .period = {30, 0}, .name = "Watchdog", .handler = Watchdog_handler };

// All timers referenced in timer_bindings with be opened in the InitPeripheralsAndHandlers function
#define DECLARE_DX_TIMER_BINDINGS
static DX_TIMER_BINDING *timer_bindings[] = {  &tmr_AzureIotConnectedLed, &tmr_ButtonA, &tmr_MeasureCarbonMonoxide, &tmr_MeasureTemperature, &tmr_PublishTelemetry, &tmr_Watchdog };

/****************************************************************************************
* Azure IoT Device Twin Bindings
****************************************************************************************/
static DX_DEVICE_TWIN_BINDING dt_DesiredAlertlevel = { .twinProperty = "DesiredAlertlevel", .twinType = DX_TYPE_DOUBLE, .handler = DesiredAlertlevel_handler };
static DX_DEVICE_TWIN_BINDING dt_DesiredHumidity = { .twinProperty = "DesiredHumidity", .twinType = DX_TYPE_FLOAT, .handler = DesiredHumidity_handler };
static DX_DEVICE_TWIN_BINDING dt_DesiredTemperature = { .twinProperty = "DesiredTemperature", .twinType = DX_TYPE_FLOAT, .handler = DesiredTemperature_handler };
static DX_DEVICE_TWIN_BINDING dt_ReportedTemperature = { .twinProperty = "ReportedTemperature", .twinType = DX_TYPE_FLOAT };

// All direct methods referenced in direct_method_bindings will be subscribed to in the InitPeripheralsAndHandlers function
#define DECLARE_DX_DEVICE_TWIN_BINDINGS
static DX_DEVICE_TWIN_BINDING* device_twin_bindings[] = {  &dt_DesiredAlertlevel, &dt_DesiredHumidity, &dt_DesiredTemperature, &dt_ReportedTemperature };

/****************************************************************************************
* Azure IoT Direct Method Bindings
****************************************************************************************/
static DX_DIRECT_METHOD_BINDING dm_FanOff = { .methodName = "FanOff", .handler = FanOff_handler };
static DX_DIRECT_METHOD_BINDING dm_LightControl = { .methodName = "LightControl", .handler = LightControl_handler };

// All direct methods referenced in direct_method_bindings will be subscribed to in the InitPeripheralsAndHandlers function
#define DECLARE_DX_DIRECT_METHOD_BINDINGS
static DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {  &dm_FanOff, &dm_LightControl };

/****************************************************************************************
* GPIO Bindings
****************************************************************************************/
static DX_GPIO_BINDING gpio_AzureIotConnectedLed = { .pin = NETWORK_CONNECTED_LED, .name = "AzureIotConnectedLed", .direction = DX_OUTPUT, .initialState = GPIO_Value_Low, .invertPin = true };
static DX_GPIO_BINDING gpio_ButtonA = { .pin = BUTTON_A, .name = "ButtonA", .direction = DX_INPUT };

// All GPIOs referenced in gpio_bindings with be opened in the InitPeripheralsAndHandlers function
#define DECLARE_DX_GPIO_BINDINGS
static DX_GPIO_BINDING *gpio_bindings[] = {  &gpio_AzureIotConnectedLed, &gpio_ButtonA };


/****************************************************************************************
* Initialise bindings
****************************************************************************************/

static void dx_initPeripheralAndHandlers(void)
{
#if defined(DX_PUBLISH_TELEMETRY) || defined(DECLARE_DX_DEVICE_TWIN_BINDINGS) || defined(DECLARE_DX_DIRECT_METHOD_BINDINGS)
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, PNP_MODEL_ID);
#endif
#if defined(DECLARE_DX_GPIO_BINDINGS)
    dx_gpioSetOpen(gpio_bindings, NELEMS(gpio_bindings));
#endif
#if defined(DECLARE_DX_DEVICE_TWIN_BINDINGS)
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));
#endif
#if defined(DECLARE_DX_DIRECT_METHOD_BINDINGS)
    dx_directMethodSubscribe(direct_method_bindings, NELEMS(direct_method_bindings));
#endif
#if defined(DECLARE_DX_TIMER_BINDINGS)
    dx_timerSetStart(timer_bindings, NELEMS(timer_bindings));
#endif
}

static void dx_closePeripheralAndHandlers(void){
#if defined(DECLARE_DX_TIMER_BINDINGS)
	dx_timerSetStop(timer_bindings, NELEMS(timer_bindings));
#endif
#if defined(DECLARE_DX_GPIO_BINDINGS)
	dx_gpioSetClose(gpio_bindings, NELEMS(gpio_bindings));
#endif
#ifdef DECLARE_DX_DEVICE_TWIN_BINDINGS
	dx_deviceTwinUnsubscribe();
#endif
#ifdef DECLARE_DX_DIRECT_METHOD_BINDINGS
	dx_directMethodUnsubscribe();
#endif
}
