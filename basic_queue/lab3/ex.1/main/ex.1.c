#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "EXERCISE1";

QueueHandle_t number_queue;

// Producer Task
void producer_task(void *parameter) {
    int num;
    while (1) {
        num = rand() % 100;  // Generate random number 0-99
        if (xQueueSend(number_queue, &num, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "PRODUCER: Generated number: %d", num);
        } else {
            ESP_LOGW(TAG, "PRODUCER: Queue full, number dropped");
        }
        vTaskDelay(pdMS_TO_TICKS(500));  // 500ms delay
    }
}

// Consumer Task
void consumer_task(void *parameter) {
    int num;
    float sum = 0;
    int count = 0;
    while (1) {
        if (xQueueReceive(number_queue, &num, pdMS_TO_TICKS(1000)) == pdTRUE) {
            sum += num;
            count++;
            ESP_LOGI(TAG, "CONSUMER: Received: %d, Average: %.2f", num, sum / count);
        } else {
            ESP_LOGW(TAG, "CONSUMER: Timeout waiting for number");
        }
    }
}

// Main Application
void app_main(void) {
    // Create queue with capacity 10 integers
    number_queue = xQueueCreate(10, sizeof(int));
    if (number_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create number queue");
        return;
    }

    // Create producer and consumer tasks
    xTaskCreate(producer_task, "ProducerTask", 2048, NULL, 5, NULL);
    xTaskCreate(consumer_task, "ConsumerTask", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Exercise 1: Basic Producer-Consumer started");
}

