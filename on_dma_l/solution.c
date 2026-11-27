#include "dma.h"

// Implement the driver here
// add any localfile states or helpers

typedef enum{
    STATE_IDLE,
    STATE_TRANSFERRING,
    STATE_READY,
    STATE_FAILED
} DriverState;

static DriverState state = STATE_IDLE;
static uint16_t buffer[BATCH_SIZE];


void app_init(void){
    DMA.CTRL = 0u;
    DMA.COUNT = 0u;
    DMA.SRC = (uintptr_t)0;
    DMA.DST = (uintptr_t)0;
    dma_clear_flags(DMA_COMPLETE | DMA_ERROR);
    state = STATE_IDLE;
}


bool app_start(void){
    if (state != STATE_IDLE){
        return false;
    }

    state = STATE_TRANSFERRING;
    DMA.CTRL = 0u;
    dma_clear_flags(DMA_COMPLETE | DMA_ERROR);
    DMA.SRC = (uintptr_t)&SENSOR_DATA;
    DMA.DST = (uintptr_t)buffer;
    DMA.COUNT = BATCH_SIZE;
    DMA.CTRL = DMA_ENABLE | DMA_P2M | DMA_WIDTH_16 |
                DMA_DST_INC | DMA_COMPLETE_IE | DMA_ERROR_IE;

    return true;
}

void app_dma_irq(void){
    const uint32_t flags = DMA.STATUS & (DMA_COMPLETE | DMA_ERROR);
    if (flags == 0){
        return;
    }

    dma_clear_flags(flags);

    if (state != STATE_TRANSFERRING){
        return;
    }

    DMA.CTRL &= ~DMA_ENABLE;

    state = ((flags & DMA_ERROR) != 0u) ? STATE_FAILED : STATE_READY;
    notify_worker_from_isr();
}


void app_task_step(void){
    if (!take_worker_notification()){
        return;
    }

    if (state == STATE_FAILED){
        state = STATE_IDLE;
        report_error();
        return;
    }

    if (state != STATE_READY){
        return;
    }

    BatchResult result = {
        .min = buffer[0],
        .max = buffer[0],
        .sum = 0u,
        .mean = 0u
    };

    for (uint32_t i = 0; i < BATCH_SIZE; ++i){
        const uint16_t sample = buffer[i];
        if (sample < result.min){
            result.min = sample;
        }
        if (sample > result.max){
            result.max = sample;
        }
        result.sum += sample;
    }
    result.mean = result.sum / BATCH_SIZE;

    state = STATE_IDLE;
    report_result(&result);
}
