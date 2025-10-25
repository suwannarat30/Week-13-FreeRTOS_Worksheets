#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "PRIORITY_CORE_EX";

// ---------------- Tasks ----------------
void high_priority_task(void *parameter)
{
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "HIGH Task iteration %d on Core %d", count++, xPortGetCoreID());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void medium_priority_task(void *parameter)
{
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "MEDIUM Task iteration %d on Core %d", count++, xPortGetCoreID());
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

void low_priority_task(void *parameter)
{
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "LOW Task iteration %d on Core %d", count++, xPortGetCoreID());
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

// ---------------- Main ----------------
void app_main(void)
{
    ESP_LOGI(TAG, "Starting Priority & Core Affinity Exercise");

    // Core 0: HIGH + LOW
    xTaskCreatePinnedToCore(high_priority_task, "HighTask", 2048, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(low_priority_task, "LowTask", 2048, NULL, 1, NULL, 0);

    // Core 1: MEDIUM
    xTaskCreatePinnedToCore(medium_priority_task, "MediumTask", 2048, NULL, 3, NULL, 1);
}
