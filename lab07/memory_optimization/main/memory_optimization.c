#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "esp_random.h"

static const char *TAG = "MEM_OPT";

// GPIO LEDs
#define LED_STATIC_ALLOC    GPIO_NUM_2
#define LED_ALIGNMENT_OPT   GPIO_NUM_4
#define LED_PACKING_OPT     GPIO_NUM_5
#define LED_MEMORY_SAVING   GPIO_NUM_18
#define LED_OPTIMIZATION    GPIO_NUM_19

// Alignment utilities
#define ALIGN_UP(num, align) (((num) + (align) - 1) & ~((align) - 1))
#define IS_ALIGNED(ptr, align) (((uintptr_t)(ptr) & ((align) - 1)) == 0)

// Static memory pools
#define STATIC_BUFFER_SIZE   4096
#define STATIC_BUFFER_COUNT  8
#define TASK_STACK_SIZE      2048
#define MAX_TASKS            4

static uint8_t static_buffers[STATIC_BUFFER_COUNT][STATIC_BUFFER_SIZE] __attribute__((aligned(4)));
static bool static_buffer_used[STATIC_BUFFER_COUNT] = {false};
static SemaphoreHandle_t static_buffer_mutex;

static StackType_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(8)));
static StaticTask_t task_buffers[MAX_TASKS];
static int next_task_slot = 0;

typedef struct {
    size_t static_allocations;
    size_t dynamic_allocations;
    size_t alignment_optimizations;
    size_t packing_optimizations;
    size_t memory_saved_bytes;
    size_t fragmentation_reduced;
    uint64_t allocation_time_saved;
} optimization_stats_t;

static optimization_stats_t opt_stats = {0};

// ---------------- Struct examples ----------------
typedef struct {
    char a;
    int b;
    char c;
    double d;
    char e;
} __attribute__((packed)) bad_struct_t;

typedef struct {
    double d;
    int b;
    char a;
    char c;
    char e;
} __attribute__((aligned(8))) good_struct_t;

// ---------------- Static buffer management ----------------
void* allocate_static_buffer(void) {
    void* buffer = NULL;
    if (static_buffer_mutex && xSemaphoreTake(static_buffer_mutex, pdMS_TO_TICKS(100))) {
        for (int i = 0; i < STATIC_BUFFER_COUNT; i++) {
            if (!static_buffer_used[i]) {
                static_buffer_used[i] = true;
                buffer = static_buffers[i];
                opt_stats.static_allocations++;
                gpio_set_level(LED_STATIC_ALLOC, 1);
                break;
            }
        }
        xSemaphoreGive(static_buffer_mutex);
    }
    return buffer;
}

void free_static_buffer(void* buffer) {
    if (!buffer || !static_buffer_mutex) return;
    if (xSemaphoreTake(static_buffer_mutex, pdMS_TO_TICKS(100))) {
        for (int i = 0; i < STATIC_BUFFER_COUNT; i++) {
            if (buffer == static_buffers[i] && static_buffer_used[i]) {
                static_buffer_used[i] = false;
                break;
            }
        }
        bool any_used = false;
        for (int i = 0; i < STATIC_BUFFER_COUNT; i++)
            if (static_buffer_used[i]) any_used = true;

        if (!any_used) gpio_set_level(LED_STATIC_ALLOC, 0);
        xSemaphoreGive(static_buffer_mutex);
    }
}

// ---------------- Aligned malloc/free ----------------
void* aligned_malloc(size_t size, size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return NULL;
    size_t total_size = size + alignment + sizeof(void*);
    void* raw_ptr = malloc(total_size);
    if (!raw_ptr) return NULL;
    uintptr_t aligned_addr = ALIGN_UP((uintptr_t)raw_ptr + sizeof(void*), alignment);
    void** orig_ptr_storage = (void**)aligned_addr - 1;
    *orig_ptr_storage = raw_ptr;
    opt_stats.alignment_optimizations++;
    opt_stats.dynamic_allocations++;
    return (void*)aligned_addr;
}

void aligned_free(void* aligned_ptr) {
    if (!aligned_ptr) return;
    void** orig_ptr_storage = (void**)aligned_ptr - 1;
    free(*orig_ptr_storage);
}

// ---------------- Struct optimization demo ----------------
void demonstrate_struct_optimization(void) {
    bad_struct_t bad_example;
    good_struct_t good_example;
    (void)bad_example;
    (void)good_example;

    ESP_LOGI(TAG, "Bad struct: %d bytes, Good struct: %d bytes, Saved: %d bytes",
             (int)sizeof(bad_struct_t), (int)sizeof(good_struct_t),
             (int)(sizeof(bad_struct_t) - sizeof(good_struct_t)));

    const int array_size = 1000;
    opt_stats.memory_saved_bytes += (sizeof(bad_struct_t) - sizeof(good_struct_t)) * array_size;
    opt_stats.packing_optimizations++;
}

// ---------------- Memory access optimization ----------------
void optimize_memory_access_patterns(void) {
    const size_t array_size = 1024;
    const int iterations = 1000;
    uint32_t* test_array = aligned_malloc(array_size * sizeof(uint32_t), 32);
    if (!test_array) return;
    for (size_t i = 0; i < array_size; i++) test_array[i] = i;

    volatile uint32_t sum = 0;
    uint64_t start = esp_timer_get_time();
    for (int iter = 0; iter < iterations; iter++)
        for (size_t i = 0; i < array_size; i++) sum += test_array[i];
    uint64_t seq_time = esp_timer_get_time() - start;

    sum = 0;
    start = esp_timer_get_time();
    for (int iter = 0; iter < iterations; iter++)
        for (size_t i = 0; i < array_size; i++)
            sum += test_array[esp_random() % array_size];
    uint64_t rand_time = esp_timer_get_time() - start;

    ESP_LOGI(TAG, "Access patterns: Sequential %llu μs, Random %llu μs, Speedup %.2fx",
             seq_time, rand_time, (float)rand_time / seq_time);

    aligned_free(test_array);
}

// ---------------- Task creation ----------------
BaseType_t create_static_task(TaskFunction_t func, const char* name, UBaseType_t prio, void* params) {
    if (next_task_slot >= MAX_TASKS) return pdFAIL;
    TaskHandle_t h = xTaskCreateStatic(func, name, TASK_STACK_SIZE, params, prio,
                                       task_stacks[next_task_slot], &task_buffers[next_task_slot]);
    if (h) next_task_slot++;
    return h ? pdPASS : pdFAIL;
}

// ---------------- Tasks ----------------
void optimization_test_task(void* pv) {
    (void)pv;
    while (1) {
        gpio_set_level(LED_OPTIMIZATION, 1);
        demonstrate_struct_optimization();
        optimize_memory_access_patterns();
        gpio_set_level(LED_OPTIMIZATION, 0);
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

void memory_usage_test_task(void* pv) {
    (void)pv;
    while (1) {
        void* bufs[4] = {NULL};
        for (int i = 0; i < 4; i++) bufs[i] = allocate_static_buffer();
        vTaskDelay(pdMS_TO_TICKS(3000));
        for (int i = 0; i < 4; i++) if (bufs[i]) free_static_buffer(bufs[i]);

        void* aligned_ptrs[3];
        aligned_ptrs[0] = aligned_malloc(1024, 16);
        aligned_ptrs[1] = aligned_malloc(2048, 32);
        aligned_ptrs[2] = aligned_malloc(4096, 64);
        vTaskDelay(pdMS_TO_TICKS(2000));
        for (int i = 0; i < 3; i++) aligned_free(aligned_ptrs[i]);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void optimization_monitor_task(void* pv) {
    (void)pv;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));
        ESP_LOGI(TAG, "=== Optimization Stats ===");
        ESP_LOGI(TAG, "Static allocations: %d", (int)opt_stats.static_allocations);
        ESP_LOGI(TAG, "Dynamic allocations: %d", (int)opt_stats.dynamic_allocations);
        ESP_LOGI(TAG, "Alignment opt: %d", (int)opt_stats.alignment_optimizations);
        ESP_LOGI(TAG, "Packing opt: %d", (int)opt_stats.packing_optimizations);
        ESP_LOGI(TAG, "Memory saved: %d bytes", (int)opt_stats.memory_saved_bytes);

        gpio_set_level(LED_MEMORY_SAVING, opt_stats.memory_saved_bytes > 1024 ? 1 : 0);
    }
}

// ---------------- Main ----------------
void app_main(void) {
    gpio_set_direction(LED_STATIC_ALLOC, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_ALIGNMENT_OPT, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PACKING_OPT, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_MEMORY_SAVING, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_OPTIMIZATION, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_STATIC_ALLOC, 0);
    gpio_set_level(LED_ALIGNMENT_OPT, 0);
    gpio_set_level(LED_PACKING_OPT, 0);
    gpio_set_level(LED_MEMORY_SAVING, 0);
    gpio_set_level(LED_OPTIMIZATION, 0);

    static_buffer_mutex = xSemaphoreCreateMutex();

    create_static_task(optimization_test_task, "OptTest", 5, NULL);
    create_static_task(memory_usage_test_task, "MemUsage", 4, NULL);
    xTaskCreate(optimization_monitor_task, "OptMonitor", 3072, NULL, 6, NULL);

    ESP_LOGI(TAG, "Memory Optimization Lab started!");
}
