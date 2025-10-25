#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_random.h"

static const char *TAG = "BINARY_SEM_EXP3";

// -------------------------
// Hardware Configuration
// -------------------------
#define LED_PRODUCER GPIO_NUM_2
#define LED_CONSUMER GPIO_NUM_4
#define LED_TIMER    GPIO_NUM_5
#define BUTTON_PIN   GPIO_NUM_0

// -------------------------
// Semaphore and Timer
// -------------------------
SemaphoreHandle_t xBinarySemaphore;
SemaphoreHandle_t xTimerSemaphore;
SemaphoreHandle_t xButtonSemaphore;
gptimer_handle_t gptimer = NULL;

// -------------------------
// Statistics Structure
// -------------------------
typedef struct {
    uint32_t signals_sent;
    uint32_t signals_received;
    uint32_t timer_events;
    uint32_t button_presses;
} semaphore_stats_t;

semaphore_stats_t stats = {0, 0, 0, 0};

// -------------------------
// Timer ISR Callback
// -------------------------
static bool IRAM_ATTR timer_callback(gptimer_handle_t timer,
                                    const gptimer_alarm_event_data_t *edata,
                                    void *user_data) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xTimerSemaphore, &xHigherPriorityTaskWoken);
    return xHigherPriorityTaskWoken == pdTRUE;
}

// -------------------------
// Button ISR Handler
// -------------------------
static void IRAM_ATTR button_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// -------------------------
// Producer Task
// -------------------------
void producer_task(void *pvParameters) {
    int event_counter = 0;
    ESP_LOGI(TAG, "Producer task started");

    while (1) {
        // Generate event every 2–5 seconds
        vTaskDelay(pdMS_TO_TICKS(2000 + (esp_random() % 3000)));
        event_counter++;
        ESP_LOGI(TAG, "🔥 Producer: Generating event #%d", event_counter);

        // Give semaphore
        if (xSemaphoreGive(xBinarySemaphore) == pdTRUE) {
            stats.signals_sent++;
            ESP_LOGI(TAG, "✓ Producer: Event signaled successfully");

            // Blink LED
            gpio_set_level(LED_PRODUCER, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_PRODUCER, 0);
        } else {
            ESP_LOGW(TAG, "✗ Producer: Semaphore already given");
        }
    }
}

// -------------------------
// Consumer Task (Short Timeout)
// -------------------------
void consumer_task(void *pvParameters) {
    ESP_LOGI(TAG, "Consumer task started (Short Timeout: 3 seconds)");

    while (1) {
        ESP_LOGI(TAG, "🔍 Waiting for event (max 3s)...");
        if (xSemaphoreTake(xBinarySemaphore, pdMS_TO_TICKS(3000)) == pdTRUE) {
            stats.signals_received++;
            ESP_LOGI(TAG, "⚡ Consumer: Event received and processing...");

            gpio_set_level(LED_CONSUMER, 1);
            vTaskDelay(pdMS_TO_TICKS(1000 + (esp_random() % 2000)));  // process 1–3s
            gpio_set_level(LED_CONSUMER, 0);

            ESP_LOGI(TAG, "✓ Consumer: Event processed");
        } else {
            ESP_LOGW(TAG, "⏰ Consumer: Timeout (no event within 3s)");
        }
    }
}

// -------------------------
// Timer Event Task
// -------------------------
void timer_event_task(void *pvParameters) {
    ESP_LOGI(TAG, "Timer event task started");

    while (1) {
        if (xSemaphoreTake(xTimerSemaphore, portMAX_DELAY) == pdTRUE) {
            stats.timer_events++;
            ESP_LOGI(TAG, "⏱️ Timer event #%lu", stats.timer_events);

            gpio_set_level(LED_TIMER, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_TIMER, 0);

            if (stats.timer_events % 5 == 0) {
                ESP_LOGI(TAG, "📊 Stats - Sent:%lu, Received:%lu, Timer:%lu, Button:%lu",
                        stats.signals_sent, stats.signals_received,
                        stats.timer_events, stats.button_presses);
            }
        }
    }
}

// -------------------------
// Button Event Task
// -------------------------
void button_event_task(void *pvParameters) {
    ESP_LOGI(TAG, "Button event task started");

    while (1) {
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE) {
            stats.button_presses++;
            ESP_LOGI(TAG, "🔘 Button pressed #%lu", stats.button_presses);

            vTaskDelay(pdMS_TO_TICKS(300)); // debounce
            ESP_LOGI(TAG, "🚀 Button: Trigger immediate producer event");

            xSemaphoreGive(xBinarySemaphore);
            stats.signals_sent++;
        }
    }
}

// -------------------------
// System Monitor Task
// -------------------------
void monitor_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));

        ESP_LOGI(TAG, "\n════ SEMAPHORE SYSTEM MONITOR ════");
        ESP_LOGI(TAG, "Binary Semaphore Available: %s",
                 uxSemaphoreGetCount(xBinarySemaphore) ? "YES" : "NO");
        ESP_LOGI(TAG, "Timer Semaphore Count: %d", uxSemaphoreGetCount(xTimerSemaphore));
        ESP_LOGI(TAG, "Button Semaphore Count: %d", uxSemaphoreGetCount(xButtonSemaphore));

        ESP_LOGI(TAG, "Event Statistics:");
        ESP_LOGI(TAG, "  Producer Events: %lu", stats.signals_sent);
        ESP_LOGI(TAG, "  Consumer Events: %lu", stats.signals_received);
        ESP_LOGI(TAG, "  Timer Events:    %lu", stats.timer_events);
        ESP_LOGI(TAG, "  Button Presses:  %lu", stats.button_presses);

        float efficiency = stats.signals_sent > 0 ?
                           (float)stats.signals_received / stats.signals_sent * 100 : 0;
        ESP_LOGI(TAG, "  System Efficiency: %.1f%%", efficiency);
        ESP_LOGI(TAG, "══════════════════════════════════\n");
    }
}

// -------------------------
// Main Application Entry
// -------------------------
void app_main(void) {
    ESP_LOGI(TAG, "Binary Semaphores Experiment 3 - Short Timeout");

    // Configure GPIO
    gpio_set_direction(LED_PRODUCER, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_CONSUMER, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_TIMER, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);

    gpio_set_level(LED_PRODUCER, 0);
    gpio_set_level(LED_CONSUMER, 0);
    gpio_set_level(LED_TIMER, 0);

    // Create Semaphores
    xBinarySemaphore = xSemaphoreCreateBinary();
    xTimerSemaphore = xSemaphoreCreateBinary();
    xButtonSemaphore = xSemaphoreCreateBinary();

    if (xBinarySemaphore && xTimerSemaphore && xButtonSemaphore) {
        ESP_LOGI(TAG, "Semaphores created successfully");

        // Install button ISR
        gpio_install_isr_service(0);
        gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

        // Configure Timer (8-second interval)
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 1000000,
        };
        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

        gptimer_event_callbacks_t cbs = {.on_alarm = timer_callback};
        ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
        ESP_ERROR_CHECK(gptimer_enable(gptimer));

        gptimer_alarm_config_t alarm_config = {
            .alarm_count = 8000000,
            .flags.auto_reload_on_alarm = true,
        };
        ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
        ESP_ERROR_CHECK(gptimer_start(gptimer));

        // Create Tasks
        xTaskCreate(producer_task, "Producer", 2048, NULL, 3, NULL);
        xTaskCreate(consumer_task, "Consumer", 2048, NULL, 2, NULL);
        xTaskCreate(timer_event_task, "TimerEvent", 2048, NULL, 2, NULL);
        xTaskCreate(button_event_task, "ButtonEvent", 2048, NULL, 4, NULL);
        xTaskCreate(monitor_task, "Monitor", 2048, NULL, 1, NULL);

        ESP_LOGI(TAG, "System started successfully!");
        ESP_LOGI(TAG, "💡 Press BOOT button (GPIO0) to trigger immediate events!");
    } else {
        ESP_LOGE(TAG, "Failed to create semaphores!");
    }
}
