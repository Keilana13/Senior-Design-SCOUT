#include "../include/uwbRanging.h"
#include <SPI.h>
#include <DW1000.h>

// connection pins
const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = SS; // spi select pin

// messages used in the ranging protocol
#define POLL 0
#define POLL_ACK 1
#define RANGE 2
#define RANGE_REPORT 3
#define RANGE_FAILED 255

// message flow state
volatile byte expectedMsgId = POLL_ACK;
// message sent/received state
volatile boolean sentAck = false;
volatile boolean receivedAck = false;

// timestamps to remember
DW1000Time timePollSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;

// data buffer
#define LEN_DATA 16
byte data[LEN_DATA];

// watchdog and reset period
uint32_t lastActivity;
uint32_t resetPeriod = 250;

// reply times (same on both sides for symm. ranging)
uint16_t replyDelayTimeUS = 3000;

// range result
float curRange = 0.0;

// function declarations
void noteActivity();
void resetInactive();
void handleSent();
void handleReceived();
void transmitPoll();
void transmitRange();
void receiver();

// constructor
UwbRanging::UwbRanging(int csPin, int irqPin, int rstPin) {
    _cs = csPin;
    _irq = irqPin;
    _rst = rstPin;
}

// initialization of the UWB module
void UwbRanging::setupUWB() {
    // DEBUG monitoring
    Serial.begin(115200);
    Serial.println(F("### DW1000-arduino-ranging-tag ###"));

    // initialize the driver/hardware
    DW1000.begin(_irq, _rst);
    DW1000.select(_cs);
    Serial.println("DW1000 initialized ...");

    // DW1000 configuration
    DW1000.newConfiguration();
    DW1000.setDefaults();
    DW1000.setDeviceAddress(2); // device address
    DW1000.setNetworkId(10); // network id
    DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER); // operation mode
    DW1000.commitConfiguration();
    Serial.println(F("Committed configuration ..."));

    // DEBUG chip info and registers pretty printed
    // print device id, unique id, network id & device address, and device mode
    char msg[128];
    DW1000.getPrintableDeviceIdentifier(msg);
    Serial.print("Device ID: ");Serial.println(msg);
    DW1000.getPrintableExtendedUniqueIdentifier(msg);
    Serial.print("Unique ID: "); Serial.println(msg);
    DW1000.getPrintableNetworkIdAndShortAddress(msg);
    Serial.print("Network ID & Device Address: "); Serial.println(msg);
    DW1000.getPrintableDeviceMode(msg);
    Serial.print("Device mode: "); Serial.println(msg);

    // attach callback for (successfully) sent and received messages
    DW1000.attachSentHandler(handleSent);
    DW1000.attachReceivedHandler(handleReceived);
    
    // anchor starts by transmitting a POLL message
    receiver();
    transmitPoll();
    noteActivity();
}

// ############## HELPER FUNCTIONS ############

void noteActivity() {
    // update activity timestamp, so that we do not reach "resetPeriod"
    lastActivity = millis();
}

void resetInactive() {
    // tag sends POLL and listens for POLL_ACK
    expectedMsgId = POLL_ACK;
    transmitPoll();
    noteActivity();
}

// five functions included in the 4-step ranging protocol
void handleSent() {
    // status change on sent success
    sentAck = true;
}

void handleReceived() {
    // status change on received success
    receivedAck = true;
}

void transmitPoll() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = POLL;
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
}

void transmitRange() {
    DW1000.newTransmit();
    DW1000.setDefaults();
    data[0] = RANGE;
    // delay sending the message and remember expected future sent timestamp
    DW1000Time deltaTime = DW1000Time(replyDelayTimeUS, DW1000Time::MICROSECONDS);
    timeRangeSent = DW1000.setDelay(deltaTime);
    timePollSent.getTimestamp(data + 1);
    timePollAckReceived.getTimestamp(data + 6);
    timeRangeSent.getTimestamp(data + 11);
    DW1000.setData(data, LEN_DATA);
    DW1000.startTransmit();
    //Serial.print("Expect RANGE to be sent @ "); Serial.println(timeRangeSent.getAsFloat());
}

void receiver() {
    DW1000.newReceive();
    DW1000.setDefaults();
    // so we don't need to restart the receiver manually
    DW1000.receivePermanently(true);
    DW1000.startReceive();
}

float UwbRanging::getLatestRange() {
    return curRange;
}

void UwbRanging::loopUWB(){
    if(!sentAck && !receivedAck) {
        // check if inactive -> restart protocol after 250ms
        if (millis() - lastActivity > resetPeriod) {
            resetInactive();
        }
        return;
    }
    // if tag successfully sent a message, check which one and update state
    if (sentAck) {
        sentAck = false;
        byte msgId = data[0];
        // POLL sent, record timestamp of transmission
        if (msgId == POLL) {
            DW1000.getTransmitTimestamp(timePollSent);
            //Serial.print("Sent POLL @ "); Serial.println(timePollSent.getAsFloat());
        } 
        // RANGE sent, record timestamp of transmission
        else if (msgId == RANGE) {
            DW1000.getTransmitTimestamp(timeRangeSent);
            noteActivity();
        }
    }
    // if tag successfully received a message, check which one and update state
    if (receivedAck) {
        receivedAck = false;
        // get message and parse
        DW1000.getData(data, LEN_DATA);
        byte msgId = data[0];
        if (msgId != expectedMsgId) {
            // unexpected message, start over again
            //Serial.print("Received wrong message # "); Serial.println(msgId);
            expectedMsgId = POLL_ACK;
            transmitPoll();
            return;
        }
        // POLL_ACK received, record timestamp of reception and send RANGE
        if (msgId == POLL_ACK) {
            DW1000.getReceiveTimestamp(timePollAckReceived);
            expectedMsgId = RANGE_REPORT;
            transmitRange();
            noteActivity();
        } 
        // RANGE_REPORT received, parse range and start new ranging exchange
        else if (msgId == RANGE_REPORT) {
            expectedMsgId = POLL_ACK;
            memcpy(&curRange, data + 1, 4);
            transmitPoll();
            noteActivity();
        } 
        // RANGE_FAILED received, start new ranging exchange
        else if (msgId == RANGE_FAILED) {
            expectedMsgId = POLL_ACK;
            transmitPoll();
            noteActivity();
        }
    }
}