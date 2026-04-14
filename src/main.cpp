#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

#define SPI_SCK 18 // SPI clock pin
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS 4 // Chip select pin
#define PIN_RST 27
#define PIN_IRQ 34 // Interrupt pin

void setup() {
  // Start serial for debugging
  Serial.begin(115200);
  delay(2000); // Time to open the monitor
  Serial.println("### S.C.O.U.T. UWB Connectivity Test ###");

  // Initialize the DW1000 driver
  DW1000.begin(PIN_IRQ, PIN_RST);
  DW1000.select(DW_CS);
  
  Serial.println("DW1000 Initialized. Fetching ID...");

  // Get the device ID
  char msg[128];
  DW1000.getPrintableDeviceIdentifier(msg);
  
  Serial.print("Device ID: ");
  Serial.println(msg);

  // verification
  if (strstr(msg, "DECA") != NULL) {
    Serial.println("SUCCESS: UWB chip is alive and responding!");
  } else {
    Serial.println("ERROR: Could not communicate with DW1000. Check your pins/soldering.");
  }
}

void loop() {

}