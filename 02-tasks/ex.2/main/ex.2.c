#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "CORE_AFFINITY";

// High priority task pinned to Core 0
void high_priority_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "High priority task running on Core 0");
        for (int i = 0; i < 100000; i++) { volatile int dummy = i; }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

// Low priority task pinned to Core 1
void low_priority_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Low priority task running on Core 1");
        for (int i = 0; i < 200000; i++) { volatile int dummy = i; }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Task Affinity Demo (Dual-Core) ===");

    // Pin tasks to specific cores
    xTaskCreatePinnedToCore(high_priority_task, "HighPrio", 2048, NULL, 5, NULL, 0); // Core 0
    xTaskCreatePinnedToCore(low_priority_task, "LowPrio", 2048, NULL, 1, NULL, 1);   // Core 1

    ESP_LOGI(TAG, "Tasks pinned to cores. Observe logs for CPU utilization.");
}
