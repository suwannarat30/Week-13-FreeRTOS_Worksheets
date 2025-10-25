#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "DUAL_CORE_EX";

QueueHandle_t core0_to_core1_queue;

void core0_task(void *parameter)
{
    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "Core 0 compute task: iteration %d on Core %d", counter++, xPortGetCoreID());

        // ส่งข้อมูลไป Core1
        if (xQueueSend(core0_to_core1_queue, &counter, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Queue full, cannot send data");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1Hz
    }
}

void core1_task(void *parameter)
{
    int received_value = 0;
    while (1) {
        // รับข้อมูลจาก Core0
        if (xQueueReceive(core0_to_core1_queue, &received_value, pdMS_TO_TICKS(500)) == pdTRUE) {
            ESP_LOGI(TAG, "Core 1 I/O task: received %d from Core 0 on Core %d", received_value, xPortGetCoreID());
        } else {
            ESP_LOGI(TAG, "Core 1 waiting for data on Core %d", xPortGetCoreID());
        }

        vTaskDelay(pdMS_TO_TICKS(1500)); // 0.66Hz
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Dual-Core Exercise");

    // สร้าง Queue สำหรับ Core0 -> Core1
    core0_to_core1_queue = xQueueCreate(5, sizeof(int));
    if (!core0_to_core1_queue) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    // สร้าง Task กระจายทั้งสอง Core
    xTaskCreatePinnedToCore(core0_task, "Core0Task", 2048, NULL, 5, NULL, 0); // Pin to Core0
    xTaskCreatePinnedToCore(core1_task, "Core1Task", 2048, NULL, 5, NULL, 1); // Pin to Core1
}
