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
#include <EEPROM.h>

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

#define TARE_DURATION 20

#define EEPROM_MAX_VALUES 10
#define EEPROM_VERSION 0xAB01
#define EEPROM_START 0
#define EEPROM_ADDRESS_CURRENT_SENSOR (EEPROM_START + sizeof(uint16_t))
#define EEPROM_ADDRESS_SCALES_START (EEPROM_ADDRESS_CURRENT_SENSOR + sizeof(unsigned char))
unsigned int eeprom_address_scale[EEPROM_MAX_VALUES];
unsigned char current_sensor = 0;
float current_scale = SCALE;

#define PARSE(CMD) parseCommand(cmd, CMD, &arg1, &arg2)

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


  // Setup EEPROM
  uint16_t eeprom_version = 0;
  // EEPROM_ADDRESSES

  for (unsigned int i=0; i < EEPROM_MAX_VALUES; i++)
  {
    eeprom_address_scale[i] = EEPROM_ADDRESS_SCALES_START + i*sizeof(float);
  }

  EEPROM.get(EEPROM_START, eeprom_version);
  if (eeprom_version != EEPROM_VERSION)
  {
    // EEPROM is not initialized
    EEPROM.put(EEPROM_START, (uint16_t)(EEPROM_VERSION));
    EEPROM.put(EEPROM_ADDRESS_CURRENT_SENSOR, (unsigned char)0);
    for (unsigned int i=0; i < EEPROM_MAX_VALUES; i++)
    {
      EEPROM.put(eeprom_address_scale[i], (float)(SCALE));
    }
  }

  EEPROM.get(EEPROM_ADDRESS_CURRENT_SENSOR, current_sensor);
  EEPROM.get(eeprom_address_scale[current_sensor], current_scale);



  Serial.println("Initializing the scale");
  // parameter "gain" is omitted; the default value 128 is used by the library (channel A)
  scale.begin(DATA_PIN, CLK_PIN);

  scale.set_scale(current_scale);
  scale.tare(TARE_DURATION);  // set the tare
  
}

bool parseCommand(String commandLine, const char *expectedCommand, int *arg1, float *arg2)
{
  const char *command = commandLine.c_str();
  *arg1 = -1;
  *arg2 = 0.0f;
  // check if this is the command we are looking for
  if (!commandLine.startsWith(expectedCommand))
    return false;

  // parse arguments
  char *currentTok = strtok(command, " ");
  currentTok = strtok(NULL, " "); // this should be the first argument
  if (!currentTok) return true;
  *arg1 = atoi(currentTok);
  currentTok = strtok(NULL, " "); // this should be the second argument
  if (!currentTok) return true;
  *arg2 = atof(currentTok);
}

void setSensor(int sensor)
{
  current_sensor = sensor;
  EEPROM.get(eeprom_address_scale[current_sensor], current_scale);
  scale.set_scale(current_scale);
}

void setScaleForSensor(int sensor, float scale)
{
  EEPROM.put(eeprom_address_scale[sensor], scale);
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
    int arg1;
    float arg2;
    // look for a command
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (PARSE("ON"))
    {
      outputWillEnable = RELAY_ENABLE;
      Serial.println("Enabling");
    } else if (PARSE("OFF"))
    {
      outputWillEnable = !RELAY_ENABLE;
      Serial.println("Disabling");
    } else if (PARSE("RESET"))
    {
      Serial.println("Resetting the tare");
      scale.tare(TARE_DURATION);
      Serial.println("Done");
    } else if (PARSE("TRIG"))
    {
      lastPulse = 0;
      runPulse = true;
      Serial.println("Forcing trigger");
      
    } else if (PARSE("GET_SCALE"))
    {
      Serial.print("Current scale: ");
      Serial.println(current_scale);
    } else if (PARSE("GET_SENSOR"))
    {
      Serial.print("Current sensor: ");
      Serial.println(current_sensor);
    } else if (PARSE("SET_SENSOR"))
    {
      if (arg1 < 0)
      {
        Serial.println("Malformed command");
      } else
      {
        setSensor(arg1);
        Serial.print("Sensor changed to: ");
        Serial.print(current_sensor);
        Serial.print(", scale: ");
        Serial.println(current_scale);
      }
    } else if (PARSE("SET_SCALE"))
    {
      if (arg1 < 0 || arg2 < 0)
      {
        Serial.println("Malformed command");
      } else
      {
        setScaleForSensor(arg1, arg2);
        Serial.print("Scale changed for sensor: ");
        Serial.print(arg1);
        Serial.print(" to: ");
        Serial.println(arg2);
      }
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
