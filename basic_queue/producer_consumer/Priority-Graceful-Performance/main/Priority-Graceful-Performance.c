#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_random.h"

static const char *TAG = "PROD_CONS";

// LED pins
#define LED_PRODUCER_1 GPIO_NUM_2
#define LED_PRODUCER_2 GPIO_NUM_4
#define LED_PRODUCER_3 GPIO_NUM_5
#define LED_PRODUCER_4 GPIO_NUM_15
#define LED_CONSUMER_1 GPIO_NUM_18

// Queue and mutex
QueueHandle_t xProductQueue;
SemaphoreHandle_t xPrintMutex;

// Shutdown flag
bool system_shutdown = false;

// Statistics
typedef struct {
    uint32_t produced;
    uint32_t consumed;
    uint32_t dropped;
} stats_t;

stats_t global_stats = {0,0,0};

// Performance monitoring
typedef struct {
    uint32_t avg_processing_time;
    uint32_t max_queue_size;
    uint32_t throughput_per_minute;
} performance_t;

performance_t system_perf = {0,0,0};

// Product with priority
typedef struct {
    int producer_id;
    int product_id;
    char product_name[30];
    uint32_t production_time;
    int processing_time_ms;
    int priority; // 1=low .. 5=high
} product_t;

// Safe printf
void safe_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    if (xSemaphoreTake(xPrintMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        vprintf(format, args);
        xSemaphoreGive(xPrintMutex);
    }
    va_end(args);
}

// Producer task
void producer_task(void *pvParameters) {
    int producer_id = *((int*)pvParameters);
    product_t product;
    int product_counter = 0;
    gpio_num_t led_pin;

    switch(producer_id){
        case 1: led_pin = LED_PRODUCER_1; break;
        case 2: led_pin = LED_PRODUCER_2; break;
        case 3: led_pin = LED_PRODUCER_3; break;
        case 4: led_pin = LED_PRODUCER_4; break;
        default: led_pin = LED_PRODUCER_1;
    }

    safe_printf("Producer %d started\n", producer_id);

    while(!system_shutdown) {
        product.producer_id = producer_id;
        product.product_id = product_counter++;
        snprintf(product.product_name, sizeof(product.product_name), "Product-P%d-#%d", producer_id, product.product_id);
        product.production_time = xTaskGetTickCount();
        product.processing_time_ms = 500 + (esp_random() % 2000);
        product.priority = 1 + (esp_random() % 5);

        BaseType_t xStatus = xQueueSend(xProductQueue, &product, pdMS_TO_TICKS(100));
        if (xStatus == pdPASS) {
            global_stats.produced++;
            safe_printf("✓ Producer %d: Created %s (priority %d, processing %dms)\n",
                producer_id, product.product_name, product.priority, product.processing_time_ms);
            gpio_set_level(led_pin, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(led_pin, 0);
        } else {
            global_stats.dropped++;
            safe_printf("✗ Producer %d: Queue full! Dropped %s\n", producer_id, product.product_name);
        }

        vTaskDelay(pdMS_TO_TICKS(1000 + (esp_random()%2000)));
    }

    safe_printf("Producer %d: Shutdown gracefully\n", producer_id);
    vTaskDelete(NULL);
}

// Consumer task
void consumer_task(void *pvParameters) {
    int consumer_id = *((int*)pvParameters);
    product_t product;
    gpio_num_t led_pin;
    uint32_t total_processing_time = 0;
    uint32_t processed_count = 0;

    led_pin = LED_CONSUMER_1;
    safe_printf("Consumer %d started\n", consumer_id);

    while(!system_shutdown) {
        if (xQueueReceive(xProductQueue, &product, pdMS_TO_TICKS(5000)) == pdPASS) {
            uint32_t start = xTaskGetTickCount();
            gpio_set_level(led_pin, 1);
            vTaskDelay(pdMS_TO_TICKS(product.processing_time_ms));
            gpio_set_level(led_pin, 0);
            uint32_t end = xTaskGetTickCount();
            uint32_t elapsed = (end - start) * portTICK_PERIOD_MS;

            total_processing_time += elapsed;
            processed_count++;
            system_perf.avg_processing_time = total_processing_time / processed_count;

            // Update max queue size
            UBaseType_t qsize = uxQueueMessagesWaiting(xProductQueue);
            if (qsize > system_perf.max_queue_size) system_perf.max_queue_size = qsize;

            global_stats.consumed++;
            safe_printf("→ Consumer %d: Finished %s (queue time: %lums)\n", consumer_id, product.product_name, (xTaskGetTickCount() - product.production_time)*portTICK_PERIOD_MS);
        }
    }

    safe_printf("Consumer %d: Shutdown gracefully\n", consumer_id);
    vTaskDelete(NULL);
}

// Statistics & Performance task
void statistics_task(void *pvParameters) {
    while(!system_shutdown) {
        UBaseType_t queue_items = uxQueueMessagesWaiting(xProductQueue);

        safe_printf("\n═══ SYSTEM STATISTICS ═══\n");
        safe_printf("Products Produced: %lu\n", global_stats.produced);
        safe_printf("Products Consumed: %lu\n", global_stats.consumed);
        safe_printf("Products Dropped:  %lu\n", global_stats.dropped);
        safe_printf("Queue Backlog:     %d\n", queue_items);
        safe_printf("System Efficiency: %.1f%%\n", global_stats.produced > 0 ? (float)global_stats.consumed/global_stats.produced*100:0);
        safe_printf("Avg Processing Time: %lums\n", system_perf.avg_processing_time);
        safe_printf("Max Queue Size: %lu\n", system_perf.max_queue_size);

        printf("Queue: [");
        for(int i=0;i<10;i++){
            if(i<queue_items) printf("■"); else printf("□");
        }
        printf("]\n");
        safe_printf("═══════════════════════════\n\n");

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    vTaskDelete(NULL);
}

// Load balancer
void load_balancer_task(void *pvParameters) {
    const int MAX_QUEUE_SIZE = 8;
    while(!system_shutdown) {
        UBaseType_t queue_items = uxQueueMessagesWaiting(xProductQueue);
        if(queue_items > MAX_QUEUE_SIZE){
            safe_printf("⚠️ HIGH LOAD DETECTED! Queue size: %d\n", queue_items);
            safe_printf("💡 Suggestion: Add more consumers or optimize processing\n");

            gpio_set_level(LED_PRODUCER_1,1); gpio_set_level(LED_PRODUCER_2,1);
            gpio_set_level(LED_PRODUCER_3,1); gpio_set_level(LED_PRODUCER_4,1);
            gpio_set_level(LED_CONSUMER_1,1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_PRODUCER_1,0); gpio_set_level(LED_PRODUCER_2,0);
            gpio_set_level(LED_PRODUCER_3,0); gpio_set_level(LED_PRODUCER_4,0);
            gpio_set_level(LED_CONSUMER_1,0);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

// app_main
void app_main(void){
    ESP_LOGI(TAG, "Producer-Consumer System Starting...");

    gpio_set_direction(LED_PRODUCER_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PRODUCER_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PRODUCER_3, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PRODUCER_4, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_CONSUMER_1, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_PRODUCER_1,0); gpio_set_level(LED_PRODUCER_2,0);
    gpio_set_level(LED_PRODUCER_3,0); gpio_set_level(LED_PRODUCER_4,0);
    gpio_set_level(LED_CONSUMER_1,0);

    xProductQueue = xQueueCreate(10, sizeof(product_t));
    xPrintMutex = xSemaphoreCreateMutex();

    if(xProductQueue && xPrintMutex){
        static int producer1_id=1, producer2_id=2, producer3_id=3, producer4_id=4;
        static int consumer1_id=1;

        xTaskCreate(producer_task,"Producer1",3072,&producer1_id,3,NULL);
        xTaskCreate(producer_task,"Producer2",3072,&producer2_id,3,NULL);
        xTaskCreate(producer_task,"Producer3",3072,&producer3_id,3,NULL);
        xTaskCreate(producer_task,"Producer4",3072,&producer4_id,3,NULL);

        xTaskCreate(consumer_task,"Consumer1",3072,&consumer1_id,2,NULL);

        xTaskCreate(statistics_task,"Statistics",3072,NULL,1,NULL);
        xTaskCreate(load_balancer_task,"LoadBalancer",2048,NULL,1,NULL);

        ESP_LOGI(TAG,"All tasks created. System operational.");
    } else {
        ESP_LOGE(TAG,"Failed to create queue or mutex!");
    }
}

// Optional: trigger shutdown after some time
void trigger_shutdown(void){
    system_shutdown = true;
}
