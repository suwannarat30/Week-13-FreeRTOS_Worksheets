#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include <stdbool.h>

static const char *TAG = "EVENT_GROUPS";

// ------------------------
// GPIO สำหรับแสดงสถานะ
// ------------------------
#define LED_NETWORK_READY   GPIO_NUM_2
#define LED_SENSOR_READY    GPIO_NUM_4
#define LED_CONFIG_READY    GPIO_NUM_5
#define LED_STORAGE_READY   GPIO_NUM_18
#define LED_SYSTEM_READY    GPIO_NUM_19

// ------------------------
// Event Group และ Event Bits
// ------------------------
EventGroupHandle_t system_events;

#define NETWORK_READY_BIT   (1 << 0)
#define SENSOR_READY_BIT    (1 << 1)
#define CONFIG_READY_BIT    (1 << 2)
#define STORAGE_READY_BIT   (1 << 3)
#define SYSTEM_READY_BIT    (1 << 4)

#define BASIC_SYSTEM_BITS   (NETWORK_READY_BIT | CONFIG_READY_BIT)
#define ALL_SUBSYSTEM_BITS  (NETWORK_READY_BIT | SENSOR_READY_BIT | CONFIG_READY_BIT | STORAGE_READY_BIT)
#define FULL_SYSTEM_BITS    (ALL_SUBSYSTEM_BITS | SYSTEM_READY_BIT)

// ------------------------
// Event statistics struct
// ------------------------
typedef struct {
    uint32_t network_init_time;
    uint32_t sensor_init_time;
    uint32_t config_init_time;
    uint32_t storage_init_time;
    uint32_t total_init_time;
    uint32_t event_notifications;
} system_stats_t;

static system_stats_t stats = {0};

// ------------------------
// Event Bit Mapping
// ------------------------
typedef struct {
    EventBits_t bit;
    const char* name;
    const char* description;
    bool is_critical;
} event_bit_info_t;

static const event_bit_info_t event_map[] = {
    {NETWORK_READY_BIT, "Network", "Network connectivity", true},
    {SENSOR_READY_BIT, "Sensor", "Sensor subsystem", false},
    {CONFIG_READY_BIT, "Config", "Configuration loaded", true},
    {STORAGE_READY_BIT, "Storage", "Storage system", false},
    {SYSTEM_READY_BIT, "System", "Full system ready", true}
};

// ------------------------
// Event utilities
// ------------------------
void print_event_map(void) {
    ESP_LOGI(TAG, "\n📋 EVENT BIT MAPPING");
    for (size_t i = 0; i < sizeof(event_map)/sizeof(event_map[0]); i++) {
        ESP_LOGI(TAG, "Bit: 0x%02X | Name: %s | Desc: %s | Critical: %s",
                 event_map[i].bit,
                 event_map[i].name,
                 event_map[i].description,
                 event_map[i].is_critical ? "YES" : "NO");
    }
}

void print_event_statistics(void) {
    ESP_LOGI(TAG, "\n📈 EVENT STATISTICS");
    ESP_LOGI(TAG, "Total notifications: %lu", stats.event_notifications);
    ESP_LOGI(TAG, "System uptime: %lu ms", xTaskGetTickCount() * portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Event rate: %.2f events/min", 
             (float)stats.event_notifications * 60000.0 / 
             (xTaskGetTickCount() * portTICK_PERIOD_MS));
}

void debug_event_bits(EventBits_t bits, const char* context) {
    ESP_LOGI(TAG, "🐛 DEBUG %s - Event bits: 0x%08X", context, bits);
    ESP_LOGI(TAG, "  Network: %s", (bits & NETWORK_READY_BIT) ? "SET" : "CLEAR");
    ESP_LOGI(TAG, "  Sensor:  %s", (bits & SENSOR_READY_BIT) ? "SET" : "CLEAR");
    ESP_LOGI(TAG, "  Config:  %s", (bits & CONFIG_READY_BIT) ? "SET" : "CLEAR");
    ESP_LOGI(TAG, "  Storage: %s", (bits & STORAGE_READY_BIT) ? "SET" : "CLEAR");
    ESP_LOGI(TAG, "  System:  %s", (bits & SYSTEM_READY_BIT) ? "SET" : "CLEAR");
}

// Advanced utility: wait with retry
EventBits_t wait_for_events_with_retry(EventGroupHandle_t group, 
                                      EventBits_t bits_to_wait,
                                      bool wait_all, 
                                      TickType_t timeout,
                                      int max_retries) {
    for (int retry = 0; retry < max_retries; retry++) {
        EventBits_t result = xEventGroupWaitBits(group, bits_to_wait, pdFALSE, wait_all, timeout);
        if ((wait_all && (result & bits_to_wait) == bits_to_wait) || (!wait_all && (result & bits_to_wait))) {
            return result;
        }
        ESP_LOGW(TAG, "Event wait retry %d/%d", retry + 1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return 0; // Failed after all retries
}

// ------------------------
// Subsystem tasks (Network, Sensor, Config, Storage)
// ------------------------
void network_init_task(void *pvParameters) {
    ESP_LOGI(TAG, "🌐 Network initialization started");
    uint32_t start_time = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(6000));
    vTaskDelay(pdMS_TO_TICKS(6000));
    vTaskDelay(pdMS_TO_TICKS(4000));

    stats.network_init_time = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;

    gpio_set_level(LED_NETWORK_READY, 1);
    xEventGroupSetBits(system_events, NETWORK_READY_BIT);
    ESP_LOGI(TAG, "✅ Network ready! (took %lu ms)", stats.network_init_time);

    while(1) vTaskDelay(pdMS_TO_TICKS(5000));
}

void sensor_init_task(void *pvParameters) {
    ESP_LOGI(TAG, "🌡️ Sensor initialization started");
    uint32_t start_time = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(3000));
    vTaskDelay(pdMS_TO_TICKS(5000));
    vTaskDelay(pdMS_TO_TICKS(5000));
    vTaskDelay(pdMS_TO_TICKS(2000));

    stats.sensor_init_time = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;

    gpio_set_level(LED_SENSOR_READY, 1);
    xEventGroupSetBits(system_events, SENSOR_READY_BIT);
    ESP_LOGI(TAG, "✅ Sensors ready! (took %lu ms)", stats.sensor_init_time);

    while(1) vTaskDelay(pdMS_TO_TICKS(3000));
}

void config_load_task(void *pvParameters) {
    ESP_LOGI(TAG, "⚙️ Configuration loading started");
    uint32_t start_time = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(5000));
    vTaskDelay(pdMS_TO_TICKS(4000));
    vTaskDelay(pdMS_TO_TICKS(3000));
    vTaskDelay(pdMS_TO_TICKS(3000));

    stats.config_init_time = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;

    gpio_set_level(LED_CONFIG_READY, 1);
    xEventGroupSetBits(system_events, CONFIG_READY_BIT);
    ESP_LOGI(TAG, "✅ Configuration loaded! (took %lu ms)", stats.config_init_time);

    while(1) vTaskDelay(pdMS_TO_TICKS(8000));
}

void storage_init_task(void *pvParameters) {
    ESP_LOGI(TAG, "💾 Storage initialization started");
    uint32_t start_time = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(5000));
    vTaskDelay(pdMS_TO_TICKS(5000));
    vTaskDelay(pdMS_TO_TICKS(3000));
    vTaskDelay(pdMS_TO_TICKS(4000));

    stats.storage_init_time = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;

    gpio_set_level(LED_STORAGE_READY, 1);
    xEventGroupSetBits(system_events, STORAGE_READY_BIT);
    ESP_LOGI(TAG, "✅ Storage ready! (took %lu ms)", stats.storage_init_time);

    while(1) vTaskDelay(pdMS_TO_TICKS(10000));
}

// ------------------------
// System coordinator task
// ------------------------
void system_coordinator_task(void *pvParameters) {
    uint32_t total_start_time = xTaskGetTickCount();

    ESP_LOGI(TAG, "🎛️ System coordinator started - waiting for subsystems...");

    EventBits_t bits = xEventGroupWaitBits(system_events, BASIC_SYSTEM_BITS, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    debug_event_bits(bits, "Phase 1");

    if ((bits & BASIC_SYSTEM_BITS) == BASIC_SYSTEM_BITS) stats.event_notifications++;

    bits = xEventGroupWaitBits(system_events, ALL_SUBSYSTEM_BITS, pdFALSE, pdTRUE, pdMS_TO_TICKS(15000));
    debug_event_bits(bits, "Phase 2");

    if ((bits & ALL_SUBSYSTEM_BITS) == ALL_SUBSYSTEM_BITS) {
        xEventGroupSetBits(system_events, SYSTEM_READY_BIT);
        gpio_set_level(LED_SYSTEM_READY, 1);
        stats.total_init_time = (xTaskGetTickCount() - total_start_time) * portTICK_PERIOD_MS;
    }

    while(1) {
        EventBits_t current_bits = xEventGroupGetBits(system_events);
        debug_event_bits(current_bits, "Monitoring");
        print_event_statistics();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// ------------------------
// Event monitor task
// ------------------------
void event_monitor_task(void *pvParameters) {
    while(1) {
        EventBits_t bits = wait_for_events_with_retry(system_events, ALL_SUBSYSTEM_BITS, false, pdMS_TO_TICKS(5000), 3);
        if (bits) stats.event_notifications++;
        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}

// ------------------------
// app_main
// ------------------------
void app_main(void) {
    gpio_set_direction(LED_NETWORK_READY, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_SENSOR_READY, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_CONFIG_READY, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_STORAGE_READY, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_SYSTEM_READY, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_NETWORK_READY, 0);
    gpio_set_level(LED_SENSOR_READY, 0);
    gpio_set_level(LED_CONFIG_READY, 0);
    gpio_set_level(LED_STORAGE_READY, 0);
    gpio_set_level(LED_SYSTEM_READY, 0);

    system_events = xEventGroupCreate();
    if (!system_events) return;

    print_event_map();  // <-- เรียกใช้เพื่อแก้ unused variable

    xTaskCreate(network_init_task, "NetworkInit", 3072, NULL, 6, NULL);
    xTaskCreate(sensor_init_task, "SensorInit", 2048, NULL, 5, NULL);
    xTaskCreate(config_load_task, "ConfigLoad", 2048, NULL, 4, NULL);
    xTaskCreate(storage_init_task, "StorageInit", 2048, NULL, 4, NULL);
    xTaskCreate(system_coordinator_task, "SysCoord", 3072, NULL, 8, NULL);
    xTaskCreate(event_monitor_task, "EventMon", 2048, NULL, 3, NULL);
}
