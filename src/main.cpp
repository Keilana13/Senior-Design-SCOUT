#include <Arduino.h>
#include "../include/uwbRanging.h"
// #include "motors.h"  <-- Future module
// #include "sensors.h" <-- Future module

// Task Handles
TaskHandle_t UWBTaskHandle = NULL;

UwbRanging uwbDevice(SS, 2, 9); // Initialize with SPI SS pin, IRQ pin, and RST pin

void UWBTask(void *pvParameters) {
  uwbDevice.setupUWB(); // Initialize UWB module
  for (;;) {
    uwbDevice.loopUWB(); // uwb ranging loop
    vTaskDelay(1 / portTICK_PERIOD_MS); // 1ms delay to yield to other tasks
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting SCOUT...");

  // tasks initialization
  xTaskCreatePinnedToCore(
    UWBTask,        // Function to implement the task
    "UWB_Task",     // Name of the task
    4096,           // Stack size
    NULL,           // Parameter passed into the task
    2,              // Priority (High priority for ranging)
    &UWBTaskHandle, // Task handle
    1               // Core 1
  );
}

void loop() {
  float dist = uwbDevice.getLatestRange();
  if (dist > 0)
    Serial.printf("Current Distance: %.2f m\n", dist);
  delay(500);
}