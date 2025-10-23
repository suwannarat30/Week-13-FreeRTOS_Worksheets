#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

// -------------------------
// 1. Queue State Inspection
// -------------------------
void debug_queue_state(QueueHandle_t queue, const char* name)
{
    UBaseType_t items = uxQueueMessagesWaiting(queue);
    UBaseType_t spaces = uxQueueSpacesAvailable(queue);
    ESP_LOGI("QUEUE_DEBUG", "Queue %s: Items=%d, Spaces=%d", name, items, spaces);
}

// -------------------------
// 2. Task Block Detection Example
// -------------------------
void task_receive_with_timeout(QueueHandle_t queue)
{
    int data;
    if (xQueueReceive(queue, &data, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGW("QUEUE_DEBUG", "Queue receive timeout - possible producer issue");
    } else {
        ESP_LOGI("QUEUE_DEBUG", "Received data: %d", data);
    }
}

// -------------------------
// 3. Memory Usage Check
// -------------------------
void print_heap_usage(void)
{
    size_t free_heap = esp_get_free_heap_size();
    size_t caps_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI("QUEUE_DEBUG", "Free heap: %d bytes, Internal heap: %d bytes", 
             (int)free_heap, (int)caps_heap);
}

// -------------------------
// 4. Debug Task
// -------------------------
void debug_task(void *param)
{
    QueueHandle_t *queues = (QueueHandle_t *)param;
    while (1) {
        debug_queue_state(queues[0], "NumberQueue");
        debug_queue_state(queues[1], "MultiQueue");
        debug_queue_state(queues[2], "HighQueue");
        debug_queue_state(queues[3], "NormalQueue");
        debug_queue_state(queues[4], "RawQueue");
        debug_queue_state(queues[5], "ProcessedQueue");
        debug_queue_state(queues[6], "AlertQueue");

        print_heap_usage();

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// -------------------------
// Example queues
// -------------------------
QueueHandle_t NumberQueue;
QueueHandle_t MultiQueue;
QueueHandle_t HighQueue;
QueueHandle_t NormalQueue;
QueueHandle_t RawQueue;
QueueHandle_t ProcessedQueue;
QueueHandle_t AlertQueue;

// -------------------------
// app_main
// -------------------------
void app_main(void)
{
    // สร้าง queues ตัวอย่าง
    NumberQueue = xQueueCreate(10, sizeof(int));
    MultiQueue = xQueueCreate(10, sizeof(int));
    HighQueue = xQueueCreate(5, sizeof(int));
    NormalQueue = xQueueCreate(15, sizeof(int));
    RawQueue = xQueueCreate(10, sizeof(int));
    ProcessedQueue = xQueueCreate(10, sizeof(int));
    AlertQueue = xQueueCreate(5, sizeof(int));

    QueueHandle_t queues[] = {
        NumberQueue, MultiQueue, HighQueue, NormalQueue,
        RawQueue, ProcessedQueue, AlertQueue
    };

    // สร้าง debug task
    xTaskCreate(debug_task, "DebugTask", 4096, queues, 2, NULL);

    ESP_LOGI("MAIN", "Debug system started");
}
