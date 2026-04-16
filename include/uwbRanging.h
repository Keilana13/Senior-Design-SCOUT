#ifndef UWB_RANGING_H
#define UWB_RANGING_H
//#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <DW1000.h>

class UwbRanging {
public:
    // We pass pins here so main.cpp controls the hardware layout
    UwbRanging(int csPin, int irqPin, int rstPin);

    // what the main.cpp can see and call
    void setupUWB();
    void loopUWB();
    float getLatestRange(); // pass the ranging distance to the main

private:
    int _cs, _irq, _rst;
    // static void handleSent(); // callback functions
    // static void handleReceived();
};

#endif