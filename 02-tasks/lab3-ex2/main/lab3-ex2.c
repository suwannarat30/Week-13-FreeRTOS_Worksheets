#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "EX2"

TaskHandle_t example_task_handle = NULL;

// ตัวอย่าง task ที่ทำงาน
void example_task(void *pvParameters)
{
    int counter=0;
    while(1){
        counter++;
        ESP_LOGI(TAG,"Example task cycle %d",counter);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Dynamic stack monitoring function
void dynamic_stack_monitor(TaskHandle_t task_handle, const char* task_name)
{
    static UBaseType_t previous=0;

    UBaseType_t current = uxTaskGetStackHighWaterMark(task_handle);
    if(previous!=0 && current<previous){
        ESP_LOGW(TAG,"%s stack usage increased by %d bytes",
                 task_name,(previous-current)*sizeof(StackType_t));
    }
    previous=current;
    ESP_LOGI(TAG,"%s current stack remaining: %d bytes",task_name,current*sizeof(StackType_t));
}

// Task สำหรับ monitor แบบต่อเนื่อง
void monitor_task(void *pvParameters)
{
    while(1){
        if(example_task_handle) dynamic_stack_monitor(example_task_handle,"ExampleTask");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG,"=== Exercise 2: Dynamic Stack Monitoring ===");

    // สร้างตัวอย่าง task
    xTaskCreate(example_task,"ExampleTask",1024,NULL,2,&example_task_handle);

    // สร้าง monitor task
    xTaskCreate(monitor_task,"MonitorTask",1024,NULL,3,NULL);
}
