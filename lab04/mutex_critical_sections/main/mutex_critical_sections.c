#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_random.h"

static const char *TAG = "MUTEX_EX3";

#define LED_TASK1 GPIO_NUM_2
#define LED_TASK2 GPIO_NUM_4
#define LED_TASK3 GPIO_NUM_5
#define LED_CRITICAL GPIO_NUM_18

SemaphoreHandle_t xMutex;

typedef struct {
    uint32_t counter;
    char shared_buffer[100];
    uint32_t checksum;
    uint32_t access_count;
} shared_resource_t;

shared_resource_t shared_data = {0, "", 0, 0};

uint32_t calculate_checksum(const char* data, uint32_t counter) {
    uint32_t sum = counter;
    for (int i = 0; data[i] != '\0'; i++) sum += (uint32_t)data[i] * (i + 1);
    return sum;
}

void access_shared_resource(const char* task_name, gpio_num_t led_pin) {
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
        gpio_set_level(led_pin, 1);
        gpio_set_level(LED_CRITICAL, 1);

        uint32_t temp = shared_data.counter;
        vTaskDelay(pdMS_TO_TICKS(500 + (esp_random() % 800)));
        shared_data.counter = temp + 1;
        snprintf(shared_data.shared_buffer, sizeof(shared_data.shared_buffer),
                 "Modified by %s #%lu", task_name, shared_data.counter);
        shared_data.checksum = calculate_checksum(shared_data.shared_buffer, shared_data.counter);
        shared_data.access_count++;

        ESP_LOGI(TAG, "[%s] Accessed shared resource → Counter=%lu",
                 task_name, shared_data.counter);

        gpio_set_level(led_pin, 0);
        gpio_set_level(LED_CRITICAL, 0);
        xSemaphoreGive(xMutex);
    } else {
        ESP_LOGW(TAG, "[%s] Failed to acquire mutex!", task_name);
    }
}

void high_task(void *pv) {
    while (1) {
        access_shared_resource("HIGH_PRI", LED_TASK1);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void med_task(void *pv) {
    while (1) {
        access_shared_resource("MED_PRI", LED_TASK2);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
void low_task(void *pv) {
    while (1) {
        access_shared_resource("LOW_PRI", LED_TASK3);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void monitor_task(void *pv) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "\n==== PRIORITY CHANGE MONITOR ====");
        ESP_LOGI(TAG, "Counter: %lu", shared_data.counter);
        ESP_LOGI(TAG, "Buffer: '%s'", shared_data.shared_buffer);
        ESP_LOGI(TAG, "Access Count: %lu\n", shared_data.access_count);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Experiment 3: Priority Change Started");

    gpio_set_direction(LED_TASK1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TASK2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TASK3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_CRITICAL, GPIO_MODE_OUTPUT);

    xMutex = xSemaphoreCreateMutex();

    if (xMutex) {
        strcpy(shared_data.shared_buffer, "Initial");
        shared_data.checksum = calculate_checksum(shared_data.shared_buffer, 0);

        // 🔁 สลับ priority: high -> 2, low -> 5
        xTaskCreate(high_task, "High", 3072, NULL, 2, NULL);
        xTaskCreate(med_task, "Med", 3072, NULL, 3, NULL);
        xTaskCreate(low_task, "Low", 3072, NULL, 5, NULL);
        xTaskCreate(monitor_task, "Mon", 3072, NULL, 1, NULL);

        ESP_LOGI(TAG, "Tasks created with new priorities:");
        ESP_LOGI(TAG, "  Low Priority Task: 5");
        ESP_LOGI(TAG, "  Medium Priority:   3");
        ESP_LOGI(TAG, "  High Priority:     2");
    }
}
