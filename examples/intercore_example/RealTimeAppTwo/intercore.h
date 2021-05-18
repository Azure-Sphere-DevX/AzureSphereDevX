#pragma once

#include "intercore_contract.h"
#include "os_hal_mbox.h"
#include "os_hal_mbox_shared_mem.h"
#include <string.h>
#include "mhal_osai.h"

/* Maximum mailbox buffer len.
 *    Maximum message len: 1024B
 *                         1024 is the maximum value when HL_APP invoke send().
 *    Component UUID len : 16B
 *    Reserved data len  : 4B
 */

#define MBOX_BUFFER_LEN_MAX 1044

// extern INTERCORE_DISK_DATA_BLOCK_T disk_ic_data;
extern u32 mbox_shared_buf_size;
extern uint32_t mbox_irq_status;
extern uint8_t mbox_local_buf[MBOX_BUFFER_LEN_MAX];
extern BufferHeader *outbound, *inbound;
extern volatile u8 blockDeqSema;
extern volatile u8 blockFifoSema;
// extern u32 pay_load_start_offset;
// extern size_t payloadStart;

// uint8_t mbox_local_buf[MBOX_BUFFER_LEN_MAX];
// BufferHeader* outbound, * inbound;
// volatile u8  blockDeqSema;
// volatile u8  blockFifoSema;

// /* Bitmap for IRQ enable. bit_0 and bit_1 are used to communicate with HL_APP */
// static uint32_t mbox_irq_status = 0x3;
// static u32 pay_load_start_offset = 20; /* UUID 16B, Reserved 4B */
// static size_t payloadStart = 20;

/// <summary>
///     When sending a message, this is the recipient HLApp's component ID.
///     When receiving a message, this is the sender HLApp's component ID.
/// </summary>
typedef struct {
    /// <summary>4-byte little-endian word</summary>
    uint32_t data1;
    /// <summary>2-byte little-endian half</summary>
    uint16_t data2;
    /// <summary>2-byte little-endian half</summary>
    uint16_t data3;
    /// <summary>2 bytes (big-endian) followed by 6 bytes (big-endian)</summary>
    uint8_t data4[8];
    /// <summary> 4 bytes reserved </summary>
    uint32_t reserved_word;
} ComponentId;

extern const ComponentId hlAppId;

void initialise_intercore_comms(void);
// void send_intercode_data_msg(const char* message);
// void send_intercore_msg(INTERCORE_DISK_DATA_BLOCK_T* ic_data_block, size_t length);
