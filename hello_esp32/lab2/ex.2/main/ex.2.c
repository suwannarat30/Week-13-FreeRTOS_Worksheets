#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "PERF";

void performance_demo(void)
{
    ESP_LOGI(TAG, "=== Performance Monitoring ===");
    
    // Measure execution time
    uint64_t start_time = esp_timer_get_time();
    
    // Simulate some work
    for (int i = 0; i < 1000000; i++) {
        volatile int dummy = i * 2;
    }
    
    uint64_t end_time = esp_timer_get_time();
    uint64_t execution_time = end_time - start_time;
    
    ESP_LOGI(TAG, "Execution time: %lld microseconds", execution_time);
    ESP_LOGI(TAG, "Execution time: %.2f milliseconds", execution_time / 1000.0);
}

// ตัวอย่างการเรียกใช้
void app_main(void)
{
    performance_demo();
}
