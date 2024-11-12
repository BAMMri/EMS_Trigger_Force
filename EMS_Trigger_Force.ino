/* Monostable multivibrator, non-retriggerable                            
 *                                                           
 * This sketch listens for a rising edge on a specific 
 * input pin and generates a square pulse on an output  
 * pin. It then enters a nonretriggerable state until   
 * the input stays constant (i.e. the interrupt is not  
 * triggered) for an amount of time specified by        
 * OFFPERIOD                                            
 * Copyright (c) 2015:                                      
 *  Francesco Santini <francesco.santini@unibas.ch>     
 *  Xeni Deligianni   <xeni.deligianni@unibas.ch>       */
 
/*  
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "HX711.h"

// Trigger defs
#define OUTPUTPIN 13
#define OUTPUTPIN2 11
#define RELAYPIN 12
#define RELAY_ENABLE LOW
#define INPUTPIN 2 // must be interrupt-enabled
#define PULSEDURATION 100
#define OFFPERIOD 200 // Run another pulse only if the input has been low for long enough

// Force sensor defs
#define DATA_PIN 7
#define CLK_PIN 6
#define SCALE 2457.3f

HX711 scale;

volatile bool runPulse = false;
long lastPulse = millis();
long triggerOnTime = -1;
bool outputEnable = true;
bool outputWillEnable = true;

void setOutput(int value)
{
  digitalWrite(OUTPUTPIN, value);
  #ifdef OUTPUTPIN2
    digitalWrite(OUTPUTPIN2, value);
  #endif
}

void setup() {
  pinMode(INPUTPIN,INPUT);
  attachInterrupt(digitalPinToInterrupt(INPUTPIN), pulseReceivedInterrupt, RISING);
  pinMode(OUTPUTPIN, OUTPUT);
//  digitalWrite(OUTPUTPIN, LOW);
  #ifdef OUTPUTPIN2
    pinMode(OUTPUTPIN2, OUTPUT);
//    digitalWrite(OUTPUTPIN2, LOW);
  #endif
  setOutput(LOW);
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, !RELAY_ENABLE);
  Serial.begin(9600); // slow serial comm

  Serial.println("Initializing the scale");
  // parameter "gain" is omitted; the default value 128 is used by the library (channel A)
  scale.begin(DATA_PIN, CLK_PIN);

  scale.set_scale(SCALE);
  scale.tare(20);  // set the tare
  
}

void loop() {

  if (triggerOnTime > 0 && (millis() - triggerOnTime >= PULSEDURATION))
  {
    /*
    digitalWrite(OUTPUTPIN, LOW);
    #ifdef OUTPUTPIN2
      digitalWrite(OUTPUTPIN2, LOW);
    #endif
    */
    setOutput(LOW);
    triggerOnTime = -1;
  }
 
  if (Serial.available() > 0)
  {
    // look for a command
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("ON"))
    {
      outputWillEnable = RELAY_ENABLE;
      Serial.println("Enabling");
    } else if (cmd.equalsIgnoreCase("OFF"))
    {
      outputWillEnable = !RELAY_ENABLE;
      Serial.println("Disabling");
    } else if (cmd.equalsIgnoreCase("RESET"))
    {
      Serial.println("Resetting the tare");
      scale.tare(20);
      Serial.println("Done");
    } else if (cmd.equalsIgnoreCase("TRIG"))
    {
      lastPulse = 0;
      runPulse = true;
      Serial.println("Forcing trigger");
    }
  }
  if (millis() - lastPulse >= OFFPERIOD)
  { // we are in a quiet time now, we can enable/disable output
     //Serial.println("QUIET");
     if (outputEnable != outputWillEnable)
     {
       
       outputEnable = outputWillEnable;
       digitalWrite(RELAYPIN, outputEnable);
       Serial.println(outputEnable);
     }
  } 
  if (runPulse)
  {
    // this means that an interrupt was called
    if (millis() - lastPulse >= OFFPERIOD)
    { 
     // run the pulse only if the last seen interrupt happened >= OFFPERIOD ms before.
     // Otherwise, don't run the pulse, but still record that an interrupt was triggered
     /*
      digitalWrite(OUTPUTPIN, HIGH);
      #ifdef OUTPUTPIN2
        digitalWrite(OUTPUTPIN2, HIGH);
      #endif
      */
      setOutput(HIGH);
      Serial.println("TRIG");
      // don't block for trigger pulse off
      /*delay(PULSEDURATION); 
      digitalWrite(OUTPUTPIN, LOW);*/
      triggerOnTime = millis();
    }
    lastPulse = millis();
    runPulse = false;
  }
  // only execute if scale is ready to read a value
  if (scale.is_ready())
  {
    Serial.print("Force: ");
    float val = scale.get_units(); 
    Serial.println(val, 1);
  }
}

void pulseReceivedInterrupt()
{
  
  if (!runPulse)
  {
    runPulse = true;
  }
}
