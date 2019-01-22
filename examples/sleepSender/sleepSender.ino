/**
 * aSysBus sleep example - sender
 * by Kai, based on "simple" example from adlerweb
 * 
 * This example shows a basic low power aSysBus send only node. The node sleeps most of the time,
 * transmitting a message every 4 seconds. 
 * In sleep mode it only consumes around 240uA (together with an Arduino Pro Mini where the power led 
 * and the regulator are removed).
 * Works best in conjunction with sleepReceiver.
 */
 
#include "asb.h"
#include "mcp_can_dfs.h"
#include <avr/sleep.h>
#include <avr/wdt.h>

// adjust the following values to your hardware:
#define CAN_INT 2 // interrupt pin of the CAN controller
#define CAN_CS 4 // CS pin of the controller 
#define CAN_CRYSTAL MCP_8MHz // crystal type

//Create new ASB node using ID 0x123 as local addess
//This is the "brain" and already handles routing between multiple interfaces and some messages like PING
ASB asb0(0x123);

//Start new CAN-Bus
ASB_CAN asbCan0(CAN_CS, CAN_125KBPS, CAN_CRYSTAL, CAN_INT);

bool sendOn = true;

// Watchdog interrupt, used to wake the MCU periodically. 
ISR (WDT_vect) 
{
   wdt_disable();  // disable watchdog
}  

void sleepMCU() 
// Sleep the MCU for four seconds.
// See http://www.gammon.com.au/power for details. 
// (You can make your life a lot easyer by using one of the numerous low power libraries for Arduino.)
{
  // disable ADC
  ADCSRA = 0;  

  // clear various "reset" flags
  MCUSR = 0;     
  // allow changes, disable reset
  WDTCSR = bit (WDCE) | bit (WDE);
  // set interrupt mode and an interval 
  WDTCSR = bit (WDIE) | bit (WDP3);    // set WDIE, and 4 seconds delay
  wdt_reset();  // pat the dog
  
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
  noInterrupts ();           // timed sequence follows
  sleep_enable();
 
  // turn off brown-out enable in software
  MCUCR = bit (BODS) | bit (BODSE);
  MCUCR = bit (BODS); 
  interrupts ();             // guarantees next instruction executed
  sleep_cpu ();  
  
  // cancel sleep as a precaution
  sleep_disable();
}

void setup() {
  //Initialize Serial port
  Serial.begin(115200);
  Serial.println(F("ASB Sender Node started"));

  //Attach the previously defined CAN-Bus to our controller
  Serial.print(F("Attach CAN..."));
  if(asb0.busAttach(&asbCan0) < 0) {
    Serial.println(F("Error attaching CAN-Bus!"));
  } else {

    Serial.println(F("CAN-Bus attached"));
    Serial.println(F("Configure sleep..."));
  
    // Init Rs
    asbCan0.setTransceiverStandbyPin(true, MCP_RX0BF);

    // Config sleep + wakeup on new message:
    asbCan0.setSleepWakeup(false); // do not wake up on incoming messages, making this a "send only" node
    asbCan0.setAutoSleep(true, 0); // sleep immidiately after sending

    // Sending automated wakeup message to the receiving node(s):
    asbCan0.setSendWakeup(true);
  
    Serial.println(F("done!"));
  }
}

void loop() {
   
  //Send a message to group 0x124 instructing it to turn on/off
  byte data[2] = {ASB_CMD_1B, sendOn};
  byte sndStat = asb0.asbSend(ASB_PKGTYPE_MULTICAST, 0x124, sizeof(data), data);
  sndStat = asb0.asbSend(ASB_PKGTYPE_MULTICAST, 0x124, sizeof(data), data);
  if(sndStat == CAN_OK){
    Serial.println(F("Message sent successfully!"));
    // toggle the message
    sendOn = !sendOn;
  } else {
    Serial.println(F("Error sending message..."));
  }

  // Sleep for 4 seconds
  Serial.println("Sleeping 4 sec...");
  Serial.flush();
  sleepMCU();
}
