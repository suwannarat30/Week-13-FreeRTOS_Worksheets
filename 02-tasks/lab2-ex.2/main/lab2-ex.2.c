#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "EX2_LED_DISPLAY";

// กำหนด GPIO สำหรับ LED
#define LED_RUNNING    GPIO_NUM_2
#define LED_READY      GPIO_NUM_4
#define LED_BLOCKED    GPIO_NUM_5
#define LED_SUSPENDED  GPIO_NUM_18

// State names
const char* state_names[] = {
    "Running",      // 0
    "Ready",        // 1  
    "Blocked",      // 2
    "Suspended",    // 3
    "Deleted",      // 4
    "Invalid"       // 5
};

// Get state name string
const char* get_state_name(eTaskState state)
{
    if (state <= eDeleted) return state_names[state];
    return state_names[5]; // Invalid
}

// ฟังก์ชันอัปเดต LED ตาม state
void update_state_display(eTaskState current_state)
{
    // ปิดทุก LED ก่อน
    gpio_set_level(LED_RUNNING, 0);
    gpio_set_level(LED_READY, 0);
    gpio_set_level(LED_BLOCKED, 0);
    gpio_set_level(LED_SUSPENDED, 0);
    
    // เปิด LED ตาม state
    switch (current_state) {
        case eRunning:
            gpio_set_level(LED_RUNNING, 1);
            break;
        case eReady:
            gpio_set_level(LED_READY, 1);
            break;
        case eBlocked:
            gpio_set_level(LED_BLOCKED, 1);
            break;
        case eSuspended:
            gpio_set_level(LED_SUSPENDED, 1);
            break;
        default:
            // ถ้า state ไม่รู้จัก กระพริบ LED ทั้งหมด
            for (int i = 0; i < 3; i++) {
                gpio_set_level(LED_RUNNING, 1);
                gpio_set_level(LED_READY, 1);
                gpio_set_level(LED_BLOCKED, 1);
                gpio_set_level(LED_SUSPENDED, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_RUNNING, 0);
                gpio_set_level(LED_READY, 0);
                gpio_set_level(LED_BLOCKED, 0);
                gpio_set_level(LED_SUSPENDED, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;
    }
}

// ตัวอย่าง task ที่จะ monitor state ของตัวเอง
void demo_task_ex2(void *pvParameters)
{
    while (1) {
        eTaskState current_state = eTaskGetState(NULL); // NULL = task ตัวเอง
        ESP_LOGI(TAG, "Current state: %s", get_state_name(current_state));
        update_state_display(current_state);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// app_main
void app_main(void)
{
    ESP_LOGI("MAIN", "Starting Exercise 2 LED State Indicator Demo");

    // GPIO Configuration
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_RUNNING) | (1ULL << LED_READY) |
                        (1ULL << LED_BLOCKED) | (1ULL << LED_SUSPENDED),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    // สร้าง task demo_task_ex2
    xTaskCreate(demo_task_ex2, "DemoEx2", 2048, NULL, 2, NULL);
}
