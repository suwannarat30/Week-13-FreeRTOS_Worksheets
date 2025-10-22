#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define LED1_PIN GPIO_NUM_2   // Task 1 indicator
#define LED2_PIN GPIO_NUM_4   // Task 2 indicator
#define LED3_PIN GPIO_NUM_5   // Emergency indicator
#define BUTTON_PIN GPIO_NUM_0 // Emergency button (Active-Low)

static const char *TAG = "COOPERATIVE";

// Global variables for cooperative scheduling
static volatile bool emergency_flag = false;
static uint64_t task_start_time = 0;
static uint32_t max_response_time = 0;

// Task structure for cooperative scheduling
typedef struct {
    void (*task_function)(void);
    const char* name;
    bool ready;
} coop_task_t;

// ------------------------ Task Definitions ------------------------

void cooperative_task1(void)
{
    static uint32_t count = 0;
    ESP_LOGI(TAG, "Coop Task1 running: %d", count++);
    gpio_set_level(LED1_PIN, 1);

    // Simulate work with voluntary yielding
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 50000; j++) {
            volatile int dummy = j * 2;
        }

        // Emergency yield point
        if (emergency_flag) {
            ESP_LOGW(TAG, "Task1 yielding for emergency");
            gpio_set_level(LED1_PIN, 0);
            return;
        }

        // Voluntary yield
        vTaskDelay(1);
    }

    gpio_set_level(LED1_PIN, 0);
}

void cooperative_task2(void)
{
    static uint32_t count = 0;
    ESP_LOGI(TAG, "Coop Task2 running: %d", count++);
    gpio_set_level(LED2_PIN, 1);

    // Simulate longer task with yield points
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 30000; j++) {
            volatile int dummy = j + i;
        }

        if (emergency_flag) {
            ESP_LOGW(TAG, "Task2 yielding for emergency");
            gpio_set_level(LED2_PIN, 0);
            return;
        }

        vTaskDelay(1);
    }

    gpio_set_level(LED2_PIN, 0);
}

void cooperative_task3_emergency(void)
{
    if (emergency_flag) {
        uint64_t response_time = esp_timer_get_time() - task_start_time;
        uint32_t response_ms = (uint32_t)(response_time / 1000);

        if (response_ms > max_response_time) {
            max_response_time = response_ms;
        }

        ESP_LOGW(TAG, "EMERGENCY RESPONSE! Response time: %d ms (Max: %d ms)",
                 response_ms, max_response_time);

        gpio_set_level(LED3_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(LED3_PIN, 0);

        emergency_flag = false;
    }
}

// ------------------------ Scheduler ------------------------

void cooperative_scheduler(void)
{
    coop_task_t tasks[] = {
        {cooperative_task1, "Task1", true},
        {cooperative_task2, "Task2", true},
        {cooperative_task3_emergency, "Emergency", true}
    };

    int num_tasks = sizeof(tasks) / sizeof(tasks[0]);
    int current_task = 0;

    while (1) {
        // Check button press (active-low)
        if (gpio_get_level(BUTTON_PIN) == 0 && !emergency_flag) {
            emergency_flag = true;
            task_start_time = esp_timer_get_time();
            ESP_LOGW(TAG, "Emergency button pressed!");
        }

        // Run current task
        if (tasks[current_task].ready) {
            tasks[current_task].task_function();
        }

        // Move to next task
        current_task = (current_task + 1) % num_tasks;

        // Small delay to prevent watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ------------------------ Entry Function ------------------------

void test_cooperative_multitasking(void)
{
    ESP_LOGI(TAG, "=== Cooperative Multitasking Demo ===");
    ESP_LOGI(TAG, "Tasks yield voluntarily");
    ESP_LOGI(TAG, "Press button to trigger emergency task");

    cooperative_scheduler();
}

// ------------------------ app_main ------------------------

void app_main(void)
{
    // Setup GPIO
    gpio_set_direction(LED1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED2_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED3_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(BUTTON_PIN); // enable pull-up for active-low button

    // Run the demo
    test_cooperative_multitasking();
}
