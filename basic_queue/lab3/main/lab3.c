#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "MAILBOX";

// Mailbox queue handle
QueueHandle_t mailbox;

// Setup mailbox
void setup_mailbox(void)
{
    // Queue length 1: ทำงานเหมือน mailbox
    mailbox = xQueueCreate(1, sizeof(int));
    if (mailbox == NULL) {
        ESP_LOGE(TAG, "Failed to create mailbox");
    }
}

// Mailbox sender task
void mailbox_sender_task(void *parameter)
{
    int message = 0;

    while (1) {
        message++;

        // Overwrite previous message if mailbox full
        xQueueOverwrite(mailbox, &message);
        ESP_LOGI(TAG, "Mailbox updated with: %d", message);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Mailbox reader task
void mailbox_reader_task(void *parameter)
{
    int received_message;

    while (1) {
        // Peek at mailbox without removing message
        if (xQueuePeek(mailbox, &received_message, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Mailbox contains: %d", received_message);
        } else {
            ESP_LOGI(TAG, "Mailbox is empty");
        }

        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void app_main(void)
{
    setup_mailbox();

    // สร้าง Tasks
    xTaskCreate(mailbox_sender_task, "MailboxSender", 2048, NULL, 2, NULL);
    xTaskCreate(mailbox_reader_task, "MailboxReader", 2048, NULL, 2, NULL);
}
