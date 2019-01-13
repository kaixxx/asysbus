/**
 * aSysBus sleep example - receiver
 * by Kai, based on "simple" example from adlerweb
 * 
 * This example shows a basic low power aSysBus-node. The node sleeps most of the time,
 * waiting for new messages to process. 
 * In sleep mode it only consumes around 240uA (together with an Arduino Pro Mini where the power led 
 * and the regulator are removed).
 * Works best in conjunction with sleepSender.
 */

#include "asb.h"
#include "mcp_can_dfs.h"
#include <avr/sleep.h>

// adjust the following values to your hardware:
#define CAN_INT 2 // interrupt pin of the CAN controller
#define CAN_CS 4 // CS pin of the controller 
#define CAN_CRYSTAL MCP_8MHz // crystal type

//Create new ASB node using ID 0x124 as local address
//This is the "brain" and already handles routing between multiple interfaces and some messages like PING
ASB asb0(0x124);

//Start new CAN-Bus
ASB_CAN asbCan0(CAN_CS, CAN_125KBPS, CAN_CRYSTAL, CAN_INT);

void setup() {
  //Initialize Serial port
  Serial.begin(115200);
  Serial.println(F("ASB Reciever Node started"));

  //Attach the previously defined CAN-Bus to our controller
  // (Always attach the CAN bus to the controller before setting any sleep options!)
  Serial.print(F("Attach CAN..."));
  asb0.busAttach(&asbCan0);
  
  // Init Rs
  Serial.println(F("Init Rs Pin..."));
  asbCan0.setTransceiverStandbyPin(true, MCP_RX0BF);

  // Config sleep + wakeup on new message:
  asbCan0.setSleepWakeup(true);
  asbCan0.setAutoSleep(true);

  // Ignore duplicate messages
  asbCan0.setFilterDuplicates(true);

  Serial.println(F("done!"));
}

void loop() {
  //asb0.loop() handles all recurring tasks like processing received messages and returns a packet
  asbPacket pkg = asb0.loop();
  if(pkg.meta.busId != -1) { //If bus is -1 there was no new packet received
    Serial.println(F("---"));
    Serial.print(F("Type: 0x"));
    Serial.println(pkg.meta.type, HEX);
    Serial.print(F("Target: 0x"));
    Serial.println(pkg.meta.target, HEX);
    Serial.print(F("Source: 0x"));
    Serial.println(pkg.meta.source, HEX);
    Serial.print(F("Port: 0x"));
    Serial.println(pkg.meta.port, HEX);
    Serial.print(F("Length: 0x"));
    Serial.println(pkg.len, HEX);
    for(byte i=0; i<pkg.len; i++) {
      Serial.print(F(" 0x"));
      Serial.print(pkg.data[i], HEX);
    }
    if(pkg.len == 1 && pkg.data[0] == ASB_CMD_WAKE) {
      Serial.print(F(" (Wakeup message - ASB_CMD_WAKE)"));
    }
    Serial.println();
  } else { // no message waiting
    if(asbCan0.isSleeping()) {
      // Put the microcontroller to sleep
      Serial.println(F("---"));
      Serial.println(F("Going to sleep..."));
      Serial.flush();
      cli(); // Disable interrupts
      if(digitalRead(CAN_INT) == HIGH) // Make sure we havn't missed an interrupt. If an interrupt happens between now and sei()/sleep_cpu() then sleep_cpu() will immediately wake up again
      {
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sleep_bod_disable();
        sei();
        sleep_cpu();
      
        // Wake up:
        sleep_disable();
      }
      sei();
    }
  }
}

























