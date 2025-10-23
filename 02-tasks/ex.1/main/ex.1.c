#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_LOW_PIN GPIO_NUM_5
#define BUTTON_PIN GPIO_NUM_0

static const char *TAG = "DYNAMIC_PRIO";

// Low priority task
void low_priority_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Low priority task running");
        for (int i = 0; i < 200000; i++) { volatile int dummy = i; }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Dynamic priority booster
void dynamic_priority_demo(void *pvParameters)
{
    TaskHandle_t low_task_handle = (TaskHandle_t)pvParameters;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGW(TAG, "Boosting low priority task to priority 4");
        vTaskPrioritySet(low_task_handle, 4);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGW(TAG, "Restoring low priority task to priority 1");
        vTaskPrioritySet(low_task_handle, 1);
    }
}

// High priority task
void high_priority_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "High priority task running");
        for (int i = 0; i < 100000; i++) { volatile int dummy = i; }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Dynamic Priority Demo ===");

    TaskHandle_t low_handle = NULL;
    xTaskCreate(low_priority_task, "LowPrio", 2048, NULL, 1, &low_handle);
    xTaskCreate(high_priority_task, "HighPrio", 2048, NULL, 5, NULL);

    // Create dynamic priority booster
    xTaskCreate(dynamic_priority_demo, "DynBoost", 2048, (void*)low_handle, 3, NULL);

    ESP_LOGI(TAG, "Dynamic priority demo running...");
}
