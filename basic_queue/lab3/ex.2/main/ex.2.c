#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "EXERCISE2";

// -------------------------
// Message Types
// -------------------------
typedef enum { MSG_TEXT, MSG_NUMBER, MSG_COMMAND } msg_type_t;

typedef struct {
    msg_type_t type;
    char text[32];
    int number;
    char command[16];
} message_t;

// -------------------------
// Queue Handle
// -------------------------
QueueHandle_t multi_queue;

// -------------------------
// Producer Tasks
// -------------------------
void producer_text_task(void *param) {
    message_t msg;
    while (1) {
        msg.type = MSG_TEXT;
        strcpy(msg.text, "Hello World");
        if (xQueueSend(multi_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "PRODUCER_TEXT: Sent text message");
        } else {
            ESP_LOGW(TAG, "PRODUCER_TEXT: Queue full");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void producer_number_task(void *param) {
    message_t msg;
    while (1) {
        msg.type = MSG_NUMBER;
        msg.number = rand() % 500;
        if (xQueueSend(multi_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "PRODUCER_NUM: Sent number message");
        } else {
            ESP_LOGW(TAG, "PRODUCER_NUM: Queue full");
        }
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

void producer_command_task(void *param) {
    message_t msg;
    while (1) {
        msg.type = MSG_COMMAND;
        strcpy(msg.command, "RESET");
        if (xQueueSend(multi_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "PRODUCER_CMD: Sent command message");
        } else {
            ESP_LOGW(TAG, "PRODUCER_CMD: Queue full");
        }
        vTaskDelay(pdMS_TO_TICKS(1400));
    }
}

// -------------------------
// Consumer Task
// -------------------------
void consumer_multi_task(void *param) {
    message_t msg;
    while (1) {
        if (xQueueReceive(multi_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
                case MSG_TEXT:
                    ESP_LOGI(TAG, "CONSUMER: Processing TEXT: \"%s\"", msg.text);
                    break;
                case MSG_NUMBER:
                    ESP_LOGI(TAG, "CONSUMER: Processing NUMBER: %d", msg.number);
                    break;
                case MSG_COMMAND:
                    ESP_LOGI(TAG, "CONSUMER: Processing COMMAND: %s", msg.command);
                    break;
            }
        }
    }
}

// -------------------------
// Main Application
// -------------------------
void app_main(void) {
    // Create queue with enough space for multiple messages
    multi_queue = xQueueCreate(10, sizeof(message_t));
    if (multi_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create multi-type queue");
        return;
    }

    // Create producer tasks
    xTaskCreate(producer_text_task, "ProducerText", 2048, NULL, 4, NULL);
    xTaskCreate(producer_number_task, "ProducerNum", 2048, NULL, 4, NULL);
    xTaskCreate(producer_command_task, "ProducerCmd", 2048, NULL, 4, NULL);

    // Create consumer task
    xTaskCreate(consumer_multi_task, "ConsumerMulti", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Exercise 2: Multi-Type Message System started");
}
