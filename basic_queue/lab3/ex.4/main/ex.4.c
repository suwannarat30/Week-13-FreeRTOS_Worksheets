#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "EXERCISE4";

// -------------------------
// Sensor Data Structures
// -------------------------
typedef struct {
    float value;
    int sensor_id;
} raw_sensor_t;

typedef struct {
    float value;
    int sensor_id;
    float processed_value;
} processed_sensor_t;

// -------------------------
// Queues
// -------------------------
QueueHandle_t raw_queue;
QueueHandle_t processed_queue;
QueueHandle_t alert_queue;

// -------------------------
// Sensor Reader Task
// -------------------------
void sensor_reader_task(void *param) {
    raw_sensor_t data;

    while (1) {
        data.sensor_id = 1;  // Example sensor ID
        data.value = 20.0 + (rand() % 300) / 10.0;  // Random 20.0-50.0
        if (xQueueSend(raw_queue, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "SENSOR_READER: Sensor %d value %.2f", data.sensor_id, data.value);
        } else {
            ESP_LOGW(TAG, "SENSOR_READER: Raw queue full, dropping value");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// -------------------------
// Data Processor Task
// -------------------------
void data_processor_task(void *param) {
    raw_sensor_t raw;
    processed_sensor_t proc;

    while (1) {
        if (xQueueReceive(raw_queue, &raw, portMAX_DELAY) == pdTRUE) {
            proc.sensor_id = raw.sensor_id;
            proc.value = raw.value;
            proc.processed_value = raw.value * 1.1; // simple processing

            // Send to processed_queue
            if (xQueueSend(processed_queue, &proc, 0) != pdTRUE) {
                ESP_LOGW(TAG, "DATA_PROCESSOR: Processed queue full");
            }

            // Check threshold for alert
            if (proc.processed_value > 45.0) {
                if (xQueueSend(alert_queue, &proc, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "DATA_PROCESSOR: Alert queue full");
                }
            }

            ESP_LOGI(TAG, "DATA_PROCESSOR: Sensor %d processed value %.2f", 
                     proc.sensor_id, proc.processed_value);
        }
    }
}

// -------------------------
// Alert Handler Task
// -------------------------
void alert_handler_task(void *param) {
    processed_sensor_t alert;

    while (1) {
        if (xQueueReceive(alert_queue, &alert, portMAX_DELAY) == pdTRUE) {
            ESP_LOGW(TAG, "ALERT_HANDLER: Sensor %d value %.2f exceeds threshold!", 
                     alert.sensor_id, alert.processed_value);
        }
    }
}

// -------------------------
// Main Application
// -------------------------
void app_main(void) {
    // Create queues
    raw_queue = xQueueCreate(10, sizeof(raw_sensor_t));
    processed_queue = xQueueCreate(10, sizeof(processed_sensor_t));
    alert_queue = xQueueCreate(5, sizeof(processed_sensor_t));

    if (!raw_queue || !processed_queue || !alert_queue) {
        ESP_LOGE(TAG, "Failed to create sensor queues");
        return;
    }

    // Create tasks
    xTaskCreate(sensor_reader_task, "SensorReader", 2048, NULL, 4, NULL);
    xTaskCreate(data_processor_task, "DataProcessor", 2048, NULL, 5, NULL);
    xTaskCreate(alert_handler_task, "AlertHandler", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Exercise 4: Sensor Data Pipeline started");
}
