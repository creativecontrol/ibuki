#include <LinkedList.h>


#include <Wire.h>
#include <VL53L0X.h>

/*
 * Diavolo Laser Harp Transmitter
 * Teensy 3.0
 * XBee
 * TCA9548 i2c multiplexer
 * VL53L0X Time of Flight sensor (Pololu library)
 * 
 * Message format:
 * < platform number, sensor number, sensor value >
 * 
 * Attempt filtering again
 */

#define PLATFORM 1
//#define DEBUG

const int numSensors = 8;
const int debounce = 3;
int deadZone = 50;      // becasue of the value inversion this limits the height of the sensor
int cyclePause = 500;
int heartbeat = 10;
int heartcycle = 10;

VL53L0X sensors[numSensors];

LinkedList<int> sensorVals[numSensors];

#define LONG_RANGE

#define TCAADDR 0x70
 
void tcaselect(int i) {
  if (i > 7) return;
 
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();  
}

int dataLed = 13;
int statusLed = 13;
int errorLed = 13;

bool upFlag[numSensors];
bool downFlag[numSensors];
int previous[numSensors];

/*
 * Gives some visual feedback as to state of the process.
 * This uses the builtin LED for the Teensy.
 * Consider simplifying since all are using the same LED.
 */
void flashLed(int pin, int times, int wait) {
    
    for (int i = 0; i < times; i++) {
      digitalWrite(pin, HIGH);
      delay(wait);
      digitalWrite(pin, LOW);
      
      if (i + 1 < times) {
        delay(wait);
      }
    }
}


/*
 * Determine and return the max value of the list. 
 */
int listMax(LinkedList<int> &list){
  int _maxi = 0;

  for(int i = 0;i<list.size();i++){
    int itemValue = list.get(i);
    if(itemValue > _maxi){
      _maxi = itemValue;
    }
  }
  #if defined(DEBUG)
  Serial1.print("maxVal: ");
  Serial1.println(_maxi);
  #endif
  return _maxi;
}

/*
 * ----------------------------------------------------
 */
void setup() {
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);
  pinMode(dataLed,  OUTPUT);
  
  Serial1.begin(38400);
   #if defined(DEBUG)
  Serial.begin(9600);
  #endif
  Wire.begin();
  Wire.setClock(400000);
  
// Indicate startup.
  flashLed(statusLed, 2, 50);
  flashLed(dataLed, 4, 50);

// Wait a moment to initalize the Serial ports
  delay(1000);
  
// Initialize each of the sensors for long range use.
  for (int i=0; i<numSensors; i++){
    tcaselect(i);
    // set to long range mode
    sensors[i].init();
    sensors[i].setTimeout(50);
        
    #if defined LONG_RANGE
      // lower the return signal rate limit (default is 0.25 MCPS)
      sensors[i].setSignalRateLimit(0.1);
      // increase laser pulse periods (defaults are 14 and 10 PCLKs)
      sensors[i].setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
      sensors[i].setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
      sensors[i].setMeasurementTimingBudget(120000);
      sensors[i].startContinuous();
    #endif
     #if defined(DEBUG)
      Serial.print("setup sensor");
      Serial.println(i);
    #endif
//      Serial1.print("setup sensor");
//      Serial1.println(i);

      delay(200);
  }
  // set up the linkedlists to store sensor values
  for(int i = 0; i < numSensors; i++){
    for(int j = 0; j < debounce; j++){
      sensorVals[i].add(0);
    }
    upFlag[i] = true;
    downFlag[i] = false;
  }
}

void loop() {
  /*
   * Read each sensor. 
   * Remap it to MIDI values with some buffer.
   * Remapping allows for 10mm for each note area.
   * If it is a new value send it out.
   */

  for (int i=0; i<numSensors; i++){
    tcaselect(i);
    int sample = sensors[i].readRangeContinuousMillimeters();

    #if defined(DEBUG)
    Serial1.print("sample: ");
    Serial1.println(sample);
    #endif
    if(sample > 8190){
      sample = 8190;
    }
    sample = map(sample, 0, 8190, 140, 0);
    
    #if defined(DEBUG)
    Serial.print("sample: ");
    Serial.print(i);
    Serial.print(" ");
    Serial.println(sample);
    
    Serial1.print("after map: ");
    Serial1.println(sample);
    #endif
    
    
    //add the sample to the list
    sensorVals[i].add(sample);
    sensorVals[i].remove(0);
    #if defined(DEBUG)
    for(int i = 0; i < debounce; i++){
      Serial1.print(sensorVals[0].get(i));
      Serial1.print(',');
    }
    Serial1.println();
    #endif
    int outval = sample;

    if(sample != previous[i]){
      
      if(sample < deadZone){
        //outval = previous[i];
        int maxi = listMax(sensorVals[i]);
        
        if(maxi < deadZone){
          downFlag[i] = true;
          outval = 0;
          previous[i] = 0;
        }
      } else{
//        if(previous[i] < deadZone){
//          // if this sample is a trigger and the previous sample was a trigger, then send the MIDI note  
//          upFlag[i] = true;
//        }
//        
        previous[i] = sample;
      }
      sendMessage(i, outval);
    }   
   } 
    // assuming the flashLED is adding enough time
    #if defined(DEBUG)
   //delay(cyclePause);
   #endif
    
   heartbeat--; 
   //heartbeat - every 10 cycles
   if(heartbeat == 0){ 
     sendMessage(20, 127);
     heartbeat = heartcycle;
   }
   //delay(5);
}

/*
 * Send the message via XBee as a series of bytes separated by commas.
 */
void sendMessage(int sensor_num, int sensor_val){
  if(upFlag[sensor_num]){
    // Send packet
    Serial1.print("<");
    Serial1.print(PLATFORM);
    Serial1.print(",");
    Serial1.print(sensor_num);
    Serial1.print(",");
    Serial1.print(sensor_val);
    Serial1.println(">");
    flashLed(dataLed, 2, 10);
    upFlag[sensor_num] = false;
  } else if (downFlag[sensor_num]){
    // Send packet
    Serial1.print("<");
    Serial1.print(PLATFORM);
    Serial1.print(",");
    Serial1.print(sensor_num);
    Serial1.print(",");
    Serial1.print(sensor_val);
    Serial1.println(">");
    flashLed(dataLed, 2, 10);
    downFlag[sensor_num] = false;
    upFlag[sensor_num] = true;
  } else{
    // Send packet
    Serial1.print("<");
    Serial1.print(PLATFORM);
    Serial1.print(",");
    Serial1.print(sensor_num);
    Serial1.print(",");
    Serial1.print(sensor_val);
    Serial1.println(">");
    flashLed(dataLed, 2, 10);
  }
  
  Serial1.flush();
}
