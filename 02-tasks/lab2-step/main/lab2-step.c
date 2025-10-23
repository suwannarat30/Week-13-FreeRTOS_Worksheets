#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_RUNNING GPIO_NUM_2
#define LED_READY GPIO_NUM_4
#define LED_BLOCKED GPIO_NUM_5
#define LED_SUSPENDED GPIO_NUM_18

#define BUTTON1_PIN GPIO_NUM_0
#define BUTTON2_PIN GPIO_NUM_35

static const char *TAG = "TASK_DEMO";

// =================== Global Handles =================== //
TaskHandle_t state_demo_task_handle = NULL;
TaskHandle_t control_task_handle = NULL;
TaskHandle_t monitor_task_handle = NULL;
SemaphoreHandle_t demo_semaphore = NULL;

// Task names
const char* state_names[] = {"Running","Ready","Blocked","Suspended","Deleted","Invalid"};
const char* get_state_name(eTaskState state) { if(state<=eDeleted) return state_names[state]; return state_names[5]; }

// =================== Step 1: Basic Task States =================== //
void state_demo_task(void *pvParameters)
{
    ESP_LOGI(TAG,"State Demo Task started");
    int cycle=0;
    while(1){
        cycle++;
        // Running
        ESP_LOGI(TAG,"Cycle %d: RUNNING",cycle);
        gpio_set_level(LED_RUNNING,1);
        gpio_set_level(LED_READY,0);
        gpio_set_level(LED_BLOCKED,0);
        gpio_set_level(LED_SUSPENDED,0);
        for(int i=0;i<1000000;i++){volatile int dummy=i*2;}
        // Ready
        ESP_LOGI(TAG,"READY (yield)");
        gpio_set_level(LED_RUNNING,0);
        gpio_set_level(LED_READY,1);
        taskYIELD();
        vTaskDelay(pdMS_TO_TICKS(100));
        // Blocked (semaphore)
        ESP_LOGI(TAG,"BLOCKED (waiting for semaphore)");
        gpio_set_level(LED_READY,0);
        gpio_set_level(LED_BLOCKED,1);
        if(xSemaphoreTake(demo_semaphore,pdMS_TO_TICKS(2000))==pdTRUE){
            ESP_LOGI(TAG,"Got semaphore! RUNNING again");
            gpio_set_level(LED_BLOCKED,0);
            gpio_set_level(LED_RUNNING,1);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            ESP_LOGI(TAG,"Semaphore timeout");
            gpio_set_level(LED_BLOCKED,0);
        }
        // Blocked by delay
        ESP_LOGI(TAG,"BLOCKED (vTaskDelay)");
        gpio_set_level(LED_RUNNING,0);
        gpio_set_level(LED_BLOCKED,1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(LED_BLOCKED,0);
    }
}

void ready_state_demo_task(void *pvParameters)
{
    while(1){
        for(int i=0;i<100000;i++){volatile int dummy=i;}
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// =================== Step 2: Advanced State Transitions =================== //
void self_deleting_task(void *pvParameters)
{
    int *lifetime=(int*)pvParameters;
    ESP_LOGI(TAG,"Self-deleting task will live %d sec",*lifetime);
    for(int i=*lifetime;i>0;i--){
        ESP_LOGI(TAG,"Countdown: %d",i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG,"Self-deleting task going to DELETED state");
    vTaskDelete(NULL);
}

void external_delete_task(void *pvParameters)
{
    int count=0;
    while(1){
        ESP_LOGI(TAG,"External task running: %d",count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// =================== Step 3: Control & Monitoring =================== //
void control_task(void *pvParameters)
{
    ESP_LOGI(TAG,"Control Task started");
    bool suspended=false;
    int cycle=0;
    bool external_deleted=false;
    TaskHandle_t external_delete_handle=(TaskHandle_t)pvParameters;

    while(1){
        cycle++;
        // Suspend/Resume button
        if(gpio_get_level(BUTTON1_PIN)==0){
            vTaskDelay(pdMS_TO_TICKS(50));
            if(!suspended){
                ESP_LOGW(TAG,"Suspending State Demo Task");
                vTaskSuspend(state_demo_task_handle);
                gpio_set_level(LED_SUSPENDED,1);
                suspended=true;
            } else {
                ESP_LOGW(TAG,"Resuming State Demo Task");
                vTaskResume(state_demo_task_handle);
                gpio_set_level(LED_SUSPENDED,0);
                suspended=false;
            }
            while(gpio_get_level(BUTTON1_PIN)==0)vTaskDelay(pdMS_TO_TICKS(10));
        }
        // Give semaphore button
        if(gpio_get_level(BUTTON2_PIN)==0){
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGW(TAG,"Giving semaphore");
            xSemaphoreGive(demo_semaphore);
            while(gpio_get_level(BUTTON2_PIN)==0)vTaskDelay(pdMS_TO_TICKS(10));
        }
        // Delete external task after 15 sec
        if(cycle==150 && !external_deleted && external_delete_handle!=NULL){
            ESP_LOGW(TAG,"Deleting external task externally");
            vTaskDelete(external_delete_handle);
            external_deleted=true;
        }
        // Task state report
        if(cycle%30==0){
            eTaskState state=eTaskGetState(state_demo_task_handle);
            UBaseType_t prio=uxTaskPriorityGet(state_demo_task_handle);
            UBaseType_t stack_remain=uxTaskGetStackHighWaterMark(state_demo_task_handle);
            ESP_LOGI(TAG,"State Demo Task: State=%s, Priority=%d, Stack=%d bytes",
                     get_state_name(state),prio,stack_remain*sizeof(StackType_t));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void system_monitor_task(void *pvParameters)
{
    // แก้ไข: ไม่เรียก vTaskList / vTaskGetRunTimeStats เพื่อให้ compile ผ่าน
    while(1){
        ESP_LOGI(TAG,"System Monitor alive...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// =================== App Main =================== //
void app_main(void)
{
    ESP_LOGI(TAG,"=== FreeRTOS Task States Demo ===");

    // GPIO Config
    gpio_config_t io_conf={
        .intr_type=GPIO_INTR_DISABLE,
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1ULL<<LED_RUNNING)|(1ULL<<LED_READY)|(1ULL<<LED_BLOCKED)|(1ULL<<LED_SUSPENDED),
        .pull_down_en=0,
        .pull_up_en=0
    };
    gpio_config(&io_conf);

    gpio_config_t btn_conf={
        .intr_type=GPIO_INTR_DISABLE,
        .mode=GPIO_MODE_INPUT,
        .pin_bit_mask=(1ULL<<BUTTON1_PIN)|(1ULL<<BUTTON2_PIN),
        .pull_up_en=1,
        .pull_down_en=0
    };
    gpio_config(&btn_conf);

    demo_semaphore=xSemaphoreCreateBinary();

    // Step1 Tasks
    xTaskCreate(state_demo_task,"StateDemo",4096,NULL,3,&state_demo_task_handle);
    xTaskCreate(ready_state_demo_task,"ReadyDemo",2048,NULL,3,NULL);

    // Step2 Tasks
    static int self_delete_time=10;
    xTaskCreate(self_deleting_task,"SelfDelete",2048,&self_delete_time,2,NULL);

    TaskHandle_t external_delete_handle=NULL;
    xTaskCreate(external_delete_task,"ExtDelete",2048,NULL,2,&external_delete_handle);

    // Step3 Tasks
    xTaskCreate(control_task,"Control",3072,(void*)external_delete_handle,4,&control_task_handle);
    xTaskCreate(system_monitor_task,"Monitor",4096,NULL,1,&monitor_task_handle);
}

