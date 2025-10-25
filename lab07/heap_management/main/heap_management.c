// -------------------------------
// Advanced Heap Management System
// -------------------------------
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/gpio.h"

static const char *TAG = "HEAP_MGMT";

// -------------------------
// GPIO สำหรับแสดงสถานะ
// -------------------------
#define LED_MEMORY_OK       GPIO_NUM_2
#define LED_LOW_MEMORY      GPIO_NUM_4
#define LED_MEMORY_ERROR    GPIO_NUM_5
#define LED_FRAGMENTATION   GPIO_NUM_18
#define LED_SPIRAM_ACTIVE   GPIO_NUM_19

// Memory thresholds
#define LOW_MEMORY_THRESHOLD      50000
#define CRITICAL_MEMORY_THRESHOLD 20000
#define FRAGMENTATION_THRESHOLD   0.3
#define MAX_ALLOCATIONS           100

// -------------------------
// Memory tracking structs
// -------------------------
typedef struct {
    void* ptr;
    size_t size;
    uint32_t caps;
    const char* description;
    uint64_t timestamp;
    bool is_active;
} memory_allocation_t;

typedef struct {
    uint32_t total_allocations;
    uint32_t total_deallocations;
    uint32_t current_allocations;
    uint64_t total_bytes_allocated;
    uint64_t total_bytes_deallocated;
    uint64_t peak_usage;
    uint32_t allocation_failures;
    uint32_t fragmentation_events;
    uint32_t low_memory_events;
} memory_stats_t;

typedef struct {
    void* ptrs[10];
    size_t sizes[10];
    int count;
    const char* batch_name;
} memory_batch_t;

// -------------------------
// Globals
// -------------------------
static memory_allocation_t allocations[MAX_ALLOCATIONS];
static memory_stats_t stats = {0};
static SemaphoreHandle_t memory_mutex;
static bool memory_monitoring_enabled = true;

// -------------------------
// Allocation helpers
// -------------------------
int find_free_allocation_slot(void) {
    for (int i = 0; i < MAX_ALLOCATIONS; i++)
        if (!allocations[i].is_active) return i;
    return -1;
}

int find_allocation_by_ptr(void* ptr) {
    for (int i = 0; i < MAX_ALLOCATIONS; i++)
        if (allocations[i].is_active && allocations[i].ptr == ptr) return i;
    return -1;
}

void* tracked_malloc(size_t size, uint32_t caps, const char* description) {
    void* ptr = heap_caps_malloc(size, caps);

    if (memory_monitoring_enabled && memory_mutex) {
        if (xSemaphoreTake(memory_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (ptr) {
                int slot = find_free_allocation_slot();
                if (slot >= 0) {
                    allocations[slot] = (memory_allocation_t){
                        .ptr = ptr,
                        .size = size,
                        .caps = caps,
                        .description = description,
                        .timestamp = esp_timer_get_time(),
                        .is_active = true
                    };
                    stats.total_allocations++;
                    stats.current_allocations++;
                    stats.total_bytes_allocated += size;
                    size_t current_usage = stats.total_bytes_allocated - stats.total_bytes_deallocated;
                    if (current_usage > stats.peak_usage) stats.peak_usage = current_usage;

                    ESP_LOGI(TAG, "✅ Allocated %d bytes at %p (%s) - Slot %d", size, ptr, description, slot);
                } else {
                    ESP_LOGW(TAG, "⚠️ Allocation tracking full!");
                }
            } else {
                stats.allocation_failures++;
                ESP_LOGE(TAG, "❌ Failed to allocate %d bytes (%s)", size, description);
            }
            xSemaphoreGive(memory_mutex);
        }
    }
    return ptr;
}

void tracked_free(void* ptr, const char* description) {
    if (!ptr) return;

    if (memory_monitoring_enabled && memory_mutex) {
        if (xSemaphoreTake(memory_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            int slot = find_allocation_by_ptr(ptr);
            if (slot >= 0) {
                allocations[slot].is_active = false;
                stats.total_deallocations++;
                stats.current_allocations--;
                stats.total_bytes_deallocated += allocations[slot].size;
                ESP_LOGI(TAG, "🗑️ Freed %d bytes at %p (%s) - Slot %d",
                         allocations[slot].size, ptr, description, slot);
            } else {
                ESP_LOGW(TAG, "⚠️ Freeing untracked pointer %p (%s)", ptr, description);
            }
            xSemaphoreGive(memory_mutex);
        }
    }
    heap_caps_free(ptr);
}

// -------------------------
// Memory analysis functions
// -------------------------
void analyze_memory_status(void) {
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    float fragmentation = (internal_free > 0) ? 1.0f - ((float)internal_largest / internal_free) : 0.0f;

    ESP_LOGI(TAG, "\n📊 Memory Status:");
    ESP_LOGI(TAG, "Internal Free: %d bytes, Largest Block: %d bytes", internal_free, internal_largest);
    ESP_LOGI(TAG, "SPIRAM Free: %d bytes", spiram_free);
    ESP_LOGI(TAG, "Fragmentation: %.1f%%", fragmentation * 100);

    // LED indicators
    gpio_set_level(LED_MEMORY_OK, internal_free >= LOW_MEMORY_THRESHOLD);
    gpio_set_level(LED_LOW_MEMORY, internal_free < LOW_MEMORY_THRESHOLD && internal_free >= CRITICAL_MEMORY_THRESHOLD);
    gpio_set_level(LED_MEMORY_ERROR, internal_free < CRITICAL_MEMORY_THRESHOLD);
    gpio_set_level(LED_FRAGMENTATION, fragmentation > FRAGMENTATION_THRESHOLD);
    gpio_set_level(LED_SPIRAM_ACTIVE, spiram_free > 0);

    if (internal_free < LOW_MEMORY_THRESHOLD) stats.low_memory_events++;
    if (fragmentation > FRAGMENTATION_THRESHOLD) stats.fragmentation_events++;
}

void print_allocation_summary(void) {
    if (!memory_mutex) return;
    if (xSemaphoreTake(memory_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGI(TAG, "\n📈 Allocation Summary:");
        ESP_LOGI(TAG, "Total Allocations: %u", stats.total_allocations);
        ESP_LOGI(TAG, "Total Deallocations: %u", stats.total_deallocations);
        ESP_LOGI(TAG, "Current Allocations: %u", stats.current_allocations);
        ESP_LOGI(TAG, "Total Bytes Allocated: %llu", stats.total_bytes_allocated);
        ESP_LOGI(TAG, "Total Bytes Deallocated: %llu", stats.total_bytes_deallocated);
        ESP_LOGI(TAG, "Peak Usage: %llu bytes", stats.peak_usage);
        ESP_LOGI(TAG, "Allocation Failures: %u", stats.allocation_failures);
        xSemaphoreGive(memory_mutex);
    }
}

void detect_memory_leaks(void) {
    if (!memory_mutex) return;
    if (xSemaphoreTake(memory_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        int leak_count = 0;
        for (int i = 0; i < MAX_ALLOCATIONS; i++)
            if (allocations[i].is_active) leak_count++;
        if (leak_count > 0) ESP_LOGW(TAG, "⚠️ Memory leaks detected: %d allocations still active", leak_count);
        else ESP_LOGI(TAG, "✅ No memory leaks detected");
        xSemaphoreGive(memory_mutex);
    }
}

// -------------------------
// Allocation patterns
// -------------------------
void analyze_allocation_patterns(void) {
    if (!memory_mutex) return;
    if (xSemaphoreTake(memory_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        size_t internal_usage=0, spiram_usage=0, dma_usage=0;
        int internal_count=0, spiram_count=0, dma_count=0;
        for(int i=0;i<MAX_ALLOCATIONS;i++) {
            if(allocations[i].is_active) {
                if(allocations[i].caps & MALLOC_CAP_INTERNAL) { internal_usage += allocations[i].size; internal_count++; }
                if(allocations[i].caps & MALLOC_CAP_SPIRAM)   { spiram_usage += allocations[i].size; spiram_count++; }
                if(allocations[i].caps & MALLOC_CAP_DMA)     { dma_usage += allocations[i].size; dma_count++; }
            }
        }
        ESP_LOGI(TAG, "\n📊 Allocation Patterns:");
        ESP_LOGI(TAG, "Internal: %d bytes in %d allocations", internal_usage, internal_count);
        ESP_LOGI(TAG, "SPIRAM:   %d bytes in %d allocations", spiram_usage, spiram_count);
        ESP_LOGI(TAG, "DMA:      %d bytes in %d allocations", dma_usage, dma_count);
        xSemaphoreGive(memory_mutex);
    }
}

// -------------------------
// Batch allocation
// -------------------------
memory_batch_t* create_memory_batch(const char* name) {
    memory_batch_t* batch = tracked_malloc(sizeof(memory_batch_t), MALLOC_CAP_INTERNAL, "BatchStruct");
    if (batch) { memset(batch,0,sizeof(memory_batch_t)); batch->batch_name=name; }
    return batch;
}

bool batch_allocate(memory_batch_t* batch, size_t size, uint32_t caps) {
    if(!batch || batch->count>=10) return false;
    batch->ptrs[batch->count] = tracked_malloc(size, caps, batch->batch_name);
    if(batch->ptrs[batch->count]) { batch->sizes[batch->count]=size; batch->count++; return true; }
    return false;
}

void batch_free(memory_batch_t* batch) {
    if(!batch) return;
    for(int i=0;i<batch->count;i++) if(batch->ptrs[i]) { tracked_free(batch->ptrs[i], batch->batch_name); batch->ptrs[i]=NULL; }
    tracked_free(batch,"BatchStruct");
}

// -------------------------
// Memory monitor task
// -------------------------
void memory_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG,"📊 Memory monitor started");
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        analyze_memory_status();
        print_allocation_summary();
        detect_memory_leaks();
        analyze_allocation_patterns();

        if(!heap_caps_check_integrity_all(true)) {
            ESP_LOGE(TAG,"🚨 HEAP CORRUPTION DETECTED!");
            gpio_set_level(LED_MEMORY_ERROR,1);
        }

        ESP_LOGI(TAG,"Free heap: %d bytes",esp_get_free_heap_size());
        ESP_LOGI(TAG,"System uptime: %llu ms\n",esp_timer_get_time()/1000);
    }
}

// -------------------------
// app_main
// -------------------------
void app_main(void) {
    ESP_LOGI(TAG,"🚀 Heap Management Lab Starting...");

    // GPIO
    int leds[]={LED_MEMORY_OK,LED_LOW_MEMORY,LED_MEMORY_ERROR,LED_FRAGMENTATION,LED_SPIRAM_ACTIVE};
    for(int i=0;i<5;i++){ gpio_set_direction(leds[i],GPIO_MODE_OUTPUT); gpio_set_level(leds[i],0); }

    memory_mutex = xSemaphoreCreateMutex();
    if(!memory_mutex){ ESP_LOGE(TAG,"Failed to create memory mutex!"); return; }

    memset(allocations,0,sizeof(allocations));
    analyze_memory_status();

    // Start monitor
    xTaskCreate(memory_monitor_task,"MemMonitor",4096,NULL,6,NULL);

    ESP_LOGI(TAG,"Heap Management System operational!");
}
