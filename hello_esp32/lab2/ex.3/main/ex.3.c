#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "ERROR_DEMO";

void error_handling_demo(void)
{
    ESP_LOGI(TAG, "=== Error Handling Demo ===");
    
    esp_err_t result;
    
    // Success case
    result = ESP_OK;
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Operation completed successfully");
    }
    
    // Error case: No memory
    result = ESP_ERR_NO_MEM;
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Error: %s", esp_err_to_name(result));
    }
    
    // Error case: Invalid argument (non-fatal)
    result = ESP_ERR_INVALID_ARG;
    ESP_ERROR_CHECK_WITHOUT_ABORT(result);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Non-fatal error: %s", esp_err_to_name(result));
    }
}

// ตัวอย่างการเรียกใช้งาน
void app_main(void)
{
    error_handling_demo();
}
