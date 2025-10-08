# วิวัฒนาการของ Multitasking ในไมโครคอนโทรลเลอร์

## บทนำ

การพัฒนาระบบฝังตัว (Embedded Systems) ได้ผ่านการวิวัฒนาการมาอย่างยาวนาน ตั้งแต่ยุคแรกเริ่มที่ไมโครคอนโทรลเลอร์สามารถทำงานได้เพียงงานเดียวในเวลาหนึ่ง จนถึงปัจจุบันที่สามารถจัดการงานหลายอย่างพร้อมกันได้อย่างมีประสิทธิภาพ

---

## 1. ยุคเริ่มต้น: Single Task Real-time Systems

### 1.1 ลักษณะการทำงาน

ในยุคแรกเริ่มของไมโครคอนโทรลเลอร์ (ประมาณปี 1970-1980) ระบบส่วนใหญ่ถูกออกแบบให้ทำงานแบบ **Single Task** หรือ **Super Loop**

```c
int main() {
    initialize_system();
    
    while(1) {  // Main Loop
        read_sensors();
        process_data();
        control_actuators();
        update_display();
        // รอให้ทุกงานเสร็จก่อนเริ่มรอบใหม่
    }
}
```

#### ไดอะแกรม Single Task System

``` mermaid
graph TD
    A[Start] --> B[Initialize System]
    B --> C[Read Sensors]
    C --> D[Process Data]
    D --> E[Control Actuators]
    E --> F[Update Display]
    F --> C
    
    style A fill:#e1f5fe
    style B fill:#f3e5f5
    style C fill:#fff3e0
    style D fill:#fff3e0
    style E fill:#fff3e0
    style F fill:#fff3e0
```

**Timeline Diagram:**
```
Time:  0ms    50ms   100ms  150ms  200ms  250ms
Task:  [Sensor][Process][Actuator][Display][Sensor]...
CPU:   [████████████████████████████████████████]
```

### 1.2 ข้อจำกัดของระบบ Single Task

1. **การตอบสนองล่าช้า (Poor Response Time)**
   - หากมีงานหนึ่งใช้เวลานาน งานอื่นจะต้องรอ
   - ไม่เหมาะสำหรับงานที่ต้องการตอบสนองเร็ว

2. **ไม่มีความยืดหยุ่น**
   - ไม่สามารถปรับเปลี่ยนลำดับความสำคัญของงานได้
   - ยากต่อการเพิ่มหรือลดงาน

3. **การใช้ทรัพยากรไม่เต็มประสิทธิภาพ**
   - CPU อาจว่างระหว่างรอข้อมูลจาก peripheral

---

## 2. การพัฒนาสู่ Time-Sharing Systems

### 2.1 แนวคิดการแบ่งเวลา

เมื่อความซับซ้อนของระบบเพิ่มขึ้น นักพัฒนาเริ่มใช้เทคนิค **Time-sharing** หรือการแบ่งเวลาให้แต่ละงาน

```c
// ตัวอย่าง Simple Time Slicing
int main() {
    initialize_system();
    unsigned int task_counter = 0;
    
    while(1) {
        switch(task_counter % 4) {
            case 0: read_sensors(); break;
            case 1: process_data(); break;
            case 2: control_actuators(); break;
            case 3: update_display(); break;
        }
        task_counter++;
        delay(10); // Time slice = 10ms
    }
}
```

#### ไดอะแกรม Time-Sharing System

```mermaid
graph TD
    A[Start] --> B[Initialize]
    B --> C[Task Scheduler]
    C --> D{Task Counter % 4}
    D -->|0| E[Read Sensors]
    D -->|1| F[Process Data]
    D -->|2| G[Control Actuators]
    D -->|3| H[Update Display]
    E --> I[Delay 10ms]
    F --> I
    G --> I
    H --> I
    I --> J[Counter++]
    J --> C
    
    style A fill:#e1f5fe
    style B fill:#f3e5f5
    style C fill:#e8f5e8
    style D fill:#fff9c4
    style E fill:#fff3e0
    style F fill:#fff3e0
    style G fill:#fff3e0
    style H fill:#fff3e0
```

**Timeline Diagram:**
```
Time:    0    10    20    30    40    50    60    70    80ms
Task A:  [██]        [██]        [██]        [██]
Task B:       [██]        [██]        [██]        [██]
Task C:            [██]        [██]        [██]
Task D:                 [██]        [██]        [██]
CPU:     [████████████████████████████████████████]
```

### 2.2 ปัญหาที่เกิดขึ้นจากการแบ่งเวลา

1. **ความซับซ้อนในการจัดการเวลา**
   - ยากต่อการกำหนด time slice ที่เหมาะสม
   - งานบางอย่างอาจต้องการเวลามากกว่างานอื่น

2. **การสูญเสียข้อมูล (Data Loss)**
   - หากงานไม่เสร็จสิ้นใน time slice ที่กำหนด
   - ข้อมูลอาจเสียหายหรือสูญหาย

3. **ความซับซ้อนในการ Debug**
   - ยากต่อการติดตามปัญหาเมื่อมีงานหลายอย่างทำงานสลับกัน

---

## 3. ผลกระทบและข้อจำกัดของระบบแบ่งเวลา

### 3.1 ผลกระทบเชิงลบ

1. **Context Switching Overhead**
   ```c
   // เวลาที่สูญเสียในการเปลี่ยนงาน
   save_current_context();    // 50-100 CPU cycles
   load_next_context();       // 50-100 CPU cycles
   // เวลาที่สูญเสีย = 5-10% ของ CPU time
   ```

#### ไดอะแกรม Context Switching

```mermaid
sequenceDiagram
    participant Task_A
    participant CPU
    participant RTOS
    participant Task_B
    
    Task_A->>CPU: Running
    Note over CPU: Timer Interrupt
    CPU->>RTOS: Context Switch Request
    RTOS->>RTOS: Save Task A Context<br/>(Registers, Stack Pointer)
    RTOS->>RTOS: Load Task B Context<br/>(Registers, Stack Pointer)
    RTOS->>Task_B: Resume Execution
    Task_B->>CPU: Running
    
    Note over RTOS: Overhead: 100-200 CPU cycles
```

**Context Switching Timeline:**
```
Time:      0    5    10   15   20   25   30   35ms
Task A:    [████]     [████]     [████]
Context:        [░░]      [░░]      [░░]  ← Overhead
Task B:           [████]     [████]     [████]
CPU:       [████████████████████████████████]
           ^    ^ ^  ^    ^ ^  ^    ^
           |    | |  |    | |  |    |
           Run  |S|  Run  |S|  Run  Context
                |a|       |a|       Switch
                |v|       |v|       Overhead
                |e|       |e|
```

2. **Priority Inversion**
   - งานสำคัญอาจถูกหน่วงเวลาโดยงานที่สำคัญน้อยกว่า

3. **Resource Contention**
   - การแย่งชิงทรัพยากรระหว่างงานต่างๆ

### 3.2 ข้อจำกัดในการพัฒนา

1. **ความซับซ้อนของโค้ด**
2. **การจัดการ Memory แบบ Manual**
3. **ไม่มีมาตรฐานในการพัฒนา**

---

## 4. ยุคของ Real-Time Operating System (RTOS)

### 4.1 การเกิดขึ้นของ RTOS

ด้วยข้อจำกัดของระบบแบ่งเวลาแบบ manual นักพัฒนาจึงสร้าง **Real-Time Operating System** เพื่อจัดการปัญหาเหล่านี้

### 4.2 คุณสมบัติหลักของ RTOS

1. **Deterministic Response Time**
   - รับประกันเวลาตอบสนองที่แน่นอน

2. **Priority-based Scheduling**
   - จัดลำดับงานตามความสำคัญ

3. **Inter-task Communication**
   - ระบบสื่อสารระหว่างงานที่มีประสิทธิภาพ

4. **Memory Management**
   - จัดการหน่วยความจำอย่างมีระบบ

---

## 5. เทคนิคการทำ Multitasking

### 5.1 Cooperative Multitasking

**หลักการ:** แต่ละ task จะสละ CPU เมื่อทำงานเสร็จ

```c
// ตัวอย่าง Cooperative Multitasking
void task1() {
    while(1) {
        do_task1_work();
        task_yield(); // สละ CPU ให้ task อื่น
    }
}

void task2() {
    while(1) {
        do_task2_work();
        task_yield(); // สละ CPU ให้ task อื่น
    }
}
```

#### ไดอะแกรม Cooperative Multitasking

```mermaid
sequenceDiagram
    participant Scheduler
    participant Task1
    participant Task2
    participant Task3
    
    Scheduler->>Task1: Start Task1
    Task1->>Task1: Do Work
    Task1->>Scheduler: yield() - สละ CPU
    Scheduler->>Task2: Start Task2
    Task2->>Task2: Do Work
    Task2->>Scheduler: yield() - สละ CPU
    Scheduler->>Task3: Start Task3
    Task3->>Task3: Do Work
    Task3->>Scheduler: yield() - สละ CPU
    Scheduler->>Task1: Resume Task1
```

**Timeline Diagram:**
```
Time:    0    20    40    60    80    100   120   140ms
Task1:   [████████]         [████████]         [████████]
Task2:            [███████]          [███████]
Task3:                    [██████]            [██████]
CPU:     [████████████████████████████████████████████]
         ^        ^       ^        ^       ^
         yield    yield   yield    yield   yield
```

**ข้อดี:**
- ใช้ทรัพยากรน้อย
- ไม่มี context switching overhead มาก
- เหมาะสำหรับระบบที่มีทรัพยากรจำกัด

**ข้อเสีย:**
- task ที่มี bug อาจทำให้ระบบทั้งหมดหยุดทำงาน
- ไม่เหมาะสำหรับงาน real-time ที่เข้มงวด
- ขึ้นอยู่กับความร่วมมือของแต่ละ task

### 5.2 Preemptive Multitasking

**หลักการ:** RTOS จะบังคับเปลี่ยน task ตาม priority และ time slice

```c
// ตัวอย่าง Preemptive Multitasking ใน FreeRTOS
void high_priority_task(void *pvParameters) {
    while(1) {
        handle_emergency();
        vTaskDelay(pdMS_TO_TICKS(100)); // Sleep 100ms
    }
}

void low_priority_task(void *pvParameters) {
    while(1) {
        background_processing();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep 1s
    }
}
```

#### ไดอะแกรม Preemptive Multitasking

```mermaid
sequenceDiagram
    participant Timer as System Timer
    participant Scheduler as RTOS Scheduler
    participant High as High Priority Task
    participant Low as Low Priority Task
    
    Timer->>Scheduler: Tick Interrupt
    Scheduler->>Low: Start Low Priority
    Low->>Low: Processing...
    Timer->>Scheduler: Tick Interrupt
    Note over High: High Priority Task Ready!
    Scheduler->>Low: Preempt (บังคับหยุด)
    Scheduler->>High: Start High Priority
    High->>High: Handle Emergency
    High->>Scheduler: Task Complete/Sleep
    Scheduler->>Low: Resume Low Priority
    Low->>Low: Continue Processing...
```

**Timeline Diagram:**
```
Time:      0    10    20    30    40    50    60    70ms
High Pri:        [██████]     [██████]     [██████]
Low Pri:   [████]      [███]       [███]       [███]
CPU:       [████████████████████████████████████████]
           ^    ^      ^   ^       ^   ^       ^
           |    |      |   |       |   |       |
        Start Preempt Resume|    Preempt|    Resume
                           Resume    Resume
```

**Priority Inversion Example:**
```mermaid
graph TD
    A[High Priority Task] -->|Blocked by| B[Resource Lock]
    B -->|Held by| C[Low Priority Task]
    D[Medium Priority Task] -->|Preempts| C
    C -->|Cannot complete| B
    A -->|Waiting...| E[Delayed Response]
    
    style A fill:#ffcdd2
    style C fill:#fff3e0
    style D fill:#e1f5fe
    style E fill:#ffebee
```

**ข้อดี:**
- รับประกันเวลาตอบสนอง
- task หนึ่งไม่สามารถครอบครอง CPU ได้ตลอด
- เหมาะสำหรับ real-time systems

**ข้อเสีย:**
- ใช้ทรัพยากรมากกว่า
- ความซับซ้อนในการ debug
- Context switching overhead

### 5.3 Round-Robin Scheduling

**หลักการ:** แต่ละ task ได้รับ time slice เท่ากัน

```c
// การกำหนด time slice ใน FreeRTOS
#define configTICK_RATE_HZ 1000        // 1ms tick
#define configTIME_SLICE_MS 10         // 10ms per task

// Task จะถูกสลับทุก 10ms
```

#### ไดอะแกรม Round-Robin Scheduling

```mermaid
graph LR
    A[Task A<br/>Priority 2] --> B[Task B<br/>Priority 2]
    B --> C[Task C<br/>Priority 2]
    C --> A
    
    subgraph "Time Slices (10ms each)"
        D[10ms] --> E[10ms] --> F[10ms] --> G[10ms]
    end
    
    style A fill:#fff3e0
    style B fill:#e1f5fe
    style C fill:#f3e5f5
```

**Timeline Diagram:**
```
Time:    0    10    20    30    40    50    60    70    80ms
Task A:  [████]          [████]          [████]
Task B:       [████]          [████]          [████]
Task C:            [████]          [████]          [████]
CPU:     [████████████████████████████████████████████]
         ^    ^    ^    ^    ^    ^    ^    ^    ^
         |    |    |    |    |    |    |    |    |
      Switch every 10ms (Time Slice)
```

**Round-Robin Queue:**
```mermaid
graph TD
    subgraph "Ready Queue (Same Priority)"
        Q1[Task A] --> Q2[Task B]
        Q2 --> Q3[Task C]
        Q3 --> Q1
    end
    
    CPU[CPU] <--> Current[Current Running Task]
    Current <--> Q1
    
    style CPU fill:#ffcdd2
    style Current fill:#c8e6c9
```

**ข้อดี:**
- ความยุติธรรมในการใช้ CPU
- เหมาะสำหรับ task ที่มีความสำคัญเท่ากัน

**ข้อเสีย:**
- ไม่เหมาะสำหรับ task ที่มี priority ต่างกัน
- อาจไม่ตอบสนองความต้องการ real-time

### 5.4 Priority-based Scheduling

**หลักการ:** Task ที่มี priority สูงกว่าจะทำงานก่อน

```c
// การสร้าง task ด้วย priority ต่างกัน
xTaskCreate(emergency_handler, "Emergency", 1024, NULL, 5, NULL);  // สูงสุด
xTaskCreate(data_logger, "Logger", 1024, NULL, 3, NULL);           // กลาง  
xTaskCreate(led_blink, "LED", 512, NULL, 1, NULL);                 // ต่ำสุด
```

#### ไดอะแกรม Priority-based Scheduling

```mermaid
graph TD
    subgraph "Priority Levels"
        P5[Priority 5<br/>Emergency Handler]
        P3[Priority 3<br/>Data Logger]
        P1[Priority 1<br/>LED Blink]
    end
    
    CPU[CPU] --> Current[Currently Running]
    
    P5 -->|Always Preempts| P3
    P5 -->|Always Preempts| P1
    P3 -->|Preempts| P1
    
    style P5 fill:#ffcdd2
    style P3 fill:#fff3e0
    style P1 fill:#e1f5fe
    style CPU fill:#c8e6c9
```

**Timeline with Priority Interruption:**
```
Time:     0    10    20    30    40    50    60    70ms
Priority 5:         [███]    [███]         [███]
Priority 3:    [██]     [██]     [███]  [██]
Priority 1:[██]                              [███]
CPU:      [████████████████████████████████████████]
          ^  ^     ^   ^    ^   ^     ^  ^   ^
          |  |     |   |    |   |     |  |   |
          |  P5    |   P5   |   P3    |  P5  P1
          P1 Pre-  P3  Pre- P3  Res-  P3 Pre-Resume
             empt     empt     ume      empt
```

**Priority Queue Structure:**
```mermaid
graph TD
    subgraph "Ready Queues by Priority"
        Q5[Priority 5 Queue]
        Q4[Priority 4 Queue]
        Q3[Priority 3 Queue]
        Q2[Priority 2 Queue]
        Q1[Priority 1 Queue]
    end
    
    Scheduler[Scheduler] --> Q5
    Scheduler --> Q4
    Scheduler --> Q3
    Scheduler --> Q2
    Scheduler --> Q1
    
    Q5 --> T5[Emergency Handler]
    Q3 --> T3[Data Logger]
    Q1 --> T1[LED Blink]
    
    style Q5 fill:#ffcdd2
    style Q3 fill:#fff3e0
    style Q1 fill:#e1f5fe
    style Scheduler fill:#c8e6c9
```

**ข้อดี:**
- เหมาะสำหรับ real-time systems
- ตอบสนองงานสำคัญได้เร็ว
- มีความยืดหยุ่นสูง

**ข้อเสีย:**
- Priority inversion
- Starvation (task priority ต่ำอาจไม่ได้ทำงาน)
- ความซับซ้อนในการออกแบบ

---

## 6. การเปรียบเทียบเทคนิคต่างๆ

| เทคนิค | Response Time | Resource Usage | Complexity | Real-time Suitability |
|---------|---------------|----------------|------------|----------------------|
| Single Task | Poor | Low | Low | Poor |
| Time Sharing | Medium | Medium | Medium | Medium |
| Cooperative | Good | Low | Medium | Medium |
| Preemptive | Excellent | High | High | Excellent |
| Round-Robin | Medium | Medium | Medium | Medium |
| Priority-based | Excellent | High | High | Excellent |

### ไดอะแกรมเปรียบเทียบการตอบสนอง

```mermaid
graph LR
    subgraph "Evolution Timeline"
        A[Single Task<br/>1970s] --> B[Time Sharing<br/>1980s]
        B --> C[Cooperative<br/>1990s]
        C --> D[Preemptive RTOS<br/>2000s+]
    end
    
    subgraph "Characteristics"
        A --> A1[Sequential<br/>Blocking]
        B --> B1[Fixed Time Slices<br/>Manual Scheduling]
        C --> C1[Voluntary Yielding<br/>Low Overhead]
        D --> D1[Priority-based<br/>Guaranteed Response]
    end
    
    style A fill:#ffcdd2
    style B fill:#fff3e0
    style C fill:#e1f5fe
    style D fill:#c8e6c9
```

### ไดอะแกรมการใช้ทรัพยากร

```mermaid
graph TD
    subgraph "Resource Usage Comparison"
        Single[Single Task<br/>RAM: 1KB<br/>CPU: 100%<br/>Response: Poor]
        TimeShare[Time Sharing<br/>RAM: 2-4KB<br/>CPU: 90%<br/>Response: Fair]
        Coop[Cooperative<br/>RAM: 4-8KB<br/>CPU: 85%<br/>Response: Good]
        Preempt[Preemptive<br/>RAM: 8-16KB<br/>CPU: 75%<br/>Response: Excellent]
    end
    
    style Single fill:#ffcdd2
    style TimeShare fill:#fff3e0
    style Coop fill:#e1f5fe
    style Preempt fill:#c8e6c9
```

---

## 7. ข้อพิจารณาในการเลือกเทคนิค

### 7.1 สำหรับระบบ Real-time เข้มงวด
- ใช้ **Preemptive Priority-based Scheduling**
- มี RTOS ที่รับรองเวลาตอบสนอง

### 7.2 สำหรับระบบที่มีทรัพยากรจำกัด
- ใช้ **Cooperative Multitasking**
- หรือ **Simple Time Sharing**

### 7.3 สำหรับระบบที่มีความซับซ้อนปานกลาง
- ใช้ **RTOS แบบ Preemptive**
- เช่น FreeRTOS, RT-Thread

---

## 8. บทสรุป

วิวัฒนาการของ multitasking ในไมโครคอนโทรลเลอร์แสดงให้เห็นถึงการพัฒนาที่ต่อเนั่อง:

1. **Single Task** → เหมาะสำหรับงานง่ายๆ
2. **Time Sharing** → เพิ่มความสามารถแต่มีข้อจำกัด
3. **RTOS** → แก้ปัญหาและเพิ่มประสิทธิภาพ

### ไดอะแกรมสรุปวิวัฒนาการ

```mermaid
flowchart LR
    A[1970s: Single Task<br/>Sequential Programming] --> B[1980s: Time Sharing<br/>Manual Task Switching]
    B --> C[1990s: Cooperative<br/>Voluntary Yielding]
    C --> D[2000s+: Preemptive RTOS<br/>Priority-based Scheduling]
    
    subgraph Problems
        P1[Poor Response Time]
        P2[Resource Contention]
        P3[Priority Inversion]
        P4[Context Overhead]
    end
    
    subgraph Solutions
        S1[Deterministic Response]
        S2[Priority Inheritance]
        S3[Resource Management]
        S4[Optimized Context Switch]
    end
    
    A --> P1
    B --> P2
    C --> P3
    D --> S1
    D --> S2
    D --> S3
    D --> S4
    
    style A fill:#ffcdd2
    style B fill:#fff3e0
    style C fill:#e1f5fe
    style D fill:#c8e6c9
    style P1 fill:#ffebee
    style P2 fill:#ffebee
    style P3 fill:#ffebee
    style P4 fill:#ffebee
    style S1 fill:#e8f5e8
    style S2 fill:#e8f5e8
    style S3 fill:#e8f5e8
    style S4 fill:#e8f5e8
```

### การตัดสินใจเลือกเทคนิค

```mermaid
graph TD
    Start[เริ่มต้นออกแบบระบบ] --> Q1{Real-time<br/>Requirements?}
    
    Q1 -->|Hard Real-time| RT[Preemptive RTOS<br/>Priority-based]
    Q1 -->|Soft Real-time| Q2{Resource<br/>Constraints?}
    Q1 -->|No Real-time| Q3{Multiple Tasks?}
    
    Q2 -->|Very Limited| Coop[Cooperative<br/>Multitasking]
    Q2 -->|Moderate| RT
    
    Q3 -->|Yes| Time[Time Sharing<br/>or Cooperative]
    Q3 -->|No| Single[Single Task<br/>Super Loop]
    
    style RT fill:#c8e6c9
    style Coop fill:#e1f5fe
    style Time fill:#fff3e0
    style Single fill:#ffcdd2
```

การเลือกเทคนิคที่เหมาะสมขึ้นอยู่กับ:
- **ข้อกำหนดของระบบ** (Real-time requirements)
- **ทรัพยากรที่มี** (Memory, CPU power)
- **ความซับซ้อนที่ยอมรับได้**
- **งบประมาณและเวลาในการพัฒนา**

RTOS สมัยใหม่เช่น FreeRTOS ได้นำเอาเทคนิคต่างๆ มารวมกัน ทำให้นักพัฒนาสามารถสร้างระบบที่มีประสิทธิภาพและตอบสนองความต้องการที่หลากหลายได้