#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "EX1_SELF_DELETE";

void temporary_task(void *pvParameters)
{
    int *duration = (int *)pvParameters;
    
    ESP_LOGI(TAG, "Temporary task will run for %d seconds", *duration);
    
    for (int i = *duration; i > 0; i--) {
        ESP_LOGI(TAG, "Temporary task countdown: %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Temporary task self-deleting");
    vTaskDelete(NULL); // Delete itself
}

void app_main(void)
{
    static int temp_duration = 10;
    xTaskCreate(temporary_task, "TempTask", 2048, &temp_duration, 1, NULL);
}
