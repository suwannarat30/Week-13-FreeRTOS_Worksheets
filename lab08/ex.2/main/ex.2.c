#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "CORE_LOAD_EX";

QueueHandle_t core0_queue;
QueueHandle_t core1_queue;

// ---------------- Core 0 Task ----------------
void core0_task1(void *parameter)
{
    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "Core 0 Task1 iteration %d on Core %d", counter++, xPortGetCoreID());
        int send_val = counter * 10;
        xQueueSend(core1_queue, &send_val, 0);  // ส่งไป Core 1
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void core0_task2(void *parameter)
{
    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "Core 0 Task2 iteration %d on Core %d", counter++, xPortGetCoreID());
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

// ---------------- Core 1 Task ----------------
void core1_task1(void *parameter)
{
    int received_val;
    while (1) {
        if (xQueueReceive(core1_queue, &received_val, pdMS_TO_TICKS(500)) == pdTRUE) {
            ESP_LOGI(TAG, "Core 1 Task1 received %d on Core %d", received_val, xPortGetCoreID());
        } else {
            ESP_LOGI(TAG, "Core 1 Task1 waiting for data on Core %d", xPortGetCoreID());
        }
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

void core1_task2(void *parameter)
{
    int counter = 0;
    while (1) {
        ESP_LOGI(TAG, "Core 1 Task2 iteration %d on Core %d", counter++, xPortGetCoreID());
        vTaskDelay(pdMS_TO_TICKS(1800));
    }
}

// ---------------- Main ----------------
void app_main(void)
{
    ESP_LOGI(TAG, "Starting Core Load Balancing Exercise");

    // Queue สำหรับส่งข้อมูลจาก Core0 -> Core1
    core1_queue = xQueueCreate(5, sizeof(int));
    if (!core1_queue) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    // สร้าง Task บน Core 0
    xTaskCreatePinnedToCore(core0_task1, "Core0Task1", 2048, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(core0_task2, "Core0Task2", 2048, NULL, 5, NULL, 0);

    // สร้าง Task บน Core 1
    xTaskCreatePinnedToCore(core1_task1, "Core1Task1", 2048, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(core1_task2, "Core1Task2", 2048, NULL, 5, NULL, 1);
}
