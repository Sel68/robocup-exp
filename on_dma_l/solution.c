
#include "dma.h"
typedef enum {
    STATE_IDLE,
    STATE_DMA_ACTIVE,
    STATE_WORKER_PENDING
} DriverState;



static uint16_t samples[BATCH_SIZE];
static DriverState state;
static bool batch_error;








void app_init(void)
{
    state = STATE_IDLE;
    batch_error = false;
}
bool app_start(void)
{
    if (state != STATE_IDLE) {
        return false;
    }
    state = STATE_DMA_ACTIVE;
    batch_error = false;

    DMA.SRC = (uintptr_t)&SENSOR_DATA;
    DMA.DST = (uintptr_t)samples;
    DMA.COUNT = BATCH_SIZE;




  
    DMA.CTRL = DMA_P2M
             | DMA_WIDTH_16
             | DMA_DST_INC
             | DMA_COMPLETE_IE
             | DMA_ERROR_IE
             | DMA_ENABLE;

    return true;
}


void app_dma_irq(void)
{
    uint32_t flags = DMA.STATUS & (DMA_COMPLETE | DMA_ERROR);
    if (flags == 0u) {
        return;
    }
    dma_clear_flags(flags);
    if (state != STATE_DMA_ACTIVE) {
        return;
    }
    batch_error = (flags & DMA_ERROR) != 0u;
    state = STATE_WORKER_PENDING;




  
    notify_worker_from_isr();
}


/* Worker task: consume notification and process the batch. */
void app_task_step(void)
{
    BatchResult result;
    if (!take_worker_notification()) {
        return;
    }
    if (state != STATE_WORKER_PENDING) {
        return;
    }
    /* Discard failed batches. */
    if (batch_error) {
        report_error();
    }
    else {

      
        /* Initialize statistics using the first sample. */
        result.min = samples[0];
        result.max = samples[0];
        result.sum = 0u;




      
        /* Calculate statistics only in the worker. */
        for (uint32_t i = 0; i < BATCH_SIZE; i++) {

            if (samples[i] < result.min) {
                result.min = samples[i];
            }

            if (samples[i] > result.max) {
                result.max = samples[i];
            }

            result.sum += samples[i];
        }




      
        /* Integer mean: fractional part is discarded. */
        result.mean = result.sum / BATCH_SIZE;

        report_result(&result);
    }




  
    /* Release the buffer after processing/reporting. */
    batch_error = false;
    state = STATE_IDLE;
}
