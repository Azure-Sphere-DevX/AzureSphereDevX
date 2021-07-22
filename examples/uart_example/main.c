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

#include "dx_exit_codes.h"
#include "dx_uart.h"
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <applibs/log.h>

#define ONE_MS 1000000

// Forward declarations
static void ButtonPressCheckHandler(EventLoopTimer *eventLoopTimer);
static void uart_rx_handler1(DX_UART_BINDING *uartBinding);

/****************************************************************************************
 * GPIO Peripherals
 ****************************************************************************************/
static DX_GPIO_BINDING buttonA = {
    .pin = BUTTON_A, .name = "buttonA", .direction = DX_INPUT, .detect = DX_GPIO_DETECT_LOW};

static DX_GPIO_BINDING buttonB = {
    .pin = BUTTON_B, .name = "buttonB", .direction = DX_INPUT, .detect = DX_GPIO_DETECT_LOW};

// All GPIOs added to gpio_bindings will be opened in InitPeripheralsAndHandlers
DX_GPIO_BINDING *gpio_bindings[] = {&buttonA, &buttonB};

/****************************************************************************************
 * UART Peripherals
 ****************************************************************************************/
static DX_UART_BINDING loopBackClick1 = {.uart = UART_CLICK1,
                                         .name = "uart click1",
                                         .handler = uart_rx_handler1,
                                         .uartConfig.baudRate = 115200,
                                         .uartConfig.dataBits = UART_DataBits_Eight,
                                         .uartConfig.parity = UART_Parity_None,
                                         .uartConfig.stopBits = UART_StopBits_One,
                                         .uartConfig.flowControl = UART_FlowControl_None};

// All UARTSs added to uart_bindings will be opened in InitPeripheralsAndHandlers
DX_UART_BINDING *uart_bindings[] = {&loopBackClick1};

/****************************************************************************************
 * Timer Bindings
 ****************************************************************************************/
static DX_TIMER_BINDING buttonPressCheckTimer = {
    .period = {0, ONE_MS}, .name = "buttonPressCheckTimer", .handler = ButtonPressCheckHandler};

// All timers referenced in timer_bindings with be opened in the InitPeripheralsAndHandlers function
DX_TIMER_BINDING *timer_bindings[] = {&buttonPressCheckTimer};

/****************************************************************************************
 * Implementation
 ****************************************************************************************/

/// <summary>
/// Handler to check for Button Presses
/// </summary>
static void ButtonPressCheckHandler(EventLoopTimer *eventLoopTimer)
{
    static GPIO_Value_Type buttonAState, buttonBState;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    char buttonAMsg[] = "This is a test, ButtonA!";
    char buttonBMsg[] = "This is a test, ButtonB!";

    if (dx_gpioStateGet(&buttonA, &buttonAState)) {

        Log_Debug("Sending data over the uart!\n");
        dx_uartWrite(&loopBackClick1, buttonAMsg, strnlen(buttonAMsg, 32));
    }

    else if (dx_gpioStateGet(&buttonB, &buttonBState)) {
        Log_Debug("Sending data over the uart!\n");
        dx_uartWrite(&loopBackClick1, buttonBMsg, strnlen(buttonBMsg, 32));
    }
}

static void uart_rx_handler1(DX_UART_BINDING *uartBinding)
{
    // Read data from the uart here
    char rxBuffer[128 + 1];
    int bytesRead = dx_uartRead(uartBinding, rxBuffer, 128);
    if (bytesRead > 0) {
        // Null terminate the buffer before printing it to debug
        rxBuffer[bytesRead] = '\0';
        Log_Debug("RX(1) %d bytes from %s: %s\n", bytesRead,
                  uartBinding->name == NULL ? "No name" : uartBinding->name, rxBuffer);
    }
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_gpioSetOpen(gpio_bindings, NELEMS(gpio_bindings));
    dx_uartSetOpen(uart_bindings, NELEMS(uart_bindings));
    dx_timerSetStart(timer_bindings, NELEMS(timer_bindings));
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timer_bindings, NELEMS(timer_bindings));
    dx_uartSetClose(uart_bindings, NELEMS(uart_bindings));
    dx_gpioSetClose(gpio_bindings, NELEMS(gpio_bindings));
    dx_timerEventLoopStop();
}

int main(void)
{
    dx_registerTerminationHandler();
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
