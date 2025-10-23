#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "PRACTICAL_SYSTEM";

// -------------------------
// Data Structures
// -------------------------
typedef enum {
    SENSOR_TEMPERATURE,
    SENSOR_HUMIDITY,
    SENSOR_PRESSURE,
    SENSOR_LIGHT
} sensor_type_t;

typedef struct {
    sensor_type_t type;
    float value;
    uint32_t timestamp;
    int sensor_id;
} sensor_reading_t;

typedef struct {
    float temperature_sum;
    float humidity_sum;
    float pressure_sum;
    float light_sum;
    int temperature_count;
    int humidity_count;
    int pressure_count;
    int light_count;
} sensor_statistics_t;

typedef enum {
    CMD_LED_ON,
    CMD_LED_OFF,
    CMD_LED_BLINK,
    CMD_LED_PATTERN,
    CMD_SYSTEM_RESET,
    CMD_GET_STATUS
} command_type_t;

typedef struct {
    command_type_t type;
    int parameter1;
    int parameter2;
    char string_param[32];
} command_t;

// -------------------------
// Queue Handles
// -------------------------
QueueHandle_t sensor_data_queue;
QueueHandle_t statistics_queue;
QueueHandle_t command_queue;

// -------------------------
// Sensor Tasks
// -------------------------
void temperature_sensor_task(void *parameter)
{
    sensor_reading_t reading;
    while (1) {
        reading.type = SENSOR_TEMPERATURE;
        reading.sensor_id = 1;
        reading.value = 20.0 + (rand() % 300) / 10.0;
        reading.timestamp = xTaskGetTickCount();
        xQueueSend(sensor_data_queue, &reading, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void humidity_sensor_task(void *parameter)
{
    sensor_reading_t reading;
    while (1) {
        reading.type = SENSOR_HUMIDITY;
        reading.sensor_id = 2;
        reading.value = 30.0 + (rand() % 700) / 10.0;
        reading.timestamp = xTaskGetTickCount();
        xQueueSend(sensor_data_queue, &reading, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

// -------------------------
// Data Processor Task
// -------------------------
void data_processor_task(void *parameter)
{
    sensor_reading_t reading;
    static sensor_statistics_t stats = {0};

    while (1) {
        if (xQueueReceive(sensor_data_queue, &reading, portMAX_DELAY) == pdTRUE) {
            switch (reading.type) {
                case SENSOR_TEMPERATURE:
                    stats.temperature_sum += reading.value;
                    stats.temperature_count++;
                    ESP_LOGI(TAG, "Temp: %.1f°C (Avg: %.1f)", reading.value,
                             stats.temperature_sum / stats.temperature_count);
                    break;
                case SENSOR_HUMIDITY:
                    stats.humidity_sum += reading.value;
                    stats.humidity_count++;
                    ESP_LOGI(TAG, "Humidity: %.1f%% (Avg: %.1f)", reading.value,
                             stats.humidity_sum / stats.humidity_count);
                    break;
                case SENSOR_PRESSURE:
                    stats.pressure_sum += reading.value;
                    stats.pressure_count++;
                    break;
                case SENSOR_LIGHT:
                    stats.light_sum += reading.value;
                    stats.light_count++;
                    break;
            }

            if ((stats.temperature_count + stats.humidity_count +
                 stats.pressure_count + stats.light_count) % 10 == 0) {
                xQueueSend(statistics_queue, &stats, 0);
            }
        }
    }
}

// -------------------------
// Statistics Task
// -------------------------
void statistics_task(void *parameter)
{
    sensor_statistics_t stats;
    while (1) {
        if (xQueueReceive(statistics_queue, &stats, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "\n=== SENSOR STATISTICS ===");
            if (stats.temperature_count > 0)
                ESP_LOGI(TAG, "Temperature Avg: %.1f°C (%d samples)",
                         stats.temperature_sum / stats.temperature_count, stats.temperature_count);
            if (stats.humidity_count > 0)
                ESP_LOGI(TAG, "Humidity Avg: %.1f%% (%d samples)",
                         stats.humidity_sum / stats.humidity_count, stats.humidity_count);
            ESP_LOGI(TAG, "========================\n");
        }
    }
}

// -------------------------
// Command Tasks
// -------------------------
void command_sender_task(void *parameter)
{
    command_t cmd;
    int counter = 0;

    while (1) {
        switch (counter % 4) {
            case 0:
                cmd.type = CMD_LED_ON;
                cmd.parameter1 = 2;
                break;
            case 1:
                cmd.type = CMD_LED_BLINK;
                cmd.parameter1 = 2;
                cmd.parameter2 = 500;
                break;
            case 2:
                cmd.type = CMD_LED_PATTERN;
                cmd.parameter1 = 3;
                strcpy(cmd.string_param, "SOS");
                break;
            case 3:
                cmd.type = CMD_GET_STATUS;
                break;
        }

        if (xQueueSend(command_queue, &cmd, pdMS_TO_TICKS(500)) != pdTRUE) {
            ESP_LOGW(TAG, "Command queue full");
        } else {
            ESP_LOGI(TAG, "Command sent: %d", cmd.type);
        }

        counter++;
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void command_processor_task(void *parameter)
{
    command_t cmd;
    while (1) {
        if (xQueueReceive(command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Processing command: %d", cmd.type);
            switch (cmd.type) {
                case CMD_LED_ON:
                    ESP_LOGI(TAG, "LED ON GPIO %d", cmd.parameter1);
                    break;
                case CMD_LED_OFF:
                    ESP_LOGI(TAG, "LED OFF GPIO %d", cmd.parameter1);
                    break;
                case CMD_LED_BLINK:
                    ESP_LOGI(TAG, "LED BLINK GPIO %d Interval %dms", cmd.parameter1, cmd.parameter2);
                    break;
                case CMD_LED_PATTERN:
                    ESP_LOGI(TAG, "LED PATTERN %d : %s", cmd.parameter1, cmd.string_param);
                    break;
                case CMD_SYSTEM_RESET:
                    ESP_LOGI(TAG, "System RESET requested");
                    break;
                case CMD_GET_STATUS:
                    ESP_LOGI(TAG, "System running normally");
                    break;
            }
        }
    }
}

// -------------------------
// Queue Monitoring Task
// -------------------------
void queue_monitor_task(void *parameter)
{
    while (1) {
        UBaseType_t items, spaces;

        items = uxQueueMessagesWaiting(sensor_data_queue);
        spaces = uxQueueSpacesAvailable(sensor_data_queue);
        if (spaces == 0)
            ESP_LOGW(TAG, "Sensor data queue FULL!");
        if (items > 0.8 * (items + spaces))
            ESP_LOGW(TAG, "Sensor data queue almost full: %d/%d", items, items + spaces);

        items = uxQueueMessagesWaiting(command_queue);
        spaces = uxQueueSpacesAvailable(command_queue);
        if (spaces == 0)
            ESP_LOGW(TAG, "Command queue FULL!");
        if (items > 0.8 * (items + spaces))
            ESP_LOGW(TAG, "Command queue almost full: %d/%d", items, items + spaces);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// -------------------------
// Main
// -------------------------
void app_main(void)
{
    // Create queues
    sensor_data_queue = xQueueCreate(20, sizeof(sensor_reading_t));
    statistics_queue = xQueueCreate(5, sizeof(sensor_statistics_t));
    command_queue = xQueueCreate(10, sizeof(command_t));

    // Create tasks
    xTaskCreate(temperature_sensor_task, "TempSensor", 2048, NULL, 5, NULL);
    xTaskCreate(humidity_sensor_task, "HumSensor", 2048, NULL, 5, NULL);
    xTaskCreate(data_processor_task, "DataProcessor", 3072, NULL, 6, NULL);
    xTaskCreate(statistics_task, "Statistics", 2048, NULL, 4, NULL);
    xTaskCreate(command_sender_task, "CmdSender", 2048, NULL, 3, NULL);
    xTaskCreate(command_processor_task, "CmdProcessor", 2048, NULL, 4, NULL);
    xTaskCreate(queue_monitor_task, "QueueMonitor", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Practical RTOS system started");
}
