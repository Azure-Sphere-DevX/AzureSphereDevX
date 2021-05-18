#include "intercore.h"

/******************************************************************************/
/* Functions */
/******************************************************************************/
/* Mailbox Fifo Interrupt handler.
 * Mailbox Fifo Interrupt is triggered when mailbox fifo been R/W.
 *     data->event.channel: Channel_0 for A7, Channel_1 for the other M4.
 *     data->event.ne_sts: FIFO Non-Empty.interrupt
 *     data->event.nf_sts: FIFO Non-Full interrupt
 *     data->event.rd_int: Read FIFO interrupt
 *     data->event.wr_int: Write FIFO interrupt
 */
void mbox_fifo_cb(struct mtk_os_hal_mbox_cb_data *data)
{
    if (data->event.channel == OS_HAL_MBOX_CH0) {
        /* A7 core write data to mailbox fifo. */
        if (data->event.wr_int)
            blockFifoSema++;
    }
}

/* SW Interrupt handler.
 * SW interrupt is triggered when:
 *    1. A7 read/write the shared memory.
 *     data->swint.swint_channel: Channel_0 for A7.
 *     Channel_0:
 *         data->swint.swint_sts bit_0: A7 read data from mailbox
 *         data->swint.swint_sts bit_1: A7 write data to mailbox
 */
void mbox_swint_cb(struct mtk_os_hal_mbox_cb_data *data)
{
    if (data->swint.channel == OS_HAL_MBOX_CH0) {
        if (data->swint.swint_sts & (1 << 1))
            blockDeqSema++;
    }
}

void initialise_intercore_comms(void)
{
    struct mbox_fifo_event mask;

    blockDeqSema = 0;
    blockFifoSema = 0;

    /* Open the MBOX channel of A7 <-> M4 */
    mtk_os_hal_mbox_open_channel(OS_HAL_MBOX_CH0);

    /* Register interrupt callback */
    mask.channel = OS_HAL_MBOX_CH0;
    mask.ne_sts = 0; /* FIFO Non-Empty interrupt */
    mask.nf_sts = 0; /* FIFO Non-Full interrupt */
    mask.rd_int = 0; /* Read FIFO interrupt */
    mask.wr_int = 1; /* Write FIFO interrupt */
    mtk_os_hal_mbox_fifo_register_cb(OS_HAL_MBOX_CH0, mbox_fifo_cb, &mask);
    mtk_os_hal_mbox_sw_int_register_cb(OS_HAL_MBOX_CH0, mbox_swint_cb, mbox_irq_status);

    /* Get mailbox shared buffer size, defined by Azure Sphere OS. */
    if (GetIntercoreBuffers(&outbound, &inbound, &mbox_shared_buf_size) == -1) {
        printf("GetIntercoreBuffers failed\n");
        return;
    }
}