#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define TAG "QUEUE_LAB"

// โครงสร้างข้อความที่จะส่งผ่าน Queue
typedef struct {
    int id;
    char text[50];
    TickType_t timestamp;
} Message;

// ตัวแปรเก็บ Queue handle
static QueueHandle_t xQueue = NULL;

// 📨 Sender Task
void sender_task(void *pvParameters)
{
    int counter = 0;
    Message message;

    while (1) {
        message.id = counter++;
        snprintf(message.text, sizeof(message.text), "Hello from sender #%d", message.id);
        message.timestamp = xTaskGetTickCount();

        // Queue Overflow Protection
        if (xQueueSend(xQueue, &message, 0) != pdPASS) {
            ESP_LOGW(TAG, "Queue full! Dropping message ID=%d", message.id);
        } else {
            ESP_LOGI(TAG, "Sent: ID=%d, MSG=%s, Time=%lu",
                     message.id, message.text, message.timestamp);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // ส่งถี่พอสมควร เพื่อจำลองการ Overflow ได้ง่าย
    }
}

// 📥 Receiver Task (Non-blocking Receive)
void receiver_task(void *pvParameters)
{
    Message received_message;

    while (1) {
        // Non-blocking Receive
        if (xQueueReceive(xQueue, &received_message, 0) == pdPASS) {
            ESP_LOGI(TAG, "Received: ID=%d, MSG=%s, Time=%lu",
                     received_message.id, received_message.text, received_message.timestamp);
        } else {
            ESP_LOGI(TAG, "No message available, doing other work...");
            vTaskDelay(pdMS_TO_TICKS(1000)); // รอ 1 วินาทีก่อนเช็กใหม่
        }
    }
}

// 🧭 Monitor Task (ดูสถานะคิว)
void queue_monitor_task(void *pvParameters)
{
    while (1) {
        UBaseType_t messages = uxQueueMessagesWaiting(xQueue);
        UBaseType_t spaces = uxQueueSpacesAvailable(xQueue);

        ESP_LOGI(TAG, "Queue Status - Messages: %d, Free spaces: %d", messages, spaces);

        // แสดงสถานะเป็นกราฟิกง่าย ๆ
        printf("Queue: [");
        for (int i = 0; i < messages; i++) printf("■");
        for (int i = messages; i < 5; i++) printf("□");
        printf("]\n");

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// 🧩 ฟังก์ชันหลัก
void app_main(void)
{
    ESP_LOGI(TAG, "Starting Queue Experiment 3: Queue Empty Test");

    // สร้าง Queue ขนาด 5 ช่อง
    xQueue = xQueueCreate(5, sizeof(Message));
    if (xQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue!");
        return;
    }

    // สร้าง Tasks
    xTaskCreate(sender_task, "SenderTask", 4096, NULL, 2, NULL);
    xTaskCreate(receiver_task, "ReceiverTask", 4096, NULL, 2, NULL);
    xTaskCreate(queue_monitor_task, "MonitorTask", 4096, NULL, 1, NULL);
}
