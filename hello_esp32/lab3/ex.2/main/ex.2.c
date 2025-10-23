#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "EX2_COMM";

// Global variable for simple communication
volatile int shared_counter = 0;

void producer_task(void *pvParameters)
{
    while (1) {
        shared_counter++;
        ESP_LOGI(TAG, "Producer: counter = %d", shared_counter);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void consumer_task(void *pvParameters)
{
    int last_value = 0;
    
    while (1) {
        if (shared_counter != last_value) {
            ESP_LOGI(TAG, "Consumer: received %d", shared_counter);
            last_value = shared_counter;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    // สร้าง task producer และ consumer
    xTaskCreate(producer_task, "Producer", 2048, NULL, 2, NULL);
    xTaskCreate(consumer_task, "Consumer", 2048, NULL, 1, NULL);
}
