#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static const char *TAG = "EVENT_SYNC_ADV";

// GPIO สำหรับ LED
#define LED_BARRIER_SYNC    GPIO_NUM_2
#define LED_PIPELINE_STAGE1 GPIO_NUM_4
#define LED_PIPELINE_STAGE2 GPIO_NUM_5
#define LED_PIPELINE_STAGE3 GPIO_NUM_18
#define LED_WORKFLOW_ACTIVE GPIO_NUM_19

// Event Groups
EventGroupHandle_t barrier_events;
EventGroupHandle_t pipeline_events;
EventGroupHandle_t workflow_events;

// Barrier bits
#define WORKER_A_READY_BIT  (1 << 0)
#define WORKER_B_READY_BIT  (1 << 1)
#define WORKER_C_READY_BIT  (1 << 2)
#define WORKER_D_READY_BIT  (1 << 3)
#define ALL_WORKERS_READY   (WORKER_A_READY_BIT | WORKER_B_READY_BIT | WORKER_C_READY_BIT | WORKER_D_READY_BIT)

// Pipeline bits
#define STAGE1_COMPLETE_BIT (1 << 0)
#define STAGE2_COMPLETE_BIT (1 << 1)
#define STAGE3_COMPLETE_BIT (1 << 2)
#define STAGE4_COMPLETE_BIT (1 << 3)
#define DATA_AVAILABLE_BIT  (1 << 4)
#define PIPELINE_RESET_BIT  (1 << 5)

// Workflow bits
#define WORKFLOW_START_BIT  (1 << 0)
#define APPROVAL_READY_BIT  (1 << 1)
#define RESOURCES_FREE_BIT  (1 << 2)
#define QUALITY_OK_BIT      (1 << 3)
#define WORKFLOW_DONE_BIT   (1 << 4)

// Data structures
typedef struct {
    uint32_t worker_id;
    uint32_t cycle_number;
    uint32_t work_duration;
    uint64_t timestamp;
} worker_data_t;

typedef struct {
    uint32_t pipeline_id;
    uint32_t stage;
    float processing_data[4];
    uint32_t quality_score;
    uint64_t stage_timestamps[4];
} pipeline_data_t;

typedef struct {
    uint32_t workflow_id;
    char description[32];
    uint32_t priority;
    uint32_t estimated_duration;
    bool requires_approval;
} workflow_item_t;

// Queues
QueueHandle_t pipeline_queue;
QueueHandle_t workflow_queue;

// Statistics
typedef struct {
    uint32_t barrier_cycles;
    uint32_t pipeline_completions;
    uint32_t workflow_completions;
    uint32_t synchronization_time_max;
    uint32_t synchronization_time_avg;
    uint64_t total_processing_time;
} sync_stats_t;

static sync_stats_t stats = {0};

// -------------------------
// Synchronization Metrics
// -------------------------
typedef struct {
    uint32_t total_waits;
    uint32_t successful_waits;
    uint32_t timeout_waits;
    uint32_t min_wait_time;
    uint32_t max_wait_time;
    uint32_t avg_wait_time;
} sync_metrics_t;

static sync_metrics_t barrier_metrics = {0};
static sync_metrics_t pipeline_metrics = {0};
static sync_metrics_t workflow_metrics = {0};

void update_sync_metrics(sync_metrics_t* metrics, uint32_t wait_time, bool success) {
    metrics->total_waits++;
    if (success) {
        metrics->successful_waits++;
        if (wait_time < metrics->min_wait_time || metrics->min_wait_time == 0) metrics->min_wait_time = wait_time;
        if (wait_time > metrics->max_wait_time) metrics->max_wait_time = wait_time;
        metrics->avg_wait_time = (metrics->avg_wait_time + wait_time) / 2;
    } else {
        metrics->timeout_waits++;
    }
}

// Advanced Monitoring - Barrier intervals
void analyze_synchronization_patterns(void) {
    static uint32_t last_barrier_time = 0;
    static uint32_t barrier_intervals[10] = {0};
    static int interval_index = 0;
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (last_barrier_time > 0) {
        uint32_t interval = current_time - last_barrier_time;
        barrier_intervals[interval_index] = interval;
        interval_index = (interval_index + 1) % 10;
        
        uint32_t avg_interval = 0;
        for (int i = 0; i < 10; i++) avg_interval += barrier_intervals[i];
        avg_interval /= 10;
        
        ESP_LOGI(TAG, "📊 Barrier interval: %lu ms (avg: %lu ms)", interval, avg_interval);
    }
    last_barrier_time = current_time;
}

// -------------------------
// Barrier Worker Task
// -------------------------
void barrier_worker_task(void *pvParameters) {
    uint32_t worker_id = (uint32_t)pvParameters;
    EventBits_t my_ready_bit = (1 << worker_id);
    uint32_t cycle = 0;
    
    ESP_LOGI(TAG, "🏃 Barrier Worker %lu started", worker_id);
    
    while (1) {
        cycle++;
        uint32_t work_duration = 1000 + (esp_random() % 3000);
        ESP_LOGI(TAG, "👷 Worker %lu: Cycle %lu - Independent work (%lu ms)", worker_id, cycle, work_duration);
        vTaskDelay(pdMS_TO_TICKS(work_duration));
        
        uint64_t barrier_start = esp_timer_get_time();
        ESP_LOGI(TAG, "🚧 Worker %lu: Ready for barrier (cycle %lu)", worker_id, cycle);
        xEventGroupSetBits(barrier_events, my_ready_bit);
        
        EventBits_t bits = xEventGroupWaitBits(barrier_events, ALL_WORKERS_READY, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
        uint64_t barrier_end = esp_timer_get_time();
        uint32_t barrier_time = (barrier_end - barrier_start) / 1000;
        
        if ((bits & ALL_WORKERS_READY) == ALL_WORKERS_READY) {
            ESP_LOGI(TAG, "🎯 Worker %lu: Barrier passed! (waited %lu ms)", worker_id, barrier_time);
            update_sync_metrics(&barrier_metrics, barrier_time, true);
            analyze_synchronization_patterns();
            
            if (barrier_time > stats.synchronization_time_max) stats.synchronization_time_max = barrier_time;
            stats.synchronization_time_avg = (stats.synchronization_time_avg + barrier_time)/2;
            
            if (worker_id == 0) {
                stats.barrier_cycles++;
                gpio_set_level(LED_BARRIER_SYNC, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(LED_BARRIER_SYNC, 0);
            }
            
            vTaskDelay(pdMS_TO_TICKS(500 + (esp_random() % 500)));
        } else {
            ESP_LOGW(TAG, "⏰ Worker %lu: Barrier timeout!", worker_id);
            update_sync_metrics(&barrier_metrics, 10000, false);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// -------------------------
// Pipeline Stage Task
// -------------------------
void pipeline_stage_task(void *pvParameters) {
    uint32_t stage_id = (uint32_t)pvParameters;
    EventBits_t stage_complete_bit = (1 << stage_id);
    EventBits_t prev_stage_bit = (stage_id > 0) ? (1 << (stage_id - 1)) : DATA_AVAILABLE_BIT;
    const char* stage_names[] = {"Input", "Processing", "Filtering", "Output"};
    gpio_num_t stage_leds[] = {LED_PIPELINE_STAGE1, LED_PIPELINE_STAGE2, LED_PIPELINE_STAGE3, LED_WORKFLOW_ACTIVE};
    
    ESP_LOGI(TAG, "🏭 Pipeline Stage %lu (%s) started", stage_id, stage_names[stage_id]);
    
    while (1) {
        ESP_LOGI(TAG, "⏳ Stage %lu: Waiting for input...", stage_id);
        uint64_t stage_start = esp_timer_get_time();
        
        EventBits_t bits = xEventGroupWaitBits(pipeline_events, prev_stage_bit, pdTRUE, pdTRUE, portMAX_DELAY);
        
        if (bits & prev_stage_bit) {
            gpio_set_level(stage_leds[stage_id], 1);
            pipeline_data_t pipeline_data;
            
            if (xQueueReceive(pipeline_queue, &pipeline_data, pdMS_TO_TICKS(100)) == pdTRUE) {
                pipeline_data.stage_timestamps[stage_id] = esp_timer_get_time();
                pipeline_data.stage = stage_id;
                
                uint32_t processing_time = 500 + (esp_random() % 1000);
                
                // Stage-specific logic
                switch(stage_id) {
                    case 0: for(int i=0;i<4;i++) pipeline_data.processing_data[i]=(esp_random()%1000)/10.0; pipeline_data.quality_score=70+(esp_random()%30); break;
                    case 1: for(int i=0;i<4;i++) pipeline_data.processing_data[i]*=1.1; pipeline_data.quality_score+=(esp_random()%20)-10; break;
                    case 2: { float avg=0; for(int i=0;i<4;i++) avg+=pipeline_data.processing_data[i]; avg/=4.0; ESP_LOGI(TAG,"Avg %.2f Quality %lu",avg,pipeline_data.quality_score); } break;
                    case 3: stats.pipeline_completions++; stats.total_processing_time += (esp_timer_get_time() - pipeline_data.stage_timestamps[0]); ESP_LOGI(TAG,"✅ Pipeline %lu done in %llu ms",pipeline_data.pipeline_id,(pipeline_data.stage_timestamps[3]-pipeline_data.stage_timestamps[0])/1000); break;
                }
                vTaskDelay(pdMS_TO_TICKS(processing_time));
                
                if(stage_id<3){
                    if(xQueueSend(pipeline_queue,&pipeline_data,pdMS_TO_TICKS(100))==pdTRUE)
                        xEventGroupSetBits(pipeline_events,stage_complete_bit);
                    else
                        ESP_LOGW(TAG,"⚠️ Stage %lu: Queue full, data lost",stage_id);
                }
                
                uint32_t wait_time = (esp_timer_get_time() - stage_start)/1000;
                update_sync_metrics(&pipeline_metrics, wait_time, true);
            } else {
                ESP_LOGW(TAG,"⚠️ Stage %lu: No data in queue",stage_id);
            }
            gpio_set_level(stage_leds[stage_id], 0);
        }
        
        if(xEventGroupGetBits(pipeline_events) & PIPELINE_RESET_BIT){
            ESP_LOGI(TAG,"🔄 Stage %lu: Pipeline reset",stage_id);
            xEventGroupClearBits(pipeline_events,PIPELINE_RESET_BIT);
            pipeline_data_t dummy;
            while(xQueueReceive(pipeline_queue,&dummy,0)==pdTRUE);
        }
    }
}

// -------------------------
// Pipeline Generator Task
// -------------------------
void pipeline_data_generator_task(void *pvParameters) {
    uint32_t pipeline_id=0;
    ESP_LOGI(TAG,"🏭 Pipeline data generator started");
    
    while(1){
        pipeline_data_t data={0};
        data.pipeline_id=++pipeline_id;
        data.stage=0;
        data.stage_timestamps[0]=esp_timer_get_time();
        ESP_LOGI(TAG,"🚀 Generating pipeline data ID: %lu",pipeline_id);
        
        if(xQueueSend(pipeline_queue,&data,pdMS_TO_TICKS(1000))==pdTRUE)
            xEventGroupSetBits(pipeline_events,DATA_AVAILABLE_BIT);
        else
            ESP_LOGW(TAG,"⚠️ Pipeline queue full, data %lu dropped",pipeline_id);
        
        vTaskDelay(pdMS_TO_TICKS(3000+(esp_random()%4000)));
    }
}

// -------------------------
// Workflow Manager Task
// -------------------------
void workflow_manager_task(void *pvParameters) {
    ESP_LOGI(TAG,"📋 Workflow manager started");
    
    while(1){
        workflow_item_t workflow;
        if(xQueueReceive(workflow_queue,&workflow,portMAX_DELAY)==pdTRUE){
            ESP_LOGI(TAG,"📝 Workflow ID %lu - %s",workflow.workflow_id,workflow.description);
            xEventGroupSetBits(workflow_events,WORKFLOW_START_BIT);
            gpio_set_level(LED_WORKFLOW_ACTIVE,1);
            
            EventBits_t required_events=RESOURCES_FREE_BIT;
            if(workflow.requires_approval) required_events|=APPROVAL_READY_BIT;
            
            ESP_LOGI(TAG,"⏳ Waiting for workflow requirements (0x%08X)...",required_events);
            uint64_t start_wait=esp_timer_get_time();
            EventBits_t bits=xEventGroupWaitBits(workflow_events,required_events,pdFALSE,pdTRUE,pdMS_TO_TICKS(workflow.estimated_duration*2));
            uint32_t wait_time=(esp_timer_get_time()-start_wait)/1000;
            
            if((bits & required_events)==required_events){
                update_sync_metrics(&workflow_metrics,wait_time,true);
                uint32_t execution_time=workflow.estimated_duration+(esp_random()%1000);
                vTaskDelay(pdMS_TO_TICKS(execution_time));
                
                uint32_t quality=60+(esp_random()%40);
                if(quality>80){
                    xEventGroupSetBits(workflow_events,QUALITY_OK_BIT|WORKFLOW_DONE_BIT);
                    stats.workflow_completions++;
                    ESP_LOGI(TAG,"✅ Workflow %lu done (Quality: %lu%%)",workflow.workflow_id,quality);
                } else {
                    ESP_LOGW(TAG,"⚠️ Workflow %lu quality failed (%lu%%), retrying",workflow.workflow_id,quality);
                    xQueueSend(workflow_queue,&workflow,0);
                }
            } else update_sync_metrics(&workflow_metrics,wait_time,false);
            
            gpio_set_level(LED_WORKFLOW_ACTIVE,0);
            xEventGroupClearBits(workflow_events,WORKFLOW_START_BIT|WORKFLOW_DONE_BIT|QUALITY_OK_BIT);
        }
    }
}

// -------------------------
// Approval Task
// -------------------------
void approval_task(void *pvParameters){
    ESP_LOGI(TAG,"👨‍💼 Approval task started");
    while(1){
        xEventGroupWaitBits(workflow_events,WORKFLOW_START_BIT,pdFALSE,pdTRUE,portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(1000+(esp_random()%2000)));
        bool approved=(esp_random()%100)>20;
        if(approved){ xEventGroupSetBits(workflow_events,APPROVAL_READY_BIT); ESP_LOGI(TAG,"✅ Approval granted"); }
        else{ xEventGroupClearBits(workflow_events,APPROVAL_READY_BIT); ESP_LOGW(TAG,"❌ Approval denied"); }
        vTaskDelay(pdMS_TO_TICKS(5000));
        xEventGroupClearBits(workflow_events,APPROVAL_READY_BIT);
    }
}

// -------------------------
// Resource Manager Task
// -------------------------
void resource_manager_task(void *pvParameters){
    ESP_LOGI(TAG,"🏗️ Resource manager started");
    bool resources_available=true;
    while(1){
        if(resources_available){
            xEventGroupSetBits(workflow_events,RESOURCES_FREE_BIT);
            uint32_t usage_time=2000+(esp_random()%8000);
            vTaskDelay(pdMS_TO_TICKS(usage_time));
            if((esp_random()%100)>70) resources_available=false, xEventGroupClearBits(workflow_events,RESOURCES_FREE_BIT);
        } else {
            vTaskDelay(pdMS_TO_TICKS(3000+(esp_random()%5000)));
            resources_available=true;
        }
    }
}

// -------------------------
// Workflow Generator Task
// -------------------------
void workflow_generator_task(void *pvParameters){
    uint32_t workflow_counter=0;
    const char* workflow_types[]={"Data Processing","Report Generation","System Backup","Quality Analysis","Performance Test","Security Scan"};
    
    while(1){
        workflow_item_t workflow={0};
        workflow.workflow_id=++workflow_counter;
        workflow.priority=1+(esp_random()%5);
        workflow.estimated_duration=2000+(esp_random()%4000);
        workflow.requires_approval=(esp_random()%100)>60;
        strcpy(workflow.description,workflow_types[esp_random()%6]);
        xQueueSend(workflow_queue,&workflow,pdMS_TO_TICKS(1000));
        vTaskDelay(pdMS_TO_TICKS(4000+(esp_random()%6000)));
    }
}

// -------------------------
// Statistics Monitor Task
// -------------------------
void statistics_monitor_task(void *pvParameters){
    while(1){
        vTaskDelay(pdMS_TO_TICKS(15000));
        ESP_LOGI(TAG,"\n📈 ═══ SYNCHRONIZATION STATISTICS ═══");
        ESP_LOGI(TAG,"Barrier cycles:        %lu",stats.barrier_cycles);
        ESP_LOGI(TAG,"Pipeline completions:  %lu",stats.pipeline_completions);
        ESP_LOGI(TAG,"Workflow completions:  %lu",stats.workflow_completions);
        ESP_LOGI(TAG,"Max sync time:         %lu ms",stats.synchronization_time_max);
        ESP_LOGI(TAG,"Avg sync time:         %lu ms",stats.synchronization_time_avg);
        if(stats.pipeline_completions>0) ESP_LOGI(TAG,"Avg pipeline time:     %lu ms",(stats.total_processing_time/1000)/stats.pipeline_completions);
        ESP_LOGI(TAG,"📊 Barrier Metrics - total: %lu, success: %lu, timeout: %lu, min: %lu ms, max: %lu ms, avg: %lu ms",
                 barrier_metrics.total_waits,barrier_metrics.successful_waits,barrier_metrics.timeout_waits,
                 barrier_metrics.min_wait_time,barrier_metrics.max_wait_time,barrier_metrics.avg_wait_time);
        ESP_LOGI(TAG,"📊 Pipeline Metrics - total: %lu, success: %lu, timeout: %lu, min: %lu ms, max: %lu ms, avg: %lu ms",
                 pipeline_metrics.total_waits,pipeline_metrics.successful_waits,pipeline_metrics.timeout_waits,
                 pipeline_metrics.min_wait_time,pipeline_metrics.max_wait_time,pipeline_metrics.avg_wait_time);
        ESP_LOGI(TAG,"📊 Workflow Metrics - total: %lu, success: %lu, timeout: %lu, min: %lu ms, max: %lu ms, avg: %lu ms",
                 workflow_metrics.total_waits,workflow_metrics.successful_waits,workflow_metrics.timeout_waits,
                 workflow_metrics.min_wait_time,workflow_metrics.max_wait_time,workflow_metrics.avg_wait_time);
        ESP_LOGI(TAG,"Free heap:             %d bytes",esp_get_free_heap_size());
    }
}

// -------------------------
// Main Application Entry
// -------------------------
void app_main(void){
    gpio_reset_pin(LED_BARRIER_SYNC);
    gpio_reset_pin(LED_PIPELINE_STAGE1);
    gpio_reset_pin(LED_PIPELINE_STAGE2);
    gpio_reset_pin(LED_PIPELINE_STAGE3);
    gpio_reset_pin(LED_WORKFLOW_ACTIVE);
    gpio_set_direction(LED_BARRIER_SYNC,GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIPELINE_STAGE1,GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIPELINE_STAGE2,GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIPELINE_STAGE3,GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_WORKFLOW_ACTIVE,GPIO_MODE_OUTPUT);
    
    barrier_events = xEventGroupCreate();
    pipeline_events = xEventGroupCreate();
    workflow_events = xEventGroupCreate();
    
    pipeline_queue = xQueueCreate(10,sizeof(pipeline_data_t));
    workflow_queue = xQueueCreate(10,sizeof(workflow_item_t));
    
    for(uint32_t i=0;i<4;i++) xTaskCreate(barrier_worker_task,"BarrierWorker",4096,(void*)i,5,NULL);
    for(uint32_t i=0;i<4;i++) xTaskCreate(pipeline_stage_task,"PipelineStage",4096,(void*)i,4,NULL);
    
    xTaskCreate(pipeline_data_generator_task,"PipelineGen",4096,NULL,3,NULL);
    xTaskCreate(workflow_generator_task,"WorkflowGen",4096,NULL,3,NULL);
    xTaskCreate(workflow_manager_task,"WorkflowMgr",4096,NULL,4,NULL);
    xTaskCreate(approval_task,"Approval",4096,NULL,4,NULL);
    xTaskCreate(resource_manager_task,"ResourceMgr",4096,NULL,4,NULL);
    xTaskCreate(statistics_monitor_task,"StatsMonitor",4096,NULL,2,NULL);
}
