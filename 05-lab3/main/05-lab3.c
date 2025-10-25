#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

static const char *TAG = "ADV_DIST_SCHED";

// ================= CONFIG =================
#define MAX_TIMERS          5
#define HEARTBEAT_INTERVAL  1000  // ms
#define ADAPTIVE_THRESHOLD  80    // % CPU load
#define STATUS_LED          GPIO_NUM_2
#define PERFORMANCE_WINDOW  10    // แก้ undeclared

// ================= DATA STRUCTURES =================
typedef struct {
    TimerHandle_t handle;
    char name[16];
    uint32_t period_ms;
    uint8_t priority;
    TickType_t deadline_ticks;
    TickType_t last_exec;
    bool active;
} dist_timer_t;

typedef struct {
    char timer_name[16];
    uint32_t period_ms;
    uint32_t node_id;
} timer_sync_msg_t;

// ================= GLOBALS =================
dist_timer_t timers[MAX_TIMERS];
SemaphoreHandle_t timer_mutex;
uint8_t master_mac[6] = {0x24,0x6F,0x28,0xAA,0xBB,0xCC}; // Example master MAC
bool is_master = true;  // Set false for client nodes

// Performance monitoring
uint32_t perf_window[PERFORMANCE_WINDOW][MAX_TIMERS] = {0};
uint32_t perf_index[MAX_TIMERS] = {0};

// ================= UTILS =================
TickType_t current_tick() { return xTaskGetTickCount(); }

// ================= TIMER CALLBACK =================
void timer_callback(TimerHandle_t t) {
    dist_timer_t *timer = (dist_timer_t*) pvTimerGetTimerID(t);
    TickType_t start = current_tick();

    // Simulate work
    volatile uint32_t work = 100 + (esp_random() % 500);
    for (volatile uint32_t i=0;i<work;i++);

    TickType_t end = current_tick();

    // Deadline check
    if (timer->deadline_ticks > 0 && (end - timer->last_exec) > timer->deadline_ticks) {
        ESP_LOGW(TAG, "⚠️ Timer '%s' missed deadline!", timer->name);
    }

    timer->last_exec = end;

    // Adaptive performance: increase period if callback duration high
    perf_window[perf_index[timer - timers]][timer - timers] = (end-start)*(1000000/configTICK_RATE_HZ);
    perf_index[timer - timers] = (perf_index[timer - timers]+1) % PERFORMANCE_WINDOW;

    uint32_t avg = 0;
    for (int i=0;i<PERFORMANCE_WINDOW;i++) avg += perf_window[i][timer - timers];
    avg /= PERFORMANCE_WINDOW;

    if (avg > timer->period_ms*1000*0.8) {
        timer->period_ms += 10;
        xTimerChangePeriod(timer->handle, pdMS_TO_TICKS(timer->period_ms), 0);
        ESP_LOGI(TAG, "Adaptive: Timer '%s' period increased to %d ms", timer->name, timer->period_ms);
    }

    // LED heartbeat
    gpio_set_level(STATUS_LED, (end/10)%2);
}

// ================= CREATE TIMER =================
void create_timer(const char* name, uint32_t period_ms, uint8_t priority, TickType_t deadline_ms) {
    if (xSemaphoreTake(timer_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    for (int i=0;i<MAX_TIMERS;i++) {
        if (!timers[i].active) {
            strncpy(timers[i].name, name, sizeof(timers[i].name)-1);
            timers[i].period_ms = period_ms;
            timers[i].priority = priority;
            timers[i].deadline_ticks = pdMS_TO_TICKS(deadline_ms);
            timers[i].last_exec = current_tick();
            timers[i].active = true;
            timers[i].handle = xTimerCreate(name, pdMS_TO_TICKS(period_ms), pdTRUE, &timers[i], timer_callback);
            if (timers[i].handle) xTimerStart(timers[i].handle, 0);
            ESP_LOGI(TAG, "Created timer '%s' period %d ms priority %d", name, period_ms, priority);
            break;
        }
    }

    xSemaphoreGive(timer_mutex);
}

// ================= ESP-NOW CALLBACK =================
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (!data || len != sizeof(timer_sync_msg_t)) return;
    timer_sync_msg_t msg;
    memcpy(&msg, data, sizeof(msg));
    ESP_LOGI(TAG, "Received timer sync: %s period %d node %lu", msg.timer_name, msg.period_ms, msg.node_id);

    for (int i=0;i<MAX_TIMERS;i++) {
        if (timers[i].active && strcmp(timers[i].name, msg.timer_name)==0) {
            timers[i].period_ms = msg.period_ms;
            xTimerChangePeriod(timers[i].handle, pdMS_TO_TICKS(msg.period_ms), 0);
        }
    }
}

// ================= HEARTBEAT TASK =================
void heartbeat_task(void *param) {
    while(1) {
        if (is_master) {
            for (int i=0;i<MAX_TIMERS;i++) {
                if (timers[i].active) {
                    timer_sync_msg_t msg;
                    strncpy(msg.timer_name, timers[i].name, sizeof(msg.timer_name));
                    msg.period_ms = timers[i].period_ms;
                    msg.node_id = esp_random();
                    esp_now_send(master_mac, (uint8_t*)&msg, sizeof(msg));
                    ESP_LOGI(TAG, "Broadcast timer: %s period %d", msg.timer_name, msg.period_ms);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL));
    }
}

// ================= SCHEDULER TASK =================
void scheduler_task(void *param) {
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        // Load balancing example
        uint32_t total_load = 0;
        for (int i=0;i<MAX_TIMERS;i++) {
            if (timers[i].active) {
                uint32_t avg = 0;
                for (int j=0;j<PERFORMANCE_WINDOW;j++) avg += perf_window[j][i];
                avg /= PERFORMANCE_WINDOW;
                total_load += avg;
            }
        }
        uint32_t load_percent = total_load/(MAX_TIMERS>0?MAX_TIMERS:1);
        ESP_LOGI(TAG, "System load: %d%%", load_percent);
        if (load_percent > ADAPTIVE_THRESHOLD) {
            ESP_LOGW(TAG, "High load! Adjusting timers...");
            for (int i=0;i<MAX_TIMERS;i++) {
                if (timers[i].active) {
                    timers[i].period_ms += 10;
                    xTimerChangePeriod(timers[i].handle, pdMS_TO_TICKS(timers[i].period_ms), 0);
                }
            }
        }
    }
}

// ================= APP MAIN =================
void app_main(void) {
    ESP_LOGI(TAG, "Starting Advanced Distributed Scheduler");

    // Init NVS
    nvs_flash_init();

    // Init networking
    esp_netif_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // Init ESP-NOW
    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);

    // Init LED
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);

    // Init mutex
    timer_mutex = xSemaphoreCreateMutex();
    memset(timers,0,sizeof(timers));
    memset(perf_window,0,sizeof(perf_window));

    // Create timers (priority + deadline)
    create_timer("TimerA", 200, 5, 250);
    create_timer("TimerB", 300, 3, 350);
    create_timer("TimerC", 500, 1, 600);

    // Start tasks
    xTaskCreate(heartbeat_task, "HeartbeatTask", 2048, NULL, 5, NULL);
    xTaskCreate(scheduler_task, "SchedulerTask", 2048, NULL, 6, NULL); // <-- แก้ priority
}
