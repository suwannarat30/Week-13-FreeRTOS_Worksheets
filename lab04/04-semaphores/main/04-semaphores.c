#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "BINARY_SEM";

// Semaphore handle
SemaphoreHandle_t binary_semaphore;

void producer_task(void *parameter)
{
    int counter = 0;
    
    while (1) {
        // Do some work
        ESP_LOGI(TAG, "Producer working... %d", counter++);
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Signal consumer that work is done
        if (xSemaphoreGive(binary_semaphore) == pdTRUE) {
            ESP_LOGI(TAG, "Producer: Work completed, signaling consumer");
        } else {
            ESP_LOGW(TAG, "Producer: Failed to give semaphore");
        }
    }
}

void consumer_task(void *parameter)
{
    while (1) {
        ESP_LOGI(TAG, "Consumer: Waiting for signal...");
        
        // Wait for producer signal (block indefinitely)
        if (xSemaphoreTake(binary_semaphore, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Consumer: Received signal, processing...");
            
            // Process the work
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "Consumer: Processing completed");
        }
    }
}

void app_main(void)
{
    // Create binary semaphore
    binary_semaphore = xSemaphoreCreateBinary();
    if (binary_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create binary semaphore");
        return;
    }
    
    // Create tasks
    xTaskCreate(producer_task, "Producer", 2048, NULL, 5, NULL);
    xTaskCreate(consumer_task, "Consumer", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Binary semaphore example started");
}