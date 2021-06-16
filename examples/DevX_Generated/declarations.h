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
static void DesiredTemperature_handler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static DX_DIRECT_METHOD_RESPONSE_CODE FanControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static DX_DIRECT_METHOD_RESPONSE_CODE LightControl_handler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void MeasureCarbonMonoxide_handler(EventLoopTimer *eventLoopTimer);
static void MeasureTemperature_handler(EventLoopTimer *eventLoopTimer);
static void PublishTelemetry_handler(EventLoopTimer *eventLoopTimer);
static void Watchdog_handler(EventLoopTimer *eventLoopTimer);
