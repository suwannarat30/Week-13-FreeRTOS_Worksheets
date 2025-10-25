#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "EX4";

// Queue for logging task messages
#define LOG_QUEUE_LENGTH 20
#define LOG_MSG_SIZE 64
static QueueHandle_t logQueue;

// Number of dynamic tasks
#define NUM_DYNAMIC_TASKS 5

// Task function
void dynamic_task(void *parameter)
{
    int task_id = (int)(intptr_t)parameter;
    char msg[LOG_MSG_SIZE];

    while (1)
    {
        snprintf(msg, sizeof(msg), "Dynamic Task %d running on Core %d", 
                 task_id, xPortGetCoreID());
        xQueueSend(logQueue, msg, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(1000 + task_id * 200)); // different period
    }
}

// Logging task
void logger_task(void *parameter)
{
    char msg[LOG_MSG_SIZE];
    while (1)
    {
        if (xQueueReceive(logQueue, msg, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "%s", msg);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Exercise 4 - Dynamic Tasks with Core Affinity");

    // Create logging queue
    logQueue = xQueueCreate(LOG_QUEUE_LENGTH, LOG_MSG_SIZE);
    if (!logQueue)
    {
        ESP_LOGE(TAG, "Failed to create log queue");
        return;
    }

    // Create logger task on Core 0
    xTaskCreatePinnedToCore(logger_task, "LoggerTask", 2048, NULL, 10, NULL, 0);

    // Create dynamic tasks
    for (int i = 0; i < NUM_DYNAMIC_TASKS; i++)
    {
        char taskName[16];
        snprintf(taskName, sizeof(taskName), "DynTask%d", i + 1);

        // Pin even tasks to Core 0, odd tasks to Core 1
        int core = (i % 2 == 0) ? 0 : 1;

        BaseType_t result = xTaskCreatePinnedToCore(
            dynamic_task,      // task function
            taskName,          // task name
            2048,              // stack size
            (void *)(intptr_t)(i + 1), // parameter = task id
            5 + i,             // priority
            NULL,              // task handle
            core               // core affinity
        );

        if (result != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create task %s on Core %d", taskName, core);
        }
    }
}
