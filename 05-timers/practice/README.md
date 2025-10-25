# FreeRTOS Software Timers - Practical Labs

ชุดแบบฝึกหัดเกี่ยวกับ FreeRTOS Software Timers เพื่อเรียนรู้การใช้งาน Timer ในระบบ Real-time

## 📚 Labs Overview

| Lab | หัวข้อ | ระยะเวลา | ความยากง่าย |
|-----|---------|----------|-------------|
| **Lab 1** | [Basic Software Timers](lab1-basic-timers/) | 45 นาที | ⭐⭐⭐ |
| **Lab 2** | [Timer Applications](lab2-timer-applications/) | 60 นาที | ⭐⭐⭐⭐ |
| **Lab 3** | [Advanced Timer Management](lab3-advanced-timer-management/) | 75 นาที | ⭐⭐⭐⭐⭐ |

## 🎯 วัตถุประสงค์รวม

เมื่อเสร็จสิ้นการปฏิบัติการแล้ว นักศึกษาจะสามารถ:

1. **พื้นฐาน Timer Management**:
   - สร้างและจัดการ Software Timers
   - เข้าใจความแตกต่างระหว่าง One-shot และ Auto-reload
   - ใช้ Timer Callbacks อย่างมีประสิทธิภาพ

2. **Timer Applications**:
   - สร้างระบบ Watchdog Timer
   - พัฒนา LED Pattern Controller
   - ออกแบบ Sensor Sampling System

3. **Advanced Timer Concepts**:
   - จัดการ Timer Service Task
   - เพิ่มประสิทธิภาพ Timer Performance
   - สร้างระบบ Timer Health Monitoring

## 🔧 การเตรียมพร้อม

### Hardware Requirements
- ESP32 Development Board
- LEDs และ Resistors (220Ω)
- Breadboard และ Jumper Wires
- Sensors (เช่น DHT22, LDR) สำหรับ Lab 2-3

### Software Requirements
- ESP-IDF v4.4 หรือใหม่กว่า
- VS Code พร้อม ESP-IDF Extension
- Serial Monitor

### Configuration
ตรวจสอบการตั้งค่า FreeRTOS Timers:
```bash
idf.py menuconfig
# Component config → FreeRTOS → Software timers
CONFIG_FREERTOS_USE_TIMERS=y
CONFIG_FREERTOS_TIMER_TASK_PRIORITY=3
CONFIG_FREERTOS_TIMER_TASK_STACK_SIZE=2048
CONFIG_FREERTOS_TIMER_QUEUE_LENGTH=10
```

## 📋 Learning Path

### Beginner Level
1. เริ่มต้นจาก **Lab 1** เพื่อเรียนรู้พื้นฐาน
2. ศึกษา Timer Types และ Callback Functions
3. ทำความเข้าใจ Timer Service Task

### Intermediate Level
1. ทำ **Lab 2** เพื่อประยุกต์ใช้งานจริง
2. สร้างระบบที่ซับซ้อนมากขึ้น
3. เรียนรู้ Error Handling

### Advanced Level
1. ท้าทายตัวเองด้วย **Lab 3**
2. เพิ่มประสิทธิภาพและการจัดการ Resources
3. สร้างระบบ Production-ready

## 🎖️ ตัวชี้วัดความสำเร็จ

### Lab 1 - Basic Timers ✅
- [/ ] สร้าง One-shot และ Auto-reload Timers
- [/ ] เข้าใจ Timer Callbacks
- [ /] จัดการ Timer States (Start/Stop/Reset)
- [/ ] แสดงผล Timer Statistics

### Lab 2 - Timer Applications ✅
- [/ ] พัฒนาระบบ Watchdog Timer
- [/ ] สร้าง LED Pattern Controller
- [/ ] ออกแบบ Sensor Sampling System
- [/ ] จัดการ Multiple Timer Coordination

### Lab 3 - Advanced Management ✅
- [ /] เพิ่มประสิทธิภาพ Timer Performance
- [/ ] สร้าง Timer Health Monitoring
- [/ ] จัดการ Timer Resource Management
- [/ ] สร้างระบบ Error Recovery

## 📊 Tips & Best Practices

### Timer Callback Guidelines
```c
// ✅ Good - Short and simple
void good_callback(TimerHandle_t timer) {
    data_t data = {get_sensor_value(), xTaskGetTickCount()};
    xQueueSendFromISR(queue, &data, &woken);
    portYIELD_FROM_ISR(woken);
}

// ❌ Bad - Long blocking operations
void bad_callback(TimerHandle_t timer) {
    vTaskDelay(pdMS_TO_TICKS(1000));  // DON'T DO THIS!
    complex_processing();              // DON'T DO THIS!
}
```

### Memory Management
- ใช้ Static Allocation เมื่อเป็นไปได้
- จัดการ Dynamic Timer Creation/Deletion อย่างระมัดระวัง
- Monitor Timer Service Task Stack Usage

### Performance Optimization
- ตั้งค่า Timer Service Task Priority อย่างเหมาะสม
- เลี่ยงการสร้าง/ลบ Timer บ่อยๆ
- ใช้ Timer Pool สำหรับ Dynamic Timers

## 🔍 Debugging Timer Issues

### Common Problems
1. **Timer Callbacks ทำงานช้า**: ลด complexity ใน callback
2. **Timer Commands ล้มเหลว**: เช็ค Timer Service Task Priority
3. **Inconsistent Timing**: ตรวจสอบ System Load
4. **Memory Leaks**: จัดการ Dynamic Timer Lifecycle

### Debug Tools
```c
// Check timer states
if (xTimerIsTimerActive(timer)) {
    ESP_LOGI(TAG, "Timer is running");
    ESP_LOGI(TAG, "Period: %d ms", pdTICKS_TO_MS(xTimerGetPeriod(timer)));
}

// Monitor timer service task
TaskStatus_t task_status;
vTaskGetInfo(NULL, &task_status, pdTRUE, eInvalid);
ESP_LOGI(TAG, "Timer Task Stack HWM: %d", task_status.usStackHighWaterMark);
```

## 📚 Additional Resources

- [FreeRTOS Timer Documentation](https://www.freertos.org/RTOS-software-timer.html)
- [ESP32 Timer Examples](https://github.com/espressif/esp-idf/tree/master/examples/system/freertos)
- [Timer Performance Analysis](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/speed.html)

## 🚀 Next Steps

หลังจากเสร็จสิ้น Timer Labs:
1. ไปยัง [Event Groups](../../06-event-groups/) เพื่อเรียนรู้ Task Synchronization
2. ศึกษา [Memory Management](../../07-memory-management/) เพื่อเพิ่มประสิทธิภาพ
3. ปฏิบัติการ [ESP-IDF Specific](../../08-esp-idf-specific/) เพื่อความเชี่ยวชาญ

---
**หมายเหตุ**: อย่าลืมอ่านเอกสารประกอบก่อนเริ่มแต่ละ Lab และทำ Pre-lab Questions เพื่อเตรียมความพร้อม