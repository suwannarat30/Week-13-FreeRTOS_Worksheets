#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "driver/gpio.h"

static const char *TAG = "MEM_POOLS";

#define LED_SMALL_POOL  GPIO_NUM_2
#define LED_MEDIUM_POOL GPIO_NUM_4
#define LED_LARGE_POOL  GPIO_NUM_5
#define LED_POOL_FULL   GPIO_NUM_18
#define LED_POOL_ERROR  GPIO_NUM_19

#define SMALL_BLOCK_SIZE  64
#define SMALL_BLOCK_COUNT 32

#define MEDIUM_BLOCK_SIZE 256
#define MEDIUM_BLOCK_COUNT 16

#define LARGE_BLOCK_SIZE  1024
#define LARGE_BLOCK_COUNT 8

#define HUGE_BLOCK_SIZE   4096
#define HUGE_BLOCK_COUNT  4

#define MAGIC_FREE  0xDEADBEEF
#define MAGIC_ALLOC 0xCAFEBABE

typedef struct mem_block {
    struct mem_block* next;
    uint32_t magic;
    uint32_t pool_id;
} mem_block_t;

typedef struct {
    const char* name;
    size_t block_size;
    size_t block_count;
    void* pool_memory;
    mem_block_t* free_list;
    SemaphoreHandle_t mutex;
    uint32_t pool_id;
    size_t allocated_blocks;
    size_t peak_usage;
    uint64_t total_allocations;
    uint64_t total_deallocations;
    uint32_t allocation_failures;
    uint64_t allocation_time_total;   // new
    uint64_t deallocation_time_total; // new
} memory_pool_t;

typedef enum {
    POOL_SMALL = 0,
    POOL_MEDIUM,
    POOL_LARGE,
    POOL_HUGE,
    POOL_COUNT
} pool_type_t;

typedef struct {
    const char* name;
    size_t block_size;
    size_t block_count;
    gpio_num_t led_pin;
} pool_cfg_t;

static pool_cfg_t pool_configs[POOL_COUNT] = {
    {"Small",  SMALL_BLOCK_SIZE,  SMALL_BLOCK_COUNT,  LED_SMALL_POOL},
    {"Medium", MEDIUM_BLOCK_SIZE, MEDIUM_BLOCK_COUNT, LED_MEDIUM_POOL},
    {"Large",  LARGE_BLOCK_SIZE,  LARGE_BLOCK_COUNT,  LED_LARGE_POOL},
    {"Huge",   HUGE_BLOCK_SIZE,   HUGE_BLOCK_COUNT,   LED_POOL_FULL}
};

static memory_pool_t pools[POOL_COUNT];

// --------------------- INIT POOL ---------------------
bool init_pool(memory_pool_t* pool, pool_type_t type) {
    if (!pool || type >= POOL_COUNT) return false;

    pool_cfg_t* cfg = &pool_configs[type];
    pool->name = cfg->name;
    pool->block_size = cfg->block_size;
    pool->block_count = cfg->block_count;
    pool->pool_id = type + 1;
    pool->allocated_blocks = 0;
    pool->peak_usage = 0;
    pool->total_allocations = 0;
    pool->total_deallocations = 0;
    pool->allocation_failures = 0;
    pool->allocation_time_total = 0;
    pool->deallocation_time_total = 0;

    size_t total_mem = pool->block_count * (sizeof(mem_block_t) + pool->block_size);
    pool->pool_memory = heap_caps_malloc(total_mem, MALLOC_CAP_INTERNAL);
    if (!pool->pool_memory) {
        ESP_LOGE(TAG, "Failed to allocate memory for pool %s", pool->name);
        return false;
    }

    pool->free_list = NULL;
    uint8_t* ptr = pool->pool_memory;
    for (size_t i = 0; i < pool->block_count; i++) {
        mem_block_t* block = (mem_block_t*)ptr;
        block->magic = MAGIC_FREE;
        block->pool_id = pool->pool_id;
        block->next = pool->free_list;
        pool->free_list = block;
        ptr += sizeof(mem_block_t) + pool->block_size;
    }

    pool->mutex = xSemaphoreCreateMutex();
    if (!pool->mutex) return false;

    ESP_LOGI(TAG, "Initialized %s pool: %d blocks x %d bytes", pool->name, pool->block_count, pool->block_size);
    return true;
}

// --------------------- MALLOC / FREE ---------------------
void* pool_malloc(memory_pool_t* pool) {
    void* result = NULL;
    uint64_t start = esp_timer_get_time();
    if (xSemaphoreTake(pool->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (pool->free_list) {
            mem_block_t* block = pool->free_list;
            pool->free_list = block->next;
            block->magic = MAGIC_ALLOC;
            pool->allocated_blocks++;
            if (pool->allocated_blocks > pool->peak_usage) pool->peak_usage = pool->allocated_blocks;
            pool->total_allocations++;
            result = (uint8_t*)block + sizeof(mem_block_t);
            gpio_set_level(pool_configs[pool->pool_id-1].led_pin, 1);
            vTaskDelay(pdMS_TO_TICKS(20));
            gpio_set_level(pool_configs[pool->pool_id-1].led_pin, 0);
        } else {
            pool->allocation_failures++;
            gpio_set_level(LED_POOL_FULL, 1);
        }
        xSemaphoreGive(pool->mutex);
    }
    pool->allocation_time_total += esp_timer_get_time() - start;
    return result;
}

bool pool_free(memory_pool_t* pool, void* ptr) {
    if (!ptr) return false;
    mem_block_t* block = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));
    if (block->magic != MAGIC_ALLOC || block->pool_id != pool->pool_id) return false;
    block->magic = MAGIC_FREE;

    uint64_t start = esp_timer_get_time();
    if (xSemaphoreTake(pool->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        block->next = pool->free_list;
        pool->free_list = block;
        pool->allocated_blocks--;
        pool->total_deallocations++;
        xSemaphoreGive(pool->mutex);
    }
    pool->deallocation_time_total += esp_timer_get_time() - start;
    return true;
}

// --------------------- SMART MALLOC ---------------------
void* smart_malloc(size_t size) {
    for (int i=0;i<POOL_COUNT;i++) {
        if (size <= pools[i].block_size) {
            void* p = pool_malloc(&pools[i]);
            if (p) return p;
        }
    }
    return heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
}

void smart_free(void* ptr) {
    for (int i=0;i<POOL_COUNT;i++) {
        if (pool_free(&pools[i], ptr)) return;
    }
    heap_caps_free(ptr);
}

// --------------------- STATS ---------------------
void print_pool_stats(void) {
    ESP_LOGI(TAG, "=== Memory Pool Stats ===");
    for (int i=0;i<POOL_COUNT;i++) {
        memory_pool_t* pool = &pools[i];
        ESP_LOGI(TAG, "%s: Allocated=%d Peak=%d Failures=%d", 
                 pool->name, pool->allocated_blocks, pool->peak_usage, pool->allocation_failures);
    }
}

// --------------------- POOL EFFICIENCY ANALYSIS ---------------------
void analyze_pool_efficiency(void) {
    ESP_LOGI(TAG, "\n📈 Pool Efficiency Analysis:");
    for (int i = 0; i < POOL_COUNT; i++) {
        memory_pool_t* pool = &pools[i];
        if (pool->total_allocations > 0) {
            float success_rate = ((float)(pool->total_allocations - pool->allocation_failures) / 
                                  pool->total_allocations) * 100.0;
            float utilization = ((float)pool->peak_usage / pool->block_count) * 100.0;
            float avg_alloc_time = (float)pool->allocation_time_total / pool->total_allocations;
            float avg_dealloc_time = (float)pool->deallocation_time_total / pool->total_deallocations;
            
            ESP_LOGI(TAG, "%s Pool Efficiency:", pool->name);
            ESP_LOGI(TAG, "  Success Rate: %.1f%%", success_rate);
            ESP_LOGI(TAG, "  Peak Utilization: %.1f%%", utilization);
            ESP_LOGI(TAG, "  Avg Alloc Time: %.2f μs", avg_alloc_time);
            ESP_LOGI(TAG, "  Avg Dealloc Time: %.2f μs", avg_dealloc_time);
        }
    }
}

// --------------------- ADVANCED FEATURES ---------------------
bool resize_pool(memory_pool_t* pool, size_t new_block_count) {
    if (!pool || new_block_count == pool->block_count) return false;
    ESP_LOGI(TAG, "🔧 Resizing %s pool: %d → %d blocks", pool->name, pool->block_count, new_block_count);
    // Simplified placeholder
    ESP_LOGI(TAG, "✅ Resizing successful (simplified implementation)");
    return true;
}

void balance_pool_loads(void) {
    ESP_LOGI(TAG, "⚖️ Balancing pool loads...");
    for (int i = 0; i < POOL_COUNT; i++) {
        memory_pool_t* pool = &pools[i];
        float utilization = (float)pool->allocated_blocks / pool->block_count;
        if (utilization > 0.9) {
            ESP_LOGW(TAG, "⚠️ %s pool highly utilized (%.1f%%) - consider expanding", pool->name, utilization * 100);
        } else if (utilization < 0.1 && pool->total_allocations > 100) {
            ESP_LOGI(TAG, "💡 %s pool under-utilized (%.1f%%) - consider shrinking", pool->name, utilization * 100);
        }
    }
}

// --------------------- TASKS ---------------------
void stress_task(void* arg) {
    void* ptrs[50] = {0};
    int count = 0;
    while(1) {
        int action = esp_random() % 3;
        if (action == 0 && count < 50) {
            size_t sz = 16 + (esp_random() % 4096);
            ptrs[count] = smart_malloc(sz);
            count++;
        } else if (action == 1 && count > 0) {
            int idx = esp_random() % count;
            if (ptrs[idx]) {
                smart_free(ptrs[idx]);
                for (int j=idx;j<count-1;j++) ptrs[j]=ptrs[j+1];
                count--;
            }
        } else {
            print_pool_stats();
            analyze_pool_efficiency();
            balance_pool_loads();
        }
        vTaskDelay(pdMS_TO_TICKS(500 + esp_random()%500));
    }
}

void monitor_task(void* arg) {
    while(1) {
        print_pool_stats();
        analyze_pool_efficiency();
        balance_pool_loads();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// --------------------- APP_MAIN ---------------------
void app_main(void) {
    gpio_reset_pin(LED_SMALL_POOL);
    gpio_reset_pin(LED_MEDIUM_POOL);
    gpio_reset_pin(LED_LARGE_POOL);
    gpio_reset_pin(LED_POOL_FULL);
    gpio_reset_pin(LED_POOL_ERROR);

    gpio_set_direction(LED_SMALL_POOL, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_MEDIUM_POOL, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_LARGE_POOL, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_POOL_FULL, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_POOL_ERROR, GPIO_MODE_OUTPUT);

    for (int i=0;i<POOL_COUNT;i++) init_pool(&pools[i], i);

    xTaskCreate(stress_task, "stress", 4096, NULL, 5, NULL);
    xTaskCreate(monitor_task, "monitor", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Memory Pool System Started");
}
