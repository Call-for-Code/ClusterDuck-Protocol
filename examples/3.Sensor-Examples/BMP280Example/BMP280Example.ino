/**
 * @file mamaduck-send-message.ino
 * @brief Uses the built in Mama Duck with some customatizations.
 * 
 * This example is a Mama Duck, but it is also periodically sending a message in the Mesh
 * It is setup to provide a custom Emergency portal, instead of using the one provided by the SDK.
 * Notice the background color of the captive portal is Black instead of the default Red.
 * 
 * @date 2020-09-21
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "timer.h"
#include <MamaDuck.h>

#ifdef SERIAL_PORT_USBVIRTUAL
#define Serial SERIAL_PORT_USBVIRTUAL
#endif


//Setup BMP280
#include <Adafruit_BMP280.h>
Adafruit_BMP280 bmp;


// Set device ID between ""
MamaDuck duck = MamaDuck("DuckOne");

auto timer = timer_create_default();
const int INTERVAL_MS = 60000;
char message[32]; 
int counter = 1;

void setup() {
  // Use the default setup provided by the SDK
  duck.setupWithDefaults();
  Serial.println("MAMA-DUCK...READY!");

  //BMp setup
  if(!bmp.begin())
  {
    /* There was a problem detecting the BMP085 ... check your connections */
    Serial.print("Ooops, no BMP208 detected ... Check your wiring or I2C ADDR!");
    while(1); 
  }

  // initialize the timer. The timer thread runs separately from the main loop
  // and will trigger sending a counter message.
  timer.every(INTERVAL_MS, runSensor);
}

void loop() {
  timer.tick();
  // Use the default run(). The Mama duck is designed to also forward data it receives
  // from other ducks, across the network. It has a basic routing mechanism built-in
  // to prevent messages from hoping endlessly.
  duck.run();
}

bool runSensor(void *) {
  float T,P;
  
  T = bmp.readTemperature();
  Serial.println(T);
  P = bmp.readPressure();
  Serial.println(P);
  
  String sensorVal = "Temp: " + String(T) + " Pres: " + String(P); //Store Data

  duck.sendPayloadStandard(sensorVal, "BMP");
  
  return true;
}