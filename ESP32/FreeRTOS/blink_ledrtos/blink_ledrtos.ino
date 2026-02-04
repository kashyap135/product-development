/*What is an RTOS
A Real‑Time Operating System (RTOS) is an operating system designed to run multiple tasks with strict timing guarantees. Instead of “best effort” scheduling like general OSes, an RTOS ensures tasks meet deadlines—especially for control, sensing, and communication in embedded systems.
- Determinism: Tasks execute predictably; latency and jitter are minimized.
- Multitasking: Many independent tasks (threads) run “concurrently,” each with its own stack.
- Priorities: Higher‑priority tasks preempt lower ones to meet deadlines.
- Scheduling: Typically fixed‑priority preemptive; some RTOSes support round‑robin among equal priorities.
- Synchronization: Semaphores, mutexes, queues, and event groups coordinate tasks safely.
- Timing services: Ticks, delays, timers, and timeouts for precise control.
- Memory model: Stacks per task; optional static allocation for reliability.
- Interrupt integration: ISRs signal tasks via queues/semaphores for fast, safe handoff.

Why use an RTOS
- Responsiveness: Critical tasks (e.g., safety checks) run immediately when needed.
- Modularity: Each function—sensing, control, logging, comms—lives in its own task.
- Maintainability: Clear separation reduces complexity and coupling.
- Scalability: Add features by adding tasks and IPC, not giant loops.
- Reliability: Priority and timing rules prevent starvation and missed deadlines.

Typical RTOS building blocks
- Tasks: Independent functions with their own stacks.
- Scheduler: Chooses the next task based on priority and readiness.
- Queues: Thread‑safe message passing (bytes, structs).
- Semaphores/Mutexes: Resource protection and signaling.
- Timers: Callbacks at precise intervals.
- Delays: Non‑blocking sleeps that yield CPU to other tasks.*/


//RTOS Blink LED Example (FreeRTOS)
#include <Arduino.h>//- Arduino.h → gives access to hardware functions (pinMode, digitalWrite).
//#include <FreeRTOS.h>//- FreeRTOS.h → provides RTOS APIs (xTaskCreate, vTaskDelay, vTaskStartScheduler).
int LED_BUILTIN = 13;
// Task to blink LED

void TaskBlink(void *pvParameters) { //- TaskBlink → defines what the task does
    pinMode(LED_BUILTIN, OUTPUT);   // configure LED pin as output
    while (1) { //infinite loop
    Serial.print("TaskBlink running at: "); //prints label
        Serial.println(xTaskGetTickCount() * portTICK_PERIOD_MS); //prints current time in ms since scheduler start.

        digitalWrite(LED_BUILTIN, HIGH);   // turn LED ON
        vTaskDelay(500 / portTICK_PERIOD_MS); // wait 500 ms (non-blocking) - vTaskDelay(500 / portTICK_PERIOD_MS) → sleeps for 500 ms without blocking other tasks.
        digitalWrite(LED_BUILTIN, LOW);    // turn LED OFF
        vTaskDelay(500 / portTICK_PERIOD_MS); // wait 500 ms - vTaskDelay(500 / portTICK_PERIOD_MS) → sleeps again for 500 ms.
    }
}

void setup() {
    // Create the LED blink task
    Serial.begin(115200);
    xTaskCreate(
        TaskBlink,          // Task function
        "Blink",            // Task name (for debugging)
        2048,                // Stack size (words)
        NULL,               // Parameters (none here)
        1,                  // Priority (1 = normal)
        NULL                // Task handle (not stored)
    );

    // Start the scheduler
    vTaskStartScheduler(); //- vTaskStartScheduler() → starts the RTOS scheduler. From now on, FreeRTOS decides which task runs.
}

void loop() {
    // Empty: RTOS takes control, loop() is unused
}






