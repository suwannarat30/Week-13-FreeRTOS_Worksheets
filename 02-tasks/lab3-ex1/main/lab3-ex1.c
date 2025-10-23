#include <stdio.h>
#include <inttypes.h>   // สำหรับ PRIu32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "LAB3_EX1";

// Task ที่ใช้ stack เยอะ
void heavy_task(void *pvParameters)
{
    uint32_t stack_size = (uint32_t)(uintptr_t)pvParameters;

    // ใช้ stack ตามขนาดที่กำหนด
    int large_array[200];  // ใช้ stack จริง
    for (int i = 0; i < 200; i++) {
        large_array[i] = i*i;
    }

    // ตรวจสอบ high water mark ของ stack
    UBaseType_t water_mark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Heavy task with stack size %u: High water mark = %u words", stack_size, water_mark);

    vTaskDelay(pdMS_TO_TICKS(1000));
    vTaskDelete(NULL);
}

// สร้าง task หลายขนาด stack
void test_stack_sizes(void)
{
    uint32_t sizes[] = {512, 1024, 2048, 4096};

    for (int i = 0; i < 4; i++) {
        char task_name[24];

        snprintf(task_name, sizeof(task_name), "TestTask_%" PRIu32, sizes[i]);

        BaseType_t result = xTaskCreate(
            heavy_task,
            task_name,
            sizes[i],             // stack size
            (void *)(uintptr_t)sizes[i], // ส่งค่า stack size เป็น parameter
            2,                    // priority
            NULL
        );

        if (result == pdPASS) {
            ESP_LOGI(TAG, "Created task: %s with stack %u", task_name, sizes[i]);
        } else {
            ESP_LOGE(TAG, "Failed to create task: %s", task_name);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting stack size test...");
    test_stack_sizes();
}
