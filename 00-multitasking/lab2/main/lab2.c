// main.c
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define LED1_PIN GPIO_NUM_2
#define LED2_PIN GPIO_NUM_4
#define LED3_PIN GPIO_NUM_5
#define LED4_PIN GPIO_NUM_18

static const char *TAG = "TIME_SHARING";

// Task states
typedef enum {
    TASK_SENSOR,
    TASK_PROCESS,
    TASK_ACTUATOR,
    TASK_DISPLAY,
    TASK_COUNT
} task_id_t;

// Default time slice in milliseconds
#define TIME_SLICE_MS 50

// Globals for scheduler stats
static volatile uint32_t task_counter = 0;
static volatile uint64_t context_switch_time = 0; // in microseconds
static volatile uint32_t context_switches = 0;

static volatile bool run_manual_scheduler = true;
static volatile uint32_t current_time_slice_ms = TIME_SLICE_MS;

/* --------------------------
   Simulated task workloads
   -------------------------- */
void simulate_sensor_task(void)
{
    static uint32_t sensor_count = 0;
    ESP_LOGI(TAG, "Sensor Task %d", sensor_count++);
    gpio_set_level(LED1_PIN, 1);

    // light work
    for (int i = 0; i < 10000; i++) {
        volatile int dummy = i;
        (void)dummy;
    }

    gpio_set_level(LED1_PIN, 0);
}

void simulate_processing_task(void)
{
    static uint32_t process_count = 0;
    ESP_LOGI(TAG, "Processing Task %d", process_count++);
    gpio_set_level(LED2_PIN, 1);

    // heavy computation
    for (int i = 0; i < 100000; i++) {
        volatile int dummy = i * i;
        (void)dummy;
    }

    gpio_set_level(LED2_PIN, 0);
}

void simulate_actuator_task(void)
{
    static uint32_t actuator_count = 0;
    ESP_LOGI(TAG, "Actuator Task %d", actuator_count++);
    gpio_set_level(LED3_PIN, 1);

    // medium work
    for (int i = 0; i < 50000; i++) {
        volatile int dummy = i + 100;
        (void)dummy;
    }

    gpio_set_level(LED3_PIN, 0);
}

void simulate_display_task(void)
{
    static uint32_t display_count = 0;
    ESP_LOGI(TAG, "Display Task %d", display_count++);
    gpio_set_level(LED4_PIN, 1);

    // quick work
    for (int i = 0; i < 20000; i++) {
        volatile int dummy = i / 2;
        (void)dummy;
    }

    gpio_set_level(LED4_PIN, 0);
}

/* --------------------------
   Manual scheduler (single-cycle)
   -------------------------- */
void manual_scheduler(void)
{
    uint64_t start_time = esp_timer_get_time();

    // Simulate saving context
    context_switches++;

    // Context switch overhead simulation (first part)
    for (int i = 0; i < 1000; i++) {
        volatile int dummy = i;
        (void)dummy;
    }

    // Execute current task
    switch (task_counter % TASK_COUNT) {
        case TASK_SENSOR:
            simulate_sensor_task();
            break;
        case TASK_PROCESS:
            simulate_processing_task();
            break;
        case TASK_ACTUATOR:
            simulate_actuator_task();
            break;
        case TASK_DISPLAY:
            simulate_display_task();
            break;
        default:
            break;
    }

    // Context switch overhead simulation (second part)
    for (int i = 0; i < 1000; i++) {
        volatile int dummy = i;
        (void)dummy;
    }

    uint64_t end_time = esp_timer_get_time();
    // accumulate only the duration of this manual scheduling invocation
    context_switch_time += (end_time - start_time);

    task_counter++;
}

/* --------------------------
   Manual scheduler task (FreeRTOS)
   Runs in background when enabled
   -------------------------- */
void manual_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Manual scheduler task started (time slice %d ms)", (int)current_time_slice_ms);

    while (1) {
        if (run_manual_scheduler) {
            manual_scheduler();
            // wait for current time slice
            vTaskDelay(pdMS_TO_TICKS(current_time_slice_ms));
            // report statistics every 20 context switches
            if (context_switches > 0 && (context_switches % 20) == 0) {
                static uint32_t round_count = 0;
                round_count++;
                uint64_t total_time = esp_timer_get_time(); // since boot
                // avoid division by zero
                float cpu_utilization = 0.0f;
                if (total_time > 0) {
                    cpu_utilization = ((float)(context_switch_time) / (float)total_time) * 100.0f;
                }
                float overhead_percentage = 100.0f - cpu_utilization;
                int64_t avg_per_task_us = (context_switches > 0) ? (int64_t)(context_switch_time / context_switches) : 0;

                ESP_LOGI(TAG, "=== Round %d Statistics ===", round_count);
                ESP_LOGI(TAG, "Context switches: %d", context_switches);
                ESP_LOGI(TAG, "Total time (since boot): %lld us", (long long)total_time);
                ESP_LOGI(TAG, "Accumulated task+overhead time: %lld us", (long long)context_switch_time);
                ESP_LOGI(TAG, "CPU utilization (accumulated/total): %.1f%%", cpu_utilization);
                ESP_LOGI(TAG, "Overhead estimate: %.1f%%", overhead_percentage);
                ESP_LOGI(TAG, "Avg time per task invocation: %lld us", (long long)avg_per_task_us);
            }
        } else {
            // scheduler paused during experiments - sleep a bit
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/* --------------------------
   Variable time slice experiment (Part 2)
   -------------------------- */
void variable_time_slice_experiment(void)
{
    ESP_LOGI(TAG, "\n=== Variable Time Slice Experiment ===");

    // Test different time slices (ms)
    uint32_t time_slices[] = {10, 25, 50, 100, 200};
    int num_slices = sizeof(time_slices) / sizeof(time_slices[0]);

    // Pause the background manual scheduler so we can run controlled tests
    run_manual_scheduler = false;

    for (int i = 0; i < num_slices; i++) {
        uint32_t ts = time_slices[i];
        ESP_LOGI(TAG, "Testing time slice: %d ms", ts);

        // Reset counters for this test
        context_switches = 0;
        context_switch_time = 0;
        task_counter = 0;

        uint64_t test_start = esp_timer_get_time();

        // Run for ~5 seconds: we'll perform 50 iterations of (manual_scheduler + vTaskDelay(ts))
        int iterations = 50;
        for (int j = 0; j < iterations; j++) {
            manual_scheduler();
            vTaskDelay(pdMS_TO_TICKS(ts));
        }

        uint64_t test_end = esp_timer_get_time();
        uint64_t test_duration = test_end - test_start; // in us

        float efficiency = 0.0f;
        if (test_duration > 0) {
            efficiency = ((float)context_switch_time / (float)test_duration) * 100.0f;
        }

        ESP_LOGI(TAG, "Time slice %d ms: Efficiency (work/time) %.1f%%", ts, efficiency);
        ESP_LOGI(TAG, "Context switches: %d", context_switches);
        ESP_LOGI(TAG, "Test duration: %lld us", (long long)test_duration);
        ESP_LOGI(TAG, "Accumulated task+local overhead time: %lld us", (long long)context_switch_time);
        ESP_LOGI(TAG, "Avg time per invocation: %lld us", (long long)((context_switches>0)?(context_switch_time/context_switches):0));

        // small pause between tests
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Variable time slice experiment completed.");

    // Resume manual scheduler with default time slice
    current_time_slice_ms = TIME_SLICE_MS;
    run_manual_scheduler = true;
}

/* --------------------------
   Demonstrate common problems (Part 3)
   -------------------------- */
void demonstrate_problems(void)
{
    ESP_LOGI(TAG, "\n=== Demonstrating Time-Sharing Problems ===");

    // Problem 1: Priority inversion (manual scheduler has no priorties)
    ESP_LOGI(TAG, "Problem 1: No priority support -> critical task may wait");

    // Problem 2: Fixed time slice issues
    ESP_LOGI(TAG, "Problem 2: Fixed time slice -> short tasks waste relative slice, long tasks get interrupted");

    // Problem 3: Context switching overhead
    ESP_LOGI(TAG, "Problem 3: Context switching overhead -> time wasted switching");

    // Problem 4: No inter-task communication / synchronization
    ESP_LOGI(TAG, "Problem 4: No inter-task communication -> tasks cannot safely coordinate");
}

/* --------------------------
   app_main
   -------------------------- */
void app_main(void)
{
    // Configure GPIO outputs for LEDs
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        // pin_bit_mask expects a 64-bit mask; cast enum to (uint64_t) is safe
        .pin_bit_mask = (1ULL << (uint64_t)LED1_PIN) |
                        (1ULL << (uint64_t)LED2_PIN) |
                        (1ULL << (uint64_t)LED3_PIN) |
                        (1ULL << (uint64_t)LED4_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Time-Sharing System Started");
    ESP_LOGI(TAG, "Default time slice: %d ms", TIME_SLICE_MS);

    // Create manual scheduler task (background)
    xTaskCreate(&manual_task, "manual_task", 4096, NULL, 5, NULL);

    // Allow manual scheduler to run a bit to show normal behavior
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Run variable time slice experiment (Part 2)
    variable_time_slice_experiment();

    // Demonstrate problems (Part 3)
    demonstrate_problems();

    // After experiments, keep running manual scheduler in background indefinitely.
    // app_main returns and FreeRTOS continues.
}

