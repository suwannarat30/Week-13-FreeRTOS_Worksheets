#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/gpio.h"

static const char *TAG = "COMPLEX_EVENTS";

// GPIO สำหรับ Smart Home System
#define LED_LIVING_ROOM    GPIO_NUM_2
#define LED_KITCHEN        GPIO_NUM_4
#define LED_BEDROOM        GPIO_NUM_5
#define LED_SECURITY       GPIO_NUM_18
#define LED_EMERGENCY      GPIO_NUM_19
#define MOTION_SENSOR      GPIO_NUM_21
#define DOOR_SENSOR        GPIO_NUM_22

// Smart Home State Machine States
typedef enum {
    HOME_STATE_IDLE = 0,
    HOME_STATE_OCCUPIED,
    HOME_STATE_AWAY,
    HOME_STATE_SLEEP,
    HOME_STATE_SECURITY_ARMED,
    HOME_STATE_EMERGENCY,
    HOME_STATE_MAINTENANCE
} home_state_t;

// Event Groups และ Event Bits
EventGroupHandle_t sensor_events;
EventGroupHandle_t system_events;
EventGroupHandle_t pattern_events;

// Sensor Events
#define MOTION_DETECTED_BIT     (1 << 0)
#define DOOR_OPENED_BIT         (1 << 1)
#define DOOR_CLOSED_BIT         (1 << 2)
#define LIGHT_ON_BIT            (1 << 3)
#define LIGHT_OFF_BIT           (1 << 4)
#define TEMPERATURE_HIGH_BIT    (1 << 5)
#define TEMPERATURE_LOW_BIT     (1 << 6)
#define SOUND_DETECTED_BIT      (1 << 7)
#define PRESENCE_CONFIRMED_BIT  (1 << 8)

// System Events
#define SYSTEM_INIT_BIT         (1 << 0)
#define USER_HOME_BIT           (1 << 1)
#define USER_AWAY_BIT           (1 << 2)
#define SLEEP_MODE_BIT          (1 << 3)
#define SECURITY_ARMED_BIT      (1 << 4)
#define EMERGENCY_MODE_BIT      (1 << 5)
#define MAINTENANCE_MODE_BIT    (1 << 6)

// Pattern Events
#define PATTERN_NORMAL_ENTRY_BIT    (1 << 0)
#define PATTERN_BREAK_IN_BIT        (1 << 1)
#define PATTERN_EMERGENCY_BIT       (1 << 2)
#define PATTERN_GOODNIGHT_BIT       (1 << 3)
#define PATTERN_WAKE_UP_BIT         (1 << 4)
#define PATTERN_LEAVING_BIT         (1 << 5)
#define PATTERN_RETURNING_BIT       (1 << 6)

// Event และ State Management
static home_state_t current_home_state = HOME_STATE_IDLE;
static SemaphoreHandle_t state_mutex;

// Event History สำหรับ Pattern Recognition
#define EVENT_HISTORY_SIZE 20
typedef struct {
    EventBits_t event_bits;
    uint64_t timestamp;
    home_state_t state_at_time;
} event_record_t;

static event_record_t event_history[EVENT_HISTORY_SIZE];
static int history_index = 0;

// Pattern Recognition Data
typedef struct {
    const char* name;
    EventBits_t required_events[4];  // Up to 4 events in sequence
    uint32_t time_window_ms;         // Max time between events
    EventBits_t result_event;        // Event to set when pattern matches
    void (*action_callback)(void);   // Optional callback function
} event_pattern_t;

// Adaptive System Parameters
typedef struct {
    float motion_sensitivity;
    uint32_t auto_light_timeout;
    uint32_t security_delay;
    bool learning_mode;
    uint32_t pattern_confidence[10];
} adaptive_params_t;

static adaptive_params_t adaptive_params = {
    .motion_sensitivity = 0.7,
    .auto_light_timeout = 300000,  // 5 minutes
    .security_delay = 30000,       // 30 seconds
    .learning_mode = true,
    .pattern_confidence = {0}
};

// Smart Devices Control
typedef struct {
    bool living_room_light;
    bool kitchen_light;
    bool bedroom_light;
    bool security_system;
    bool emergency_mode;
    uint32_t temperature_celsius;
    uint32_t light_level_percent;
} smart_home_status_t;

static smart_home_status_t home_status = {0};

// Advanced Event Analytics
typedef struct {
    uint32_t total_patterns_detected;
    uint32_t false_positives;
    uint32_t pattern_accuracy[10];
    float correlation_strength[10];
    uint32_t adaptive_adjustments;
} pattern_analytics_t;

static pattern_analytics_t analytics = {0};

// Forward declaration
void analyze_pattern_performance(void);
void print_event_sequence(void);

// ------------------ Pattern Actions -------------------
void normal_entry_action(void) {
    ESP_LOGI(TAG, "🏠 Normal entry pattern detected - Welcome home!");
    home_status.living_room_light = true;
    gpio_set_level(LED_LIVING_ROOM, 1);
    xEventGroupSetBits(system_events, USER_HOME_BIT);
}

void break_in_action(void) {
    ESP_LOGW(TAG, "🚨 Break-in pattern detected - Security alert!");
    home_status.security_system = true;
    home_status.emergency_mode = true;
    gpio_set_level(LED_SECURITY, 1);
    gpio_set_level(LED_EMERGENCY, 1);
    xEventGroupSetBits(system_events, EMERGENCY_MODE_BIT);
}

void goodnight_action(void) {
    ESP_LOGI(TAG, "🌙 Goodnight pattern detected - Sleep mode activated");
    home_status.living_room_light = false;
    home_status.kitchen_light = false;
    gpio_set_level(LED_LIVING_ROOM, 0);
    gpio_set_level(LED_KITCHEN, 0);
    gpio_set_level(LED_BEDROOM, 1);
    xEventGroupSetBits(system_events, SLEEP_MODE_BIT);
}

void wake_up_action(void) {
    ESP_LOGI(TAG, "☀️ Wake-up pattern detected - Good morning!");
    home_status.bedroom_light = true;
    home_status.kitchen_light = true;
    gpio_set_level(LED_BEDROOM, 1);
    gpio_set_level(LED_KITCHEN, 1);
    xEventGroupClearBits(system_events, SLEEP_MODE_BIT);
}

void leaving_action(void) {
    ESP_LOGI(TAG, "🚪 Leaving pattern detected - Securing home");
    home_status.living_room_light = false;
    home_status.kitchen_light = false;
    home_status.bedroom_light = false;
    home_status.security_system = true;
    
    gpio_set_level(LED_LIVING_ROOM, 0);
    gpio_set_level(LED_KITCHEN, 0);
    gpio_set_level(LED_BEDROOM, 0);
    gpio_set_level(LED_SECURITY, 1);
    
    xEventGroupSetBits(system_events, USER_AWAY_BIT | SECURITY_ARMED_BIT);
}

void returning_action(void) {
    ESP_LOGI(TAG, "🔓 Returning pattern detected - Disabling security");
    home_status.security_system = false;
    gpio_set_level(LED_SECURITY, 0);
    xEventGroupClearBits(system_events, USER_AWAY_BIT | SECURITY_ARMED_BIT);
}

// ------------------ Event Patterns -------------------
static event_pattern_t event_patterns[] = {
    {"Normal Entry", {DOOR_OPENED_BIT, MOTION_DETECTED_BIT, DOOR_CLOSED_BIT, 0}, 10000, PATTERN_NORMAL_ENTRY_BIT, normal_entry_action},
    {"Break-in Attempt", {DOOR_OPENED_BIT, MOTION_DETECTED_BIT, 0, 0}, 5000, PATTERN_BREAK_IN_BIT, break_in_action},
    {"Goodnight Routine", {LIGHT_OFF_BIT, MOTION_DETECTED_BIT, LIGHT_OFF_BIT, 0}, 30000, PATTERN_GOODNIGHT_BIT, goodnight_action},
    {"Wake-up Routine", {MOTION_DETECTED_BIT, LIGHT_ON_BIT, 0, 0}, 5000, PATTERN_WAKE_UP_BIT, wake_up_action},
    {"Leaving Home", {LIGHT_OFF_BIT, DOOR_OPENED_BIT, DOOR_CLOSED_BIT, 0}, 15000, PATTERN_LEAVING_BIT, leaving_action},
    {"Returning Home", {DOOR_OPENED_BIT, MOTION_DETECTED_BIT, DOOR_CLOSED_BIT, 0}, 8000, PATTERN_RETURNING_BIT, returning_action}
};
#define NUM_PATTERNS (sizeof(event_patterns)/sizeof(event_pattern_t))

// ------------------ State Machine -------------------
const char* get_state_name(home_state_t state) {
    switch (state) {
        case HOME_STATE_IDLE: return "Idle";
        case HOME_STATE_OCCUPIED: return "Occupied";
        case HOME_STATE_AWAY: return "Away";
        case HOME_STATE_SLEEP: return "Sleep";
        case HOME_STATE_SECURITY_ARMED: return "Security Armed";
        case HOME_STATE_EMERGENCY: return "Emergency";
        case HOME_STATE_MAINTENANCE: return "Maintenance";
        default: return "Unknown";
    }
}

void change_home_state(home_state_t new_state) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        home_state_t old_state = current_home_state;
        current_home_state = new_state;
        ESP_LOGI(TAG, "🏠 State changed: %s → %s", get_state_name(old_state), get_state_name(new_state));
        xSemaphoreGive(state_mutex);
    }
}

// ------------------ Event History -------------------
void add_event_to_history(EventBits_t event_bits) {
    event_history[history_index].event_bits = event_bits;
    event_history[history_index].timestamp = esp_timer_get_time();
    event_history[history_index].state_at_time = current_home_state;
    history_index = (history_index + 1) % EVENT_HISTORY_SIZE;
}

// ------------------ Pattern Recognition -------------------
void pattern_recognition_task(void *pvParameters) {
    ESP_LOGI(TAG, "🧠 Pattern recognition engine started");
    while (1) {
        EventBits_t sensor_bits = xEventGroupWaitBits(sensor_events, 0xFFFFFF, pdFALSE, pdFALSE, portMAX_DELAY);
        if (sensor_bits != 0) {
            ESP_LOGI(TAG, "🔍 Sensor event detected: 0x%08X", sensor_bits);
            add_event_to_history(sensor_bits);

            for (int p = 0; p < NUM_PATTERNS; p++) {
                event_pattern_t* pattern = &event_patterns[p];
                bool state_applicable = true;
                
                if (strcmp(pattern->name, "Break-in Attempt") == 0) state_applicable = (current_home_state == HOME_STATE_SECURITY_ARMED);
                if (strcmp(pattern->name, "Wake-up Routine") == 0) state_applicable = (current_home_state == HOME_STATE_SLEEP);
                if (strcmp(pattern->name, "Returning Home") == 0) state_applicable = (current_home_state == HOME_STATE_AWAY);
                if (!state_applicable) continue;

                bool pattern_matched = true;
                uint64_t current_time = esp_timer_get_time();
                int event_index = 0;

                for (int h = 0; h < EVENT_HISTORY_SIZE && pattern->required_events[event_index] != 0; h++) {
                    int hist_idx = (history_index - 1 - h + EVENT_HISTORY_SIZE) % EVENT_HISTORY_SIZE;
                    event_record_t* record = &event_history[hist_idx];
                    if ((current_time - record->timestamp) > (pattern->time_window_ms * 1000)) break;
                    if (record->event_bits & pattern->required_events[event_index]) {
                        event_index++;
                        if (pattern->required_events[event_index] == 0) break;
                    }
                }

                if (pattern->required_events[event_index] == 0) {
                    ESP_LOGI(TAG, "🎯 Pattern matched: %s", pattern->name);
                    xEventGroupSetBits(pattern_events, pattern->result_event);
                    if (pattern->action_callback) pattern->action_callback();

                    // Update analytics
                    analytics.total_patterns_detected++;
                    if (p < 10) {
                        adaptive_params.pattern_confidence[p]++;
                        analytics.pattern_accuracy[p]++;
                    }

                    xEventGroupClearBits(sensor_events, 0xFFFFFF);
                    break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ------------------ Sensor Simulation Tasks -------------------
void motion_sensor_task(void *pvParameters) {
    while (1) {
        if ((esp_random() % 100) < 15) {
            xEventGroupSetBits(sensor_events, MOTION_DETECTED_BIT);
            vTaskDelay(pdMS_TO_TICKS(1000 + (esp_random() % 2000)));
            if ((esp_random() % 100) < 60) xEventGroupSetBits(sensor_events, PRESENCE_CONFIRMED_BIT);
        }
        vTaskDelay(pdMS_TO_TICKS(3000 + (esp_random() % 5000)));
    }
}

void door_sensor_task(void *pvParameters) {
    bool door_open = false;
    while (1) {
        if ((esp_random() % 100) < 8) {
            if (!door_open) {
                xEventGroupSetBits(sensor_events, DOOR_OPENED_BIT);
                door_open = true;
                vTaskDelay(pdMS_TO_TICKS(2000 + (esp_random() % 8000)));
                if ((esp_random() % 100) < 85) { xEventGroupSetBits(sensor_events, DOOR_CLOSED_BIT); door_open = false; }
            } else {
                xEventGroupSetBits(sensor_events, DOOR_CLOSED_BIT);
                door_open = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000 + (esp_random() % 10000)));
    }
}

void light_control_task(void *pvParameters) {
    while (1) {
        if ((esp_random() % 100) < 12) {
            bool light_action = (esp_random() % 2);
            int light_choice = esp_random() % 3;
            if (light_action) {
                xEventGroupSetBits(sensor_events, LIGHT_ON_BIT);
                switch (light_choice) {
                    case 0: home_status.living_room_light = true; gpio_set_level(LED_LIVING_ROOM, 1); break;
                    case 1: home_status.kitchen_light = true; gpio_set_level(LED_KITCHEN, 1); break;
                    case 2: home_status.bedroom_light = true; gpio_set_level(LED_BEDROOM, 1); break;
                }
            } else {
                xEventGroupSetBits(sensor_events, LIGHT_OFF_BIT);
                switch (light_choice) {
                    case 0: home_status.living_room_light = false; gpio_set_level(LED_LIVING_ROOM, 0); break;
                    case 1: home_status.kitchen_light = false; gpio_set_level(LED_KITCHEN, 0); break;
                    case 2: home_status.bedroom_light = false; gpio_set_level(LED_BEDROOM, 0); break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(4000 + (esp_random() % 8000)));
    }
}

void environmental_sensor_task(void *pvParameters) {
    while (1) {
        home_status.temperature_celsius = 20 + (esp_random() % 15);
        if (home_status.temperature_celsius > 28) xEventGroupSetBits(sensor_events, TEMPERATURE_HIGH_BIT);
        else if (home_status.temperature_celsius < 22) xEventGroupSetBits(sensor_events, TEMPERATURE_LOW_BIT);
        if ((esp_random() % 100) < 5) xEventGroupSetBits(sensor_events, SOUND_DETECTED_BIT);
        home_status.light_level_percent = esp_random() % 100;
        vTaskDelay(pdMS_TO_TICKS(8000 + (esp_random() % 7000)));
    }
}

// ------------------ State Machine Task -------------------
void state_machine_task(void *pvParameters) {
    while (1) {
        EventBits_t system_bits = xEventGroupWaitBits(system_events, 0xFFFFFF, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
        if (system_bits != 0) {
            if (system_bits & USER_HOME_BIT) if (current_home_state == HOME_STATE_AWAY || current_home_state == HOME_STATE_IDLE) change_home_state(HOME_STATE_OCCUPIED);
            if (system_bits & USER_AWAY_BIT) change_home_state(HOME_STATE_AWAY);
            if (system_bits & SLEEP_MODE_BIT) if (current_home_state == HOME_STATE_OCCUPIED) change_home_state(HOME_STATE_SLEEP);
            if (system_bits & SECURITY_ARMED_BIT) if (current_home_state == HOME_STATE_AWAY) change_home_state(HOME_STATE_SECURITY_ARMED);
            if (system_bits & EMERGENCY_MODE_BIT) change_home_state(HOME_STATE_EMERGENCY);
            if (system_bits & MAINTENANCE_MODE_BIT) change_home_state(HOME_STATE_MAINTENANCE);
        }

        switch (current_home_state) {
            case HOME_STATE_EMERGENCY:
                vTaskDelay(pdMS_TO_TICKS(10000));
                home_status.emergency_mode = false;
                gpio_set_level(LED_EMERGENCY, 0);
                change_home_state(HOME_STATE_OCCUPIED);
                break;
            case HOME_STATE_IDLE:
                EventBits_t sensor_activity = xEventGroupGetBits(sensor_events);
                if (sensor_activity & (MOTION_DETECTED_BIT | PRESENCE_CONFIRMED_BIT)) change_home_state(HOME_STATE_OCCUPIED);
                break;
            default: break;
        }
    }
}

// ------------------ Adaptive Learning Task -------------------
void adaptive_learning_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (adaptive_params.learning_mode) {
            uint32_t recent_motion_events = 0;
            uint64_t current_time = esp_timer_get_time();
            for (int h = 0; h < EVENT_HISTORY_SIZE; h++) {
                int idx = (history_index - 1 - h + EVENT_HISTORY_SIZE) % EVENT_HISTORY_SIZE;
                if ((current_time - event_history[idx].timestamp) < 300000 * 1000) {
                    if (event_history[idx].event_bits & MOTION_DETECTED_BIT) recent_motion_events++;
                }
            }
            if (recent_motion_events > 5) {
                adaptive_params.auto_light_timeout = 600000;
                analytics.adaptive_adjustments++;
            } else adaptive_params.auto_light_timeout = 300000;
        }
    }
}

// ------------------ Analytics & Event Sequence -------------------
void print_event_sequence(void) {
    ESP_LOGI(TAG, "\n📜 Last Event Sequence:");
    for (int h = 0; h < EVENT_HISTORY_SIZE; h++) {
        int idx = (history_index - 1 - h + EVENT_HISTORY_SIZE) % EVENT_HISTORY_SIZE;
        event_record_t* record = &event_history[idx];
        if (record->event_bits != 0) {
            uint64_t age_ms = (esp_timer_get_time() - record->timestamp) / 1000;
            ESP_LOGI(TAG, "  [-%3lu ms] State: %s, Events: 0x%08X", age_ms, get_state_name(record->state_at_time), record->event_bits);
        }
    }
}

void analyze_pattern_performance(void) {
    ESP_LOGI(TAG, "\n📈 Pattern Analytics:");
    ESP_LOGI(TAG, "  Total patterns detected: %u", analytics.total_patterns_detected);
    for (int i = 0; i < NUM_PATTERNS; i++) {
        ESP_LOGI(TAG, "  %s: Confidence=%u, Accuracy=%u", event_patterns[i].name, adaptive_params.pattern_confidence[i], analytics.pattern_accuracy[i]);
    }
    ESP_LOGI(TAG, "  Adaptive adjustments: %u", analytics.adaptive_adjustments);
}

// ------------------ Main Application -------------------
void app_main(void) {
    gpio_reset_pin(LED_LIVING_ROOM);
    gpio_set_direction(LED_LIVING_ROOM, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_KITCHEN);
    gpio_set_direction(LED_KITCHEN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_BEDROOM);
    gpio_set_direction(LED_BEDROOM, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_SECURITY);
    gpio_set_direction(LED_SECURITY, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_EMERGENCY);
    gpio_set_direction(LED_EMERGENCY, GPIO_MODE_OUTPUT);

    sensor_events = xEventGroupCreate();
    system_events = xEventGroupCreate();
    pattern_events = xEventGroupCreate();
    state_mutex = xSemaphoreCreateMutex();

    xTaskCreate(pattern_recognition_task, "pattern_task", 4096, NULL, 8, NULL);
    xTaskCreate(state_machine_task, "state_task", 4096, NULL, 7, NULL);
    xTaskCreate(motion_sensor_task, "motion_task", 2048, NULL, 5, NULL);
    xTaskCreate(door_sensor_task, "door_task", 2048, NULL, 5, NULL);
    xTaskCreate(light_control_task, "light_task", 2048, NULL, 5, NULL);
    xTaskCreate(environmental_sensor_task, "env_task", 2048, NULL, 5, NULL);
    xTaskCreate(adaptive_learning_task, "adaptive_task", 2048, NULL, 4, NULL);

    ESP_LOGI(TAG, "✅ Smart Home System initialized");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        print_event_sequence();
        analyze_pattern_performance();
    }
}
