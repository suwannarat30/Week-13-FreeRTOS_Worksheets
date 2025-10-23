#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "EXERCISE3";

// -------------------------
// Priority Message Structure
// -------------------------
typedef struct {
    int id;
    char content[32];
} priority_msg_t;

// -------------------------
// Queue Handles
// -------------------------
QueueHandle_t high_queue;
QueueHandle_t normal_queue;

// -------------------------
// Producer Task
// -------------------------
void priority_producer_task(void *param) {
    priority_msg_t msg;
    int counter = 0;

    while (1) {
        msg.id = counter;
        snprintf(msg.content, sizeof(msg.content), "Message %d", counter);

        if (counter % 3 == 0) { // High priority
            if (xQueueSend(high_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
                ESP_LOGI(TAG, "PRODUCER: Sent HIGH %d", counter);
            } else {
                ESP_LOGW(TAG, "PRODUCER: High queue full");
            }
        } else { // Normal priority
            if (xQueueSend(normal_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
                ESP_LOGI(TAG, "PRODUCER: Sent NORMAL %d", counter);
            } else {
                ESP_LOGW(TAG, "PRODUCER: Normal queue full");
            }
        }

        counter++;
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

// -------------------------
// Consumer Task
// -------------------------
void priority_consumer_task(void *param) {
    priority_msg_t msg;

    while (1) {
        // Check high priority queue first
        if (xQueueReceive(high_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "CONSUMER: Processing HIGH %d: %s", msg.id, msg.content);
        }
        // Then normal queue
        else if (xQueueReceive(normal_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "CONSUMER: Processing NORMAL %d: %s", msg.id, msg.content);
        }
        else {
            // If both empty, short delay to yield CPU
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

// -------------------------
// Queue Monitoring Task
// -------------------------
void queue_monitor_task(void *param) {
    while (1) {
        UBaseType_t high_items = uxQueueMessagesWaiting(high_queue);
        UBaseType_t high_spaces = uxQueueSpacesAvailable(high_queue);
        UBaseType_t normal_items = uxQueueMessagesWaiting(normal_queue);
        UBaseType_t normal_spaces = uxQueueSpacesAvailable(normal_queue);

        if (high_spaces == 0) ESP_LOGW(TAG, "High queue FULL");
        if (normal_spaces == 0) ESP_LOGW(TAG, "Normal queue FULL");

        if (high_items > 0.8 * (high_items + high_spaces)) {
            ESP_LOGW(TAG, "High queue almost full: %d/%d", high_items, high_items + high_spaces);
        }
        if (normal_items > 0.8 * (normal_items + normal_spaces)) {
            ESP_LOGW(TAG, "Normal queue almost full: %d/%d", normal_items, normal_items + normal_spaces);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// -------------------------
// Main Application
// -------------------------
void app_main(void) {
    // Create queues
    high_queue = xQueueCreate(5, sizeof(priority_msg_t));     // High priority queue
    normal_queue = xQueueCreate(15, sizeof(priority_msg_t));  // Normal priority queue

    if (high_queue == NULL || normal_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create priority queues");
        return;
    }

    // Create tasks
    xTaskCreate(priority_producer_task, "PriorityProducer", 2048, NULL, 4, NULL);
    xTaskCreate(priority_consumer_task, "PriorityConsumer", 2048, NULL, 5, NULL);
    xTaskCreate(queue_monitor_task, "QueueMonitor", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Exercise 3: Priority Message System started");
}
