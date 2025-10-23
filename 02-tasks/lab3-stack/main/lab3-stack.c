#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#define LED_OK GPIO_NUM_2       // Stack OK indicator
#define LED_WARNING GPIO_NUM_4  // Stack warning indicator

static const char *TAG = "STACK_MONITOR";

// Stack monitoring thresholds
#define STACK_WARNING_THRESHOLD 512
#define STACK_CRITICAL_THRESHOLD 256

TaskHandle_t light_task_handle = NULL;
TaskHandle_t medium_task_handle = NULL;
TaskHandle_t heavy_task_handle = NULL;

// ============================ Step 2: Stack Overflow Detection ============================

// Stack overflow hook
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    ESP_LOGE("STACK_OVERFLOW", "Task %s has overflowed its stack!", pcTaskName);
    ESP_LOGE("STACK_OVERFLOW", "System will restart...");

    // Blink warning LED rapidly
    for (int i = 0; i < 20; i++) {
        gpio_set_level(LED_WARNING, 1);
        vTaskDelay(pdMS_TO_TICKS(25));
        gpio_set_level(LED_WARNING, 0);
        vTaskDelay(pdMS_TO_TICKS(25));
    }

    esp_restart();
}

// ============================ Step 3: Optimized Heavy Task ============================

void optimized_heavy_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Optimized Heavy Task started");

    // ใช้ heap แทน stack สำหรับ large data
    char *large_buffer = malloc(1024);
    int *large_numbers = malloc(200 * sizeof(int));
    char *another_buffer = malloc(512);

    if (!large_buffer || !large_numbers || !another_buffer) {
        ESP_LOGE(TAG, "Failed to allocate heap memory");
        free(large_buffer);
        free(large_numbers);
        free(another_buffer);
        vTaskDelete(NULL);
        return;
    }

    int cycle = 0;
    while (1) {
        cycle++;

        ESP_LOGI(TAG, "Cycle %d: Using heap instead of stack", cycle);

        // ใช้ heap memory
        memset(large_buffer, 'Y', 1023);
        large_buffer[1023] = '\0';

        for (int i = 0; i < 200; i++) {
            large_numbers[i] = i * cycle;
        }

        snprintf(another_buffer, 512, "Optimized cycle %d", cycle);

        // ตรวจสอบ stack usage
        UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Optimized Heavy Task stack: %d bytes remaining",
                 stack_remaining * sizeof(StackType_t));

        vTaskDelay(pdMS_TO_TICKS(4000));
    }

    // Clean up (จะไม่ถูกเรียก)
    free(large_buffer);
    free(large_numbers);
    free(another_buffer);
}

// ============================ Stack Monitor Task ============================

void stack_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Stack Monitor Task started");

    while (1) {
        ESP_LOGI(TAG, "\n=== STACK USAGE REPORT ===");

        TaskHandle_t tasks[] = {
            light_task_handle,
            medium_task_handle,
            heavy_task_handle,
            xTaskGetCurrentTaskHandle() // Monitor itself
        };

        const char *task_names[] = {
            "LightTask",
            "MediumTask",
            "HeavyTask",
            "StackMonitor"
        };

        bool stack_warning = false;
        bool stack_critical = false;

        for (int i = 0; i < 4; i++) {
            if (tasks[i] != NULL) {
                UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(tasks[i]);
                uint32_t stack_bytes = stack_remaining * sizeof(StackType_t);

                ESP_LOGI(TAG, "%s: %d bytes remaining", task_names[i], stack_bytes);

                if (stack_bytes < STACK_CRITICAL_THRESHOLD) {
                    ESP_LOGE(TAG, "CRITICAL: %s stack very low!", task_names[i]);
                    stack_critical = true;
                } else if (stack_bytes < STACK_WARNING_THRESHOLD) {
                    ESP_LOGW(TAG, "WARNING: %s stack low", task_names[i]);
                    stack_warning = true;
                }
            }
        }

        // Update LED indicators
        if (stack_critical) {
            for (int i = 0; i < 10; i++) {
                gpio_set_level(LED_WARNING, 1);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level(LED_WARNING, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            gpio_set_level(LED_OK, 0);
        } else if (stack_warning) {
            gpio_set_level(LED_WARNING, 1);
            gpio_set_level(LED_OK, 0);
        } else {
            gpio_set_level(LED_OK, 1);
            gpio_set_level(LED_WARNING, 0);
        }

        ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Min free heap: %d bytes", esp_get_minimum_free_heap_size());

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// ============================ Light and Medium Tasks ============================

void light_stack_task(void *pvParameters)
{
    int counter = 0;
    while (1) {
        counter++;
        ESP_LOGI(TAG, "Light task cycle: %d", counter);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void medium_stack_task(void *pvParameters)
{
    while (1) {
        char buffer[256];
        int numbers[50];
        memset(buffer, 'A', sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        for (int i = 0; i < 50; i++) numbers[i] = i * i;
        ESP_LOGI(TAG, "Medium task: buffer[0]=%c, numbers[49]=%d", buffer[0], numbers[49]);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// ============================ app_main ============================

void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Stack Monitoring & Optimization Demo ===");

    // GPIO Configuration
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_OK) | (1ULL << LED_WARNING),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    // Create tasks
    xTaskCreate(light_stack_task, "LightTask", 1024, NULL, 2, &light_task_handle);
    xTaskCreate(medium_stack_task, "MediumTask", 2048, NULL, 2, &medium_task_handle);
    xTaskCreate(optimized_heavy_task, "HeavyTask", 1024, NULL, 2, &heavy_task_handle); // ใช้ heap
    xTaskCreate(stack_monitor_task, "StackMonitor", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "All tasks created. Monitoring stack usage every 3 seconds.");
}
