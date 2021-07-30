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
 *
 ************************************************************************************************/

#include "intercore.h"
#include "intercore_contract.h"

#include "os_hal_uart.h"
#include "nvic.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t mbox_local_buf[MBOX_BUFFER_LEN_MAX];
BufferHeader *outbound, *inbound;
volatile u8 blockDeqSema;
volatile u8 blockFifoSema;

const ComponentId hlAppId = {.data1 = 0x8E24FC73,
                             .data2 = 0xA329,
                             .data3 = 0x4D99,
                             .data4 = {0x9b, 0x37, 0x2e, 0xe5, 0x4d, 0xc7, 0x95, 0xe7},
                             .reserved_word = 0};

/* Bitmap for IRQ enable. bit_0 and bit_1 are used to communicate with HL_APP */
uint32_t mbox_irq_status = 0x3;
size_t payloadStart = 20; /* UUID 16B, Reserved 4B */

u32 mbox_shared_buf_size = 0;

static const uint8_t uart_port_num = OS_HAL_UART_ISU3;

/******************************************************************************/
/* Applicaiton Hooks */
/******************************************************************************/
/* Hook for "printf". */
void _putchar(char character)
{
    mtk_os_hal_uart_put_char(uart_port_num, character);
    if (character == '\n')
        mtk_os_hal_uart_put_char(uart_port_num, '\r');
}

static void send_intercore_msg(size_t length)
{
    uint32_t dataSize;

    // copy high level appid to first 20 bytes
    memcpy((void *)mbox_local_buf, &hlAppId, sizeof(hlAppId));
    dataSize = payloadStart + length;

    EnqueueData(inbound, outbound, mbox_shared_buf_size, mbox_local_buf, dataSize);
}

static void process_inbound_message()
{
    static int msgId = 0;
    u32 mbox_local_buf_len;
    int result;
    LP_INTER_CORE_BLOCK *in_data;
    LP_INTER_CORE_BLOCK *out_data;

    mbox_local_buf_len = MBOX_BUFFER_LEN_MAX;
    result =
        DequeueData(outbound, inbound, mbox_shared_buf_size, mbox_local_buf, &mbox_local_buf_len);

    if (result == 0 && mbox_local_buf_len > payloadStart) {

        in_data = (LP_INTER_CORE_BLOCK *)(mbox_local_buf + payloadStart);

        switch (in_data->cmd) {

        case LP_IC_ECHO:
            out_data = (LP_INTER_CORE_BLOCK *)(mbox_local_buf + payloadStart);

            out_data->msgId = msgId++;
            memcpy(out_data->message, in_data->message, sizeof(in_data->message));

            send_intercore_msg(sizeof(LP_INTER_CORE_BLOCK));
            break;
        default:
            break;
        }
    }
}

_Noreturn void RTCoreMain(void)
{

    /* Init Vector Table */
    NVIC_SetupVectorTable();

    initialise_intercore_comms();

    for (;;) {

        if (blockDeqSema > 0) {
            process_inbound_message();
        }
    }
}
